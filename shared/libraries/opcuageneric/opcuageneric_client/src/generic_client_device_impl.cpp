#include <opcuageneric_client/generic_client_device_impl.h>

#include <opendaq/device_info_factory.h>
#include <opendaq/function_block_type_factory.h>
#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/property_helper.h>
#include "opcuashared/opcuaendpoint.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

std::atomic<int> OpcuaGenericClientDeviceImpl::localIndex = 0;

OpcuaGenericClientDeviceImpl::OpcuaGenericClientDeviceImpl(const ContextPtr& ctx,
                                                           const ComponentPtr& parent,
                                                           const PropertyObjectPtr& config,
                                                           std::shared_ptr<OpcUaClient> client,
                                                           const std::string& localId,
                                                           const std::string& name,
                                                           uint32_t reconnectIntervalMs)
    : Device(ctx, parent, localId.empty() ? generateLocalId() : localId)
    , connectionStatus("ConnectionStatusType", "ConnectionStatus", statusContainer, "Connected", context.getTypeManager())
    , client(client)
    , reconnectIntervalMs(reconnectIntervalMs)
{
    if (this->client == nullptr)
        DAQ_THROW_EXCEPTION(UninitializedException, "OpcUaClient is not initialized");

    this->name = name.empty() ? GENERIC_OPCUA_CLIENT_DEVICE_NAME : name;

    if (config.assigned())
        initProperties(property_helper::populateDefaultConfig(createDefaultConfig(), config));
    else
        initProperties(createDefaultConfig());

    initComponentStatus();

    try
    {
        this->client->connect();
        this->client->runIterate();
    }
    catch (const OpcUaException& e)
    {
        switch (e.getStatusCode())
        {
            case UA_STATUSCODE_BADUSERACCESSDENIED:
            case UA_STATUSCODE_BADIDENTITYTOKENINVALID:
                DAQ_THROW_EXCEPTION(AuthenticationFailedException, e.what());
            default:
                DAQ_THROW_EXCEPTION(GeneralErrorException, e.what());
        }
    }

    initNestedFbTypes();
    startReconnectMonitor();
}

OpcuaGenericClientDeviceImpl::~OpcuaGenericClientDeviceImpl()
{
    stopReconnectMonitor();
}

PropertyObjectPtr OpcuaGenericClientDeviceImpl::createDefaultConfig()
{
    auto defaultConfig = PropertyObject();

    {
        auto builder = SelectionPropertyBuilder(PROPERTY_NAME_OPCUA_TS_MODE,
                                                List<IString>("None", "ServerTimestamp", "SourceTimestamp", "LocalSystemTimestamp"),
                                                static_cast<int>(DomainSource::SourceTimestamp))
                           .setDescription("Defines what to use as a domain signal. By default it is set to SourceTimestamp.");
        defaultConfig.addProperty(builder.build());
    }

    return defaultConfig;
}

void OpcuaGenericClientDeviceImpl::initProperties(const PropertyObjectPtr& config)
{
    const auto defaultConfig = createDefaultConfig();
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (defaultConfig.hasProperty(propName))
        {
            if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true); internalProp.assigned())
            {
                objPtr.addProperty(internalProp.clone());
                objPtr.setPropertyValue(propName, prop.getValue());
                objPtr.getOnPropertyValueWrite(prop.getName()) +=
                    [this](PropertyObjectPtr&, PropertyValueEventArgsPtr&) { propertyChanged(); };
            }
        }
    }
    readProperties();
}

void OpcuaGenericClientDeviceImpl::readProperties()
{
    using namespace property_helper;
    auto lock = this->getRecursiveConfigLock();
    using DS = DomainSource;
    const auto tmpDomainSource =
        readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DS::SourceTimestamp));
    if (tmpDomainSource < static_cast<int>(DS::_count) && tmpDomainSource >= 0)
    {
        domainSource = static_cast<DS>(tmpDomainSource);
    }
    else
    {
        domainSource = DS::ServerTimestamp;
    }
}

void OpcuaGenericClientDeviceImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);
    readProperties();

    for (const auto& fb : nestedFunctionBlocks)
    {
        if (fb.assigned())
        {
            auto monitoredItemFb = reinterpret_cast<OpcUaMonitoredItemFbImpl*>(*fb);
            monitoredItemFb->setDomainSource(domainSource);
        }
    }
}

std::string OpcuaGenericClientDeviceImpl::getConnectionString() const
{
    return client->getEndpoint().getUrl();
}

void OpcuaGenericClientDeviceImpl::removed()
{
    stopReconnectMonitor();
    Device::removed();
    client->disconnect(false);
}

void OpcuaGenericClientDeviceImpl::startReconnectMonitor()
{
    reconnectRunning = true;
    reconnectThread = std::thread([this] { reconnectMonitorLoop(); });
}

void OpcuaGenericClientDeviceImpl::stopReconnectMonitor()
{
    {
        std::lock_guard<std::mutex> lock(reconnectMutex);
        reconnectRunning = false;
    }
    reconnectCv.notify_all();
    if (reconnectThread.joinable())
        reconnectThread.join();
}

void OpcuaGenericClientDeviceImpl::reconnectMonitorLoop()
{
    auto interruptibleSleep = [&]()
    {
        std::unique_lock<std::mutex> lock(reconnectMutex);
        reconnectCv.wait_for(lock, std::chrono::milliseconds(reconnectIntervalMs), [this]() { return !reconnectRunning.load(); });
    };

    while (reconnectRunning)
    {
        if (client->isConnected() == false)
        {
            connectionStatus.setStatus("Reconnecting");
            try
            {
                client->disconnect(false);
                client->connect();
                client->runIterate();
                connectionStatus.setStatus("Connected");
            }
            catch (const OpcUaException& e)
            {
                if (e.getStatusCode() == UA_STATUSCODE_BADUSERACCESSDENIED ||
                    e.getStatusCode() == UA_STATUSCODE_BADIDENTITYTOKENINVALID)
                {
                    connectionStatus.setStatus("Unrecoverable");
                    reconnectRunning = false;
                }
            }
        }
        interruptibleSleep();
    }
}

DeviceInfoPtr OpcuaGenericClientDeviceImpl::onGetInfo()
{
    return DeviceInfo(getConnectionString(), GENERIC_OPCUA_CLIENT_DEVICE_NAME);
}


void OpcuaGenericClientDeviceImpl::initNestedFbTypes()
{
    nestedFbTypes = Dict<IString, IFunctionBlockType>();
    // Add a function block type for monitoring an OPCUA node
    {
        const auto fbType = OpcUaMonitoredItemFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }
}


DictPtr<IString, IFunctionBlockType> OpcuaGenericClientDeviceImpl::onGetAvailableFunctionBlockTypes()
{
    return nestedFbTypes;
}

FunctionBlockPtr OpcuaGenericClientDeviceImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    FunctionBlockPtr nestedFunctionBlock;
    {
        if (nestedFbTypes.hasKey(typeId))
        {
            auto fbTypePtr = nestedFbTypes.getOrDefault(typeId);
            if (fbTypePtr.getName() == GENERIC_OPCUA_MONITORED_ITEM_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, OpcUaMonitoredItemFbImpl>(
                    context, functionBlocks, fbTypePtr, client, domainSource, config);
            }
            else
            {
                setComponentStatusWithMessage(ComponentStatus::Error, "Function block type is not available: " + typeId.toStdString());
                return nestedFunctionBlock;
            }
        }
        if (nestedFunctionBlock.assigned())
        {
            addNestedFunctionBlock(nestedFunctionBlock);
            setComponentStatus(ComponentStatus::Ok);

            {
                auto lockProcessing = std::scoped_lock(processingMutex);
                nestedFunctionBlocks.push_back(nestedFunctionBlock);
            }
        }
        else
        {
            DAQ_THROW_EXCEPTION(NotFoundException, "Function block type is not available: " + typeId.toStdString());
        }
    }
    return nestedFunctionBlock;
}

void OpcuaGenericClientDeviceImpl::onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock)
{
    {
        auto lockProcessing = std::scoped_lock(processingMutex);
        auto it = std::find_if(nestedFunctionBlocks.begin(),
                               nestedFunctionBlocks.end(),
                               [&functionBlock](const FunctionBlockPtr& fb) { return fb.getObject() == functionBlock.getObject(); });

        if (it != nestedFunctionBlocks.end())
        {
            nestedFunctionBlocks.erase(it);
        }
    }
    auto lock = this->getRecursiveConfigLock2();
    GenericDevice::onRemoveFunctionBlock(functionBlock);
}
std::string OpcuaGenericClientDeviceImpl::generateLocalId()
{
    return std::string(GENERIC_OPCUA_CLIENT_DEVICE_NAME + std::to_string(localIndex++));
}
END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
