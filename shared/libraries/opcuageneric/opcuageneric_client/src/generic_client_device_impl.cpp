#include <opcuageneric_client/generic_client_device_impl.h>

#include <opendaq/device_info_factory.h>
#include <opendaq/function_block_type_factory.h>
#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/property_helper.h>
#include "opcuashared/opcuaendpoint.h"
#include <limits>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

std::atomic<int> OpcuaGenericClientDeviceImpl::localIndex = 0;

OpcuaGenericClientDeviceImpl::OpcuaGenericClientDeviceImpl(const ContextPtr& ctx,
                                                           const ComponentPtr& parent,
                                                           std::shared_ptr<OpcUaClient> client,
                                                           const std::string& localId,
                                                           const std::string& name,
                                                           uint32_t reconnectIntervalMs)
    : Device(ctx, parent, localId.empty() ? generateLocalId() : localId)
    , connectionStatus("ConnectionStatusType", "ConnectionStatus", statusContainer, "Connected", context.getTypeManager())
    , client(client)
    , defaultSamplingIntervalMs(DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL)
    , sampler([this] { return this->client->isConnected(); })
    , reconnectIntervalMs(reconnectIntervalMs)
{
    if (this->client == nullptr)
        DAQ_THROW_EXCEPTION(UninitializedException, "OpcUaClient is not initialized");

    this->name = name.empty() ? GENERIC_OPCUA_CLIENT_DEVICE_NAME : name;

    initProperties();
    initComponentStatus();

    initNestedFbTypes();
    sampler.start();
    startReconnectMonitor();
}

OpcuaGenericClientDeviceImpl::~OpcuaGenericClientDeviceImpl()
{
    stopReconnectMonitor();
    sampler.stop();
}

void OpcuaGenericClientDeviceImpl::initProperties()
{
    {
        auto builder = SelectionPropertyBuilder(PROPERTY_NAME_OPCUA_DEFAULT_TS_MODE,
                                                List<IString>("None", "ServerTimestamp", "SourceTimestamp", "LocalSystemTimestamp"),
                                                static_cast<int>(DomainSource::SourceTimestamp))
                           .setDescription(fmt::format("Default \"{}\" for newly added monitored items. By default it is set to "
                                                       "SourceTimestamp.",
                                                       PROPERTY_NAME_OPCUA_TS_MODE));
        objPtr.addProperty(builder.build());
        objPtr.getOnPropertyValueWrite(PROPERTY_NAME_OPCUA_DEFAULT_TS_MODE) += [this](PropertyObjectPtr&, PropertyValueEventArgsPtr&) { propertyChanged(); };
    }

    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL, Integer(DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL))
                .setDescription(fmt::format("Default sampling interval in milliseconds for newly added monitored items. By default it is "
                                            "set to {} ms.",
                                            DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL));
        objPtr.addProperty(builder.build());
        objPtr.getOnPropertyValueWrite(PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL) += [this](PropertyObjectPtr&, PropertyValueEventArgsPtr&) { propertyChanged(); };
    }

    readProperties();
}

void OpcuaGenericClientDeviceImpl::readProperties()
{
    using namespace property_helper;
    auto lock = this->getRecursiveConfigLock();
    using DS = DomainSource;
    const auto tmpDomainSource =
        readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_DEFAULT_TS_MODE, static_cast<int>(DS::SourceTimestamp));
    if (tmpDomainSource < static_cast<int>(DS::_count) && tmpDomainSource >= 0)
    {
        defaultDomainSource = static_cast<DS>(tmpDomainSource);
    }
    else
    {
        defaultDomainSource = DS::ServerTimestamp;
    }

    const auto samplingInterval =
        readProperty<Int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL, DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL);
    if (samplingInterval <= 0 || samplingInterval > static_cast<Int>(std::numeric_limits<uint32_t>::max()))
    {
        LOG_W("Invalid value {} for the \"{}\" property! Sampling interval must be a positive integer. Using {} ms instead.",
              samplingInterval,
              PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL,
              DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL);
        defaultSamplingIntervalMs = DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL;
    }
    else
    {
        defaultSamplingIntervalMs = static_cast<uint32_t>(samplingInterval);
    }
}

void OpcuaGenericClientDeviceImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock2();
    readProperties();
}

std::string OpcuaGenericClientDeviceImpl::getConnectionString() const
{
    return client->getEndpoint().getUrl();
}

void OpcuaGenericClientDeviceImpl::removed()
{
    stopReconnectMonitor();
    // Stopped before the function blocks are torn down, so that no tick can reach a dying item.
    sampler.stop();
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
                sampler.onReconnected();
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
    auto lock = this->getRecursiveConfigLock2();
    return nestedFbTypes;
}

FunctionBlockPtr OpcuaGenericClientDeviceImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    FunctionBlockPtr nestedFunctionBlock;
    {
        const auto fbTypes = onGetAvailableFunctionBlockTypes();
        if (fbTypes.hasKey(typeId))
        {
            auto fbTypePtr = fbTypes.getOrDefault(typeId);
            if (fbTypePtr.getName() == GENERIC_OPCUA_MONITORED_ITEM_FB_NAME)
            {
                std::string userSpecifiedLocalId;
                if (config.assigned() && config.hasProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID))
                    userSpecifiedLocalId = config.getPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID).asPtr<IString>().toStdString();
                const auto localId = buildMILocalId(userSpecifiedLocalId);
                // The new function block starts with the device's current defaults for these.
                DomainSource initialDomainSource;
                uint32_t initialSamplingIntervalMs;
                {
                    auto lock = this->getRecursiveConfigLock2();
                    initialDomainSource = defaultDomainSource;
                    initialSamplingIntervalMs = defaultSamplingIntervalMs;
                }
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, OpcUaMonitoredItemFbImpl>(
                    context, functionBlocks, fbTypePtr, client, localId, initialDomainSource, initialSamplingIntervalMs, &sampler, config);
            }
            else
            {
                setComponentStatusWithMessage(ComponentStatus::Error, "Function block type is not available: " + typeId.toStdString());
                return nestedFunctionBlock;
            }
        }
        if (nestedFunctionBlock.assigned())
        {
            {
                auto lock = this->getRecursiveConfigLock2();
                addNestedFunctionBlock(nestedFunctionBlock);
            }
            sampler.registerItem(static_cast<OpcUaMonitoredItemFbImpl*>(*nestedFunctionBlock));
            setComponentStatus(ComponentStatus::Ok);
        }
        else
        {
            DAQ_THROW_EXCEPTION(NotFoundException, "Function block type is not available: " + typeId.toStdString());
        }
    }
    return nestedFunctionBlock;
}

std::string OpcuaGenericClientDeviceImpl::generateLocalId()
{
    return std::string(GENERIC_OPCUA_CLIENT_DEVICE_NAME + std::to_string(localIndex++));
}

std::string OpcuaGenericClientDeviceImpl::buildMILocalId(const std::string& userProvided)
{
    {
        if (userProvided.empty())
        {
            LOG_I("User did not provide local ID for the device. Generating a unique local ID for "
                  "the new function block.");
            return "";
        }
    }
    {
        daq::GenericComponentPtr<daq::IComponent> fbs;
        checkErrorInfo(getItem(String("FB"), &fbs));
        if (fbs.asPtr<IFolder>().hasItem(userProvided))
        {
            LOG_W("Function block with local ID {} already exists under the parent folder. Generating a unique local ID for "
                  "the new function block.",
                  userProvided);
            return "";
        }
        else
        {
            return userProvided;
        }
    }
    return "";
}
END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
