#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include "opendaq/binary_data_packet_factory.h"
#include "opendaq/packet_factory.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

std::atomic<int> OpcUaMonitoredItemFbImpl::localIndex = 0;

std::unordered_map<OpcUaNodeId, daq::SampleType> OpcUaMonitoredItemFbImpl::supportedDataTypes = {
    {OpcUaNodeId(), daq::SampleType::Undefined},
    {OpcUaNodeId(0, UA_NS0ID_FLOAT), daq::SampleType::Float32},
    {OpcUaNodeId(0, UA_NS0ID_DOUBLE), daq::SampleType::Float64},
    {OpcUaNodeId(0, UA_NS0ID_SBYTE), daq::SampleType::Int8},
    {OpcUaNodeId(0, UA_NS0ID_BYTE), daq::SampleType::UInt8},
    {OpcUaNodeId(0, UA_NS0ID_INT16), daq::SampleType::Int16},
    {OpcUaNodeId(0, UA_NS0ID_UINT16), daq::SampleType::UInt16},
    {OpcUaNodeId(0, UA_NS0ID_INT32), daq::SampleType::Int32},
    {OpcUaNodeId(0, UA_NS0ID_UINT32), daq::SampleType::UInt32},
    {OpcUaNodeId(0, UA_NS0ID_INT64), daq::SampleType::Int64},
    {OpcUaNodeId(0, UA_NS0ID_UINT64), daq::SampleType::UInt64},
    {OpcUaNodeId(0, UA_NS0ID_STRING), daq::SampleType::String}};

namespace
{
    PropertyObjectPtr populateDefaultConfig(const PropertyObjectPtr& defaultConfig, const PropertyObjectPtr& config)
    {
        auto newConfig = PropertyObject();
        for (const auto& prop : defaultConfig.getAllProperties())
        {
            newConfig.addProperty(prop.asPtr<IPropertyInternal>(true).clone());
            const auto propName = prop.getName();
            newConfig.setPropertyValue(propName, config.hasProperty(propName) ? config.getPropertyValue(propName) : prop.getValue());
        }
        return newConfig;
    }

    template <typename retT, typename intfT>
    retT readProperty(const PropertyObjectPtr objPtr, const std::string& propertyName, const retT defaultValue)
    {
        retT returnValue{defaultValue};
        if (objPtr.hasProperty(propertyName))
        {
            auto property = objPtr.getPropertyValue(propertyName).asPtrOrNull<intfT>();
            if (property.assigned())
            {
                returnValue = property.getValue(defaultValue);
            }
        }
        return returnValue;
    }
}

OpcUaMonitoredItemFbImpl::OpcUaMonitoredItemFbImpl(const ContextPtr& ctx,
                                                   const ComponentPtr& parent,
                                                   const FunctionBlockTypePtr& type,
                                                   daq::opcua::OpcUaClientPtr client,
                                                   const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, generateLocalId())
    , client(client)
    , nodeId()
    , running(false)
{
    initComponentStatus();
    initStatusContainer();
    if (config.assigned())
        initProperties(populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    nodeId = OpcUaNodeId{static_cast<uint16_t>(this->config.namespaceIndex), this->config.nodeId};

    validateNode();
    adjustSignalDescriptor();
    createSignal();
    updateStatuses();
    runReaderThread();
}

OpcUaMonitoredItemFbImpl::~OpcUaMonitoredItemFbImpl()
{
    if (readerThread.joinable())
    {
        running = false;
        readerThread.join();
    }
}

void OpcUaMonitoredItemFbImpl::removed()
{
    if (readerThread.joinable())
    {
        running = false;
        readerThread.join();
    }
    FunctionBlock::removed();
}

void OpcUaMonitoredItemFbImpl::initStatusContainer()
{
    statuses.addStatus("config");
    statuses.addStatus("nodeValidation");
    statuses.addStatus("valueValidation");
    statuses.addStatus("responseValidation");
    statuses.addStatus("exeption");
}

FunctionBlockTypePtr OpcUaMonitoredItemFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    {
        auto builder =
            SelectionPropertyBuilder(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, List<IString>("String"), static_cast<int>(NodeIDType::String))
                .setDescription("Defines the type of the NodeID of the OPCUA node to monitor. By default it is set to String. Other "
                                "formats are not supported at the moment.");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_OPCUA_NODE_ID, String(""))
                .setDescription(fmt::format("Specifies the NodeID of the OPCUA node to monitor. The format of the NodeID should correspond "
                                            "to the type specified in the \"{}\" property.",
                                            PROPERTY_NAME_OPCUA_NODE_ID_TYPE));
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder = IntPropertyBuilder(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, Integer(0))
                           .setDescription("Specifies the namespace index of the OPCUA node to monitor. This property is optional and can "
                                           "be left empty. If not set, the first occurence of NodeID will be used");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, Integer(DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL))
                .setDescription(fmt::format(
                    "Specifies the sampling interval in milliseconds for monitoring the OPCUA node. By default it is set to {} ms.",
                    DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL));
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder = SelectionPropertyBuilder(PROPERTY_NAME_OPCUA_TS_MODE,
                                                List<IString>("None", "ServerTimestamp", "SourceTimestamp"),
                                                static_cast<int>(DomainSource::ServerTimestamp))
                           .setDescription("Defines what to use as a domain signal. By default it is set to ServerTimestamp.");
        defaultConfig.addProperty(builder.build());
    }

    const auto fbType = FunctionBlockType(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME,
                                          GENERIC_OPCUA_MONITORED_ITEM_FB_NAME,
                                          "Monitors a specified OPCUA node and outputs the value and timestamp as signals.",
                                          defaultConfig);
    return fbType;
}

std::string OpcUaMonitoredItemFbImpl::generateLocalId()
{
    return std::string(OPCUA_LOCAL_MONITORED_ITEM_FB_ID_PREFIX + std::to_string(localIndex++));
}

void OpcUaMonitoredItemFbImpl::adjustSignalDescriptor()
{
    if (statuses.ok("nodeValidation") && supportedDataTypes.count(nodeDataType) != 0)
    {
        outputSignalDescriptor = DataDescriptorBuilder().setSampleType(supportedDataTypes[nodeDataType]).build();
    }
    else
    {
        outputSignalDescriptor = DataDescriptorBuilder().setSampleType(daq::SampleType::Undefined).build();
    }
}

void OpcUaMonitoredItemFbImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (!objPtr.hasProperty(propName))
        {
            if (const auto internalProp = prop.asPtrOrNull<IPropertyInternal>(true); internalProp.assigned())
            {
                objPtr.addProperty(internalProp.clone());
                objPtr.setPropertyValue(propName, prop.getValue());
                objPtr.getOnPropertyValueWrite(prop.getName()) +=
                    [this](PropertyObjectPtr&, PropertyValueEventArgsPtr&) { propertyChanged(); };
            }
        }
        else
        {
            objPtr.setPropertyValue(propName, prop.getValue());
        }
    }
    readProperties();
}

void OpcUaMonitoredItemFbImpl::readProperties()
{
    auto lock = this->getRecursiveConfigLock();
    statuses.reset("config");


    config.nodeIdType = NodeIDType::String;  // only string NodeIDs are supported at the moment
    config.nodeId = readProperty<std::string, IString>(objPtr, PROPERTY_NAME_OPCUA_NODE_ID, "");
    if (config.nodeId.empty())
    {
        statuses.addToStatus("config", fmt::format("\"{}\" property is empty!", PROPERTY_NAME_OPCUA_NODE_ID));
    }

    config.namespaceIndex = readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 0);
    config.samplingInterval =
        readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL);
    if (config.samplingInterval <= 0)
    {
        statuses.addToStatus("config", fmt::format("Invalid value for the \"{}\" property! Sampling interval must be a positive integer.",
                                                   PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL));
        config.samplingInterval = DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL;
    }

    const auto tmpDomainSource =
        readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DomainSource::ServerTimestamp));
    if (tmpDomainSource < static_cast<int>(DomainSource::_count) && tmpDomainSource >= 0)
    {
        config.domainSource = static_cast<DomainSource>(tmpDomainSource);
    }
    else
    {
        config.domainSource = DomainSource::ServerTimestamp;
    }

    updateStatuses();
}

void OpcUaMonitoredItemFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto prevConfig = config;
    readProperties();

    nodeId = OpcUaNodeId{static_cast<uint16_t>(this->config.namespaceIndex), this->config.nodeId};

    statuses.resetAll();
    validateNode();
    adjustSignalDescriptor();
    reconfigureSignal(prevConfig);
    updateStatuses();
}

void OpcUaMonitoredItemFbImpl::updateStatuses()
{
    if (!statuses.isUpdated())
        return;

    if (!statuses.ok("config"))
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Configuration is invalid! " + statuses.getMessage("config"));
    }
    else if (!statuses.ok("nodeValidation"))
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Node is invalid! " + statuses.getMessage("nodeValidation"));
    }
    else if (!statuses.ok("responseValidation"))
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Response error! " + statuses.getMessage("responseValidation"));
    }
    else if (!statuses.ok("valueValidation"))
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Value error! " + statuses.getMessage("valueValidation"));
    }
    else if (!statuses.ok("exeption"))
    {
        setComponentStatusWithMessage(ComponentStatus::Error, statuses.getMessage("exeption"));
    }
    else
    {
        setComponentStatus(ComponentStatus::Ok);
    }
}

void OpcUaMonitoredItemFbImpl::validateNode()
{
    try
    {
        statuses.reset("nodeValidation");
        auto nodeExist = client->nodeExists(nodeId);
        if (!nodeExist)
        {
            statuses.addToStatus("nodeValidation", fmt::format("Node {} does not exist", nodeId.toString()));
        }
        else if (const auto nodeClass = client->readNodeClass(nodeId); nodeClass != UA_NodeClass::UA_NODECLASS_VARIABLE)
        {
            statuses.addToStatus("nodeValidation", fmt::format("Node {} is not a variable node", nodeId.toString()));
        }
        else if (const auto accessLevel = client->readAccessLevel(nodeId); (accessLevel & UA_ACCESSLEVELMASK_READ) == 0)
        {
            statuses.addToStatus("nodeValidation", fmt::format("There is no read permission to node {}", nodeId.toString()));
        }
        else if (nodeDataType = client->readDataType(nodeId); supportedDataTypes.count(nodeDataType) == 0)
        {
            statuses.addToStatus("nodeValidation", fmt::format("Node {} has unsupported DataType ({})", nodeId.toString(), nodeDataType.toString()));
        }
    }
    catch (OpcUaException& ex)
    {
        if (ex.getStatusCode() == UA_STATUSCODE_BADUSERACCESSDENIED)
        {
            statuses.addToStatus("nodeValidation", fmt::format("Access denied for node {}", nodeId.toString()));
        }
        else
        {
            statuses.addToStatus("nodeValidation", fmt::format("Exception was thrown while node {} validatiion", nodeId.toString()));
        }
    }
}

bool OpcUaMonitoredItemFbImpl::validateResponse(const OpcUaDataValue& value)
{
    if (value.getValue().hasStatus && value.getValue().hasStatus != UA_STATUSCODE_GOOD)
    {
        statuses.addToStatus("responseValidation", fmt::format("Reading value error: {}. ", value.getValue().hasStatus));
        return false;
    }
    if (!value.getValue().hasValue)
    {
        statuses.addToStatus("responseValidation", std::string("Reading value error: response without a value."));
        return false;
    }
    if (config.domainSource == DomainSource::ServerTimestamp && !value.getValue().hasServerTimestamp)
    {
        statuses.addToStatus("responseValidation", std::string("Reading value error: there is no required server timestamp"));
        return false;
    }
    if (config.domainSource == DomainSource::SourceTimestamp && !value.getValue().hasSourceTimestamp)
    {
        statuses.addToStatus("responseValidation", std::string("Reading value error: there is no required source timestamp"));
        return false;
    }

    statuses.reset("responseValidation");
    return true;
}

bool OpcUaMonitoredItemFbImpl::validateValueDataType(const OpcUaDataValue& value)
{
    OpcUaNodeId valueDataType(value.getValue().value.type->typeId);
    if (valueDataType != nodeDataType)
    {
        nodeDataType = std::move(valueDataType);
        adjustSignalDescriptor();
        outputSignal.setDescriptor(outputSignalDescriptor);
    }

    bool valid = (value.isNumber() || value.isString());
    if (valid)
        statuses.reset("valueValidation");
    else
        statuses.set("valueValidation", "Value has unsupported type.");

    return valid;
}

void OpcUaMonitoredItemFbImpl::createSignal()
{
    auto lock = this->getRecursiveConfigLock();
    LOG_I("Creating a signal...");

    outputSignal = createAndAddSignal(OPCUA_VALUE_SIGNAL_LOCAL_ID, outputSignalDescriptor);
    if (config.domainSource != DomainSource::None)
        outputSignal.setDomainSignal(createDomainSignal());
}

void OpcUaMonitoredItemFbImpl::reconfigureSignal(const FbConfig& prevConfig)
{
    auto lock = this->getRecursiveConfigLock();

    if (config.domainSource != DomainSource::None)
    {
        if (outputDomainSignal.assigned())
        {
            outputSignal.setDomainSignal(nullptr);
            removeSignal(outputDomainSignal);
            outputDomainSignal = nullptr;
        }
    }
    else if (!outputDomainSignal.assigned())
    {
        outputSignal.setDomainSignal(createDomainSignal());
    }
}

SignalConfigPtr OpcUaMonitoredItemFbImpl::createDomainSignal()
{
    const auto domainSignalDsc = DataDescriptorBuilder()
                                     .setSampleType(SampleType::UInt64)
                                     .setRule(ExplicitDataRule())
                                     .setUnit(Unit("s", -1, "seconds", "time"))
                                     .setTickResolution(Ratio(1, 1'000'000))
                                     .setOrigin("1970-01-01T00:00:00")
                                     .setName("Time")
                                     .build();
    outputDomainSignal = createAndAddSignal(OPCUA_TS_SIGNAL_LOCAL_ID, domainSignalDsc, false);
    return outputDomainSignal;
}

void OpcUaMonitoredItemFbImpl::runReaderThread()
{
    running = true;
    readerThread = std::thread([this] { readerLoop(); });
}

void OpcUaMonitoredItemFbImpl::readerLoop()
{
    while (running)
    {
        {
            // auto lockProcessing = std::scoped_lock(processingMutex);
            if (statuses.ok("config") && statuses.ok("nodeValidation"))
            {
                OpcUaDataValue dataValue;
                try
                {
                    dataValue = client->readDataValue(nodeId);

                    statuses.reset("exeption");
                    if (validateResponse(dataValue) && validateValueDataType(dataValue))
                    {
                        const auto dps = buildDataPacket(dataValue);
                        outputSignal.sendPacket(dps.dataPacket);
                        if (dps.domainDataPacket.assigned() && outputDomainSignal.assigned())
                            outputDomainSignal.sendPacket(dps.domainDataPacket);
                    }
                }
                catch (OpcUaException&)
                {
                    statuses.set("exeption", "Exeption while reading.");
                }
            }
        }
        updateStatuses();
        std::this_thread::sleep_for(std::chrono::milliseconds(config.samplingInterval));
    }
}

OpcUaMonitoredItemFbImpl::DataPackets OpcUaMonitoredItemFbImpl::buildDataPacket(const OpcUaDataValue& value)
{
    DataPackets dps;
    dps.domainDataPacket = buildDomainDataPacket(value);

    if (value.isString())
    {
        const auto convertedValue = value.toString();
        dps.dataPacket = daq::BinaryDataPacket(dps.domainDataPacket, outputSignalDescriptor, convertedValue.size());
        std::memcpy(dps.dataPacket.getRawData(), convertedValue.data(), convertedValue.size());
    }
    else if (value.isInteger() || value.isReal())
    {
        if (dps.domainDataPacket.assigned())
            dps.dataPacket = daq::DataPacketWithDomain(dps.domainDataPacket, outputSignalDescriptor, 1);
        else
            dps.dataPacket = daq::DataPacket(outputSignalDescriptor, 1);

        switch (value.getValue().value.type->typeKind)
        {
            case UA_TYPES_SBYTE:
                *(static_cast<int8_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_SByte>();
                break;
            case UA_TYPES_BYTE:
                *(static_cast<uint8_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Byte>();
                break;
            case UA_TYPES_INT16:
                *(static_cast<int16_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Int16>();
                break;
            case UA_TYPES_UINT16:
                *(static_cast<uint16_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_UInt16>();
                break;
            case UA_TYPES_INT32:
                *(static_cast<int32_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Int32>();
                break;
            case UA_TYPES_UINT32:
                *(static_cast<uint32_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_UInt32>();
                break;
            case UA_TYPES_INT64:
                *(static_cast<int64_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Int64>();
                break;
            case UA_TYPES_UINT64:
                *(static_cast<uint64_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_UInt64>();
                break;
            case UA_TYPES_FLOAT:
                *(static_cast<float*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Float>();
                break;
            case UA_TYPES_DOUBLE:
                *(static_cast<double*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Double>();
                break;
            default:
                break;
        }
    }

    return dps;
}

DataPacketPtr OpcUaMonitoredItemFbImpl::buildDomainDataPacket(const OpcUaDataValue& value)
{
    DataPacketPtr domainDp;
    if (!outputDomainSignal.assigned())
        return domainDp;

    auto fillDmainPacket = [this](uint64_t ts)
    {
        DataPacketPtr domainDp = daq::DataPacket(outputDomainSignal.getDescriptor(), 1);
        std::memcpy(domainDp.getRawData(), &ts, sizeof(ts));
        return domainDp;
    };

    if (config.domainSource == DomainSource::ServerTimestamp && value.hasServerTimestamp())
    {
        domainDp = fillDmainPacket(value.getServerTimestampUnixEpoch());
    }
    else if (config.domainSource == DomainSource::SourceTimestamp && value.hasSourceTimestamp())
    {
        domainDp = fillDmainPacket(value.getSourceTimestampUnixEpoch());
    }

    return domainDp;
}

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
