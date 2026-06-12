#include <algorithm>
#include <coreobjects/property_factory.h>
#include <opcua_simple_server/simple_server.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/device_info_internal_ptr.h>
#include <opendaq/packet.h>
#include <opendaq/reader_factory.h>
#include <opendaq/custom_log.h>

#define PROTOCOL_ID "OpenDAQOPCUASimple"
#define PROTOCOL_NAME "OpenDAQOPCUASimple"
#define LOGGER_COMPONENT_NAME "SimpleOPCUAServer"
#define PROTOCOL_PREFIX "daq.opcua.simple"
#define PROTOCOL_TYPE ProtocolType::Unknown

//using namespace daq::opcua;

BEGIN_NAMESPACE_OPENDAQ_OPCUA

GenericServer::~GenericServer()
{
    stop();
}

GenericServer::GenericServer(const InstancePtr& instance)
    : GenericServer(instance.getRootDevice(), instance.getContext())
{
}

GenericServer::GenericServer(const DevicePtr& device, const ContextPtr& context)
    : device(device)
    , context(context)
    , readingIntervalMs(100)
{
}

void GenericServer::setOpcUaPort(uint16_t port)
{
    this->opcUaPort = port;
}

void GenericServer::setOpcUaPath(const std::string& path)
{
    this->opcUaPath = path;
}

void GenericServer::start()
{
    if (!device.assigned())
        DAQ_THROW_EXCEPTION(InvalidStateException, "Device is not set.");
    if (!context.assigned())
        DAQ_THROW_EXCEPTION(InvalidStateException, "Context is not set.");

    auto info = device.getInfo();

    server = std::make_shared<OpcUaServer>(false);
    server->setPort(opcUaPort);
    server->setAuthenticationProvider(context.getAuthenticationProvider());
    server->setClientConnectedHandler(
        [this](const std::string& clientId)
        {
            const auto loggerComponent = context.getLogger().getOrAddComponent(LOGGER_COMPONENT_NAME);
            LOG_I("New client connected, ID: {}", clientId);
            SizeT clientNumber = 0;
            if (device.assigned() && !device.isRemoved())
            {
                device.getInfo().asPtr<IDeviceInfoInternal>(true).addConnectedClient(
                    &clientNumber,
                    ConnectedClientInfo("", PROTOCOL_TYPE, PROTOCOL_NAME, "", ""));
            }
            registeredClientIds.insert({clientId, clientNumber});
        }
    );
    server->setClientDisconnectedHandler(
        [this](const std::string& clientId)
        {
            if (auto it = registeredClientIds.find(clientId); it != registeredClientIds.end())
            {
                const auto loggerComponent = context.getLogger().getOrAddComponent(LOGGER_COMPONENT_NAME);
                LOG_I("Client disconnected, ID: {}", clientId);
                if (device.assigned() && !device.isRemoved() && it->second != 0)
                {
                    device.getInfo().asPtr<IDeviceInfoInternal>(true).removeConnectedClient(it->second);
                }
                registeredClientIds.erase(it);
            }
        }
    );
    // TODO : set proper callbacks for access control
    server->setAllowBrowsingNodeCallback([](UA_Server* server,
                                            UA_AccessControl* ac,
                                            const UA_NodeId* sessionId,
                                            void* sessionContext,
                                            const UA_NodeId* nodeId,
                                            void* nodeContext) { return true; });
    server->setGetUserAccessLevelCallback([](UA_Server* server,
                                             UA_AccessControl* ac,
                                             const UA_NodeId* sessionId,
                                             void* sessionContext,
                                             const UA_NodeId* nodeId,
                                             void* nodeContext) { return UA_Byte(UA_ACCESSLEVELMASK_READ); });
    server->setGetUserRightsMaskCallback([](UA_Server* server,
                                            UA_AccessControl* ac,
                                            const UA_NodeId* sessionId,
                                            void* sessionContext,
                                            const UA_NodeId* nodeId,
                                            void* nodeContext) { return UA_UInt32(0); });
    server->setGetUserExecutableCallback([](UA_Server* server,
                                            UA_AccessControl* ac,
                                            const UA_NodeId* sessionId,
                                            void* sessionContext,
                                            const UA_NodeId* nodeId,
                                            void* nodeContext) { return false; });
    server->prepare();

    createDeviceNode();
    fillDeviceNode();
    addSignalNodes();

    auto serverCapability = ServerCapability(PROTOCOL_ID, PROTOCOL_NAME, PROTOCOL_TYPE);
    serverCapability.setPrefix(PROTOCOL_PREFIX);
    serverCapability.setConnectionType("TCP/IP");
    serverCapability.setPort(opcUaPort);
    serverCapability.addProperty(StringProperty("Path", opcUaPath == "/" ? "" : opcUaPath));
    info.asPtr<IDeviceInfoInternal>(true).addServerCapability(serverCapability);

    server->start();

    startReadingThread();
}

void GenericServer::stop()
{
    if (device.assigned() && !device.isRemoved())
    {
        const auto info = device.getInfo();
        const auto infoInternal = info.asPtr<IDeviceInfoInternal>();
        if (info.hasServerCapability(PROTOCOL_ID))
            infoInternal.removeServerCapability(PROTOCOL_ID);
        for (const auto& [_, clientNumber] : registeredClientIds)
        {
            if (clientNumber != 0)
                infoInternal.removeConnectedClient(clientNumber);
        }
    }
    registeredClientIds.clear();
    stopReadingThread();

    if (server)
        server->stop();

    server.reset();
    signalNodes.clear();
}

void GenericServer::createDeviceNode()
{
    OpcUaNodeId rootDeviceNodeId(namespaceIndex, device.getGlobalId().toStdString());

    AddObjectNodeParams params(rootDeviceNodeId, OpcUaNodeId(UA_NS0ID_OBJECTSFOLDER));

    auto browseName = device.getGlobalId().toStdString();
    std::replace(browseName.begin(), browseName.end(), '/', '-');
    params.setBrowseName(browseName);
    params.attr->displayName = UA_LOCALIZEDTEXT_ALLOC("en_US", device.getName().toStdString().c_str());
    params.attr->description = UA_LOCALIZEDTEXT_ALLOC("en_US", device.getDescription().toStdString().c_str());
    params.attr->writeMask = UA_ATTRIBUTEWRITEMASK_NONE;
    params.attr->userWriteMask = UA_ATTRIBUTEWRITEMASK_NONE;
    params.attr->eventNotifier = UA_EVENTNOTIFIER_SUBSCRIBE_TO_EVENT;

    params.referenceTypeId = OpcUaNodeId(UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES));
    params.typeDefinition = OpcUaNodeId(UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE));
    params.nodeContext = this;
    this->rootDeviceNodeId = server->addObjectNode(params);
}

void GenericServer::fillDeviceNode()
{
    // TODO
}

void GenericServer::addSignalNodes()
{
    const auto sigList = device.getSignalsRecursive(search::Any());
    for (const auto& sig : sigList)
    {
        auto config = simple_objects::SignalNode::createDefaultConfig();
        config.setPropertyValue("BrowseName", "");
        try {
            simple_objects::SignalNode node(server, rootDeviceNodeId, sig, config);
            signalNodes.push_back(std::move(node));
        } catch (const DaqException& e) {
            const auto loggerComponent = context.getLogger().getOrAddComponent(LOGGER_COMPONENT_NAME);
            LOG_E("Failed to create signal node for signal '{}', error: {}", sig.getName(), e.what());
        }

    }
}

void GenericServer::startReadingThread()
{
    readingRunning = true;
    readingThread = std::thread([this] { readingLoop(); });
}

void GenericServer::stopReadingThread()
{
    {
        std::lock_guard<std::mutex> lock(readingMutex);
        readingRunning = false;
    }
    readingCv.notify_all();
    if (readingThread.joinable())
        readingThread.join();
}

void GenericServer::readingLoop()
    {
        auto interruptibleSleep = [&](std::chrono::steady_clock::time_point nextTimePoint)
        {
            std::unique_lock<std::mutex> lock(readingMutex);
            readingCv.wait_until(lock, nextTimePoint, [this]() { return !readingRunning.load(); });
        };

        auto prewPoint = std::chrono::steady_clock::now();
        while (readingRunning)
        {
            const auto nextTimePoint = prewPoint + std::chrono::milliseconds(readingIntervalMs);
            for (auto& signalNode : signalNodes)
            {
                signalNode.process();
            }
            prewPoint = nextTimePoint;
            interruptibleSleep(nextTimePoint);
        }
    }

END_NAMESPACE_OPENDAQ_OPCUA
