#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include "opendaq/binary_data_packet_factory.h"
#include "opendaq/packet_factory.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

std::atomic<int> OpcUaMonitoredItemFbImpl::localIndex = 0;

std::unordered_map<OpcUaNodeId, daq::SampleType> OpcUaMonitoredItemFbImpl::supportedDataTypes = {{OpcUaNodeId(), daq::SampleType::Undefined},
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
    , configValid(false)
    , nodeValidationError(false)
    , valueValidationError(false)
    , client(client)
    , nodeId()
    , running(false)
{
    initComponentStatus();
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
        auto builder = StringPropertyBuilder(PROPERTY_NAME_OPCUA_NODE_ID, String(""))
                           .setDescription(fmt::format("Specifies the NodeID of the OPCUA node to monitor. The format of the NodeID should correspond "
                                           "to the type specified in the \"{}\" property.", PROPERTY_NAME_OPCUA_NODE_ID_TYPE));
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

    const auto fbType =
        FunctionBlockType(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME,
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
    if (nodeValidationError == false && supportedDataTypes.count(nodeDataType) != 0)
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
    configValid = true;
    configMsg.clear();

    config.nodeIdType = NodeIDType::String;    // only string NodeIDs are supported at the moment
    config.nodeId = readProperty<std::string, IString>(objPtr, PROPERTY_NAME_OPCUA_NODE_ID, "");
    if (config.nodeId.empty())
    {
        configMsg = fmt::format("\"{}\" property is empty!", PROPERTY_NAME_OPCUA_NODE_ID);
        configValid = false;
    }

    config.namespaceIndex = readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 0);
    config.samplingInterval =
        readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL);
    if (config.samplingInterval <= 0)
    {
        configMsg = fmt::format("Invalid value for the \"{}\" property! Sampling interval must be a positive integer.", PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL);
        configValid = false;
        config.samplingInterval = DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL;
    }

    updateStatuses();
}

void OpcUaMonitoredItemFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto prevConfig = config;
    readProperties();

    nodeId = OpcUaNodeId{static_cast<uint16_t>(this->config.namespaceIndex), this->config.nodeId};

    validateNode();
    adjustSignalDescriptor();
    reconfigureSignal(prevConfig);
    updateStatuses();
}

void OpcUaMonitoredItemFbImpl::updateStatuses()
{
    if (configValid == false)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Configuration is invalid! " + configMsg);
    }
    else if (nodeValidationError)
    {
        setComponentStatusWithMessage(ComponentStatus::Error, "Node is invalid! " + nodeValidationErrorMsg);
    }
    else
    {
        setComponentStatusWithMessage(ComponentStatus::Ok, "Parsing succeeded");
    }
}

void OpcUaMonitoredItemFbImpl::validateNode()
{
    nodeValidationError = false;
    valueValidationError = false;
    nodeValidationErrorMsg.clear();
    try {
        auto nodeExist = client->nodeExists(nodeId);
        if (!nodeExist)
        {
            nodeValidationError = true;
            nodeValidationErrorMsg = fmt::format("Node {} does not exist", nodeId.toString());
        }
        else if (const auto nodeClass = client->readNodeClass(nodeId); nodeClass != UA_NodeClass::UA_NODECLASS_VARIABLE)
        {
            nodeValidationError = true;
            nodeValidationErrorMsg = fmt::format("Node {} is not a variable node", nodeId.toString());
        }
        else if (nodeDataType = client->readDataType(nodeId); supportedDataTypes.count(nodeDataType) == 0)
        {
            nodeValidationError = true;
            nodeValidationErrorMsg = fmt::format("Node {} has unsupported DataType ({})", nodeId.toString(), nodeDataType.toString());
        }
        else
        {
            nodeValidationError = false;
        }
    }
    catch (OpcUaException& ex)
    {
        nodeValidationError = true;
        if (ex.getStatusCode() == UA_STATUSCODE_BADUSERACCESSDENIED)
        {
            nodeValidationErrorMsg = fmt::format("Access denied for node {}", nodeId.toString());
        }
        else
        {
            nodeValidationErrorMsg = fmt::format("Exception was thrown while node {} validatiion", nodeId.toString());
        }
    }
}

bool OpcUaMonitoredItemFbImpl::validateValueDataType(const OpcUaVariant& value)
{
    OpcUaNodeId valueDataType(value.getValue().type->typeId);
    if (valueDataType != nodeDataType)
    {
        nodeDataType = std::move(valueDataType);
        adjustSignalDescriptor();
        outputSignal.setDescriptor(outputSignalDescriptor);
    }

    valueValidationError = !(value.isNumber() || value.isString());
    return !valueValidationError;
}

void OpcUaMonitoredItemFbImpl::createSignal()
{
    auto lock = this->getRecursiveConfigLock();
    LOG_I("Creating a signal...");

    outputSignal = createAndAddSignal(OPCUA_VALUE_SIGNAL_LOCAL_ID, outputSignalDescriptor);
    outputSignal.setDomainSignal(createDomainSignal());
}

void OpcUaMonitoredItemFbImpl::reconfigureSignal(const FbConfig& prevConfig)
{
    auto lock = this->getRecursiveConfigLock();
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
            //auto lockProcessing = std::scoped_lock(processingMutex);
            if (configValid && nodeValidationError == false)
            {

                OpcUaVariant opcUaVariant;
                try {
                    opcUaVariant = client->readValue(nodeId);
                    if (!validateValueDataType(opcUaVariant))
                    {
                       // updateStatuses?
                    }
                    else
                    {
                        const auto dp = buildDataPacket(opcUaVariant);
                        outputSignal.sendPacket(dp);
                    }
                }
                catch (OpcUaException&)
                {
                    LOG_E("Exeption while reading \"{}\"", nodeId.toString());
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config.samplingInterval));
    }
}

DataPacketPtr OpcUaMonitoredItemFbImpl::buildDataPacket(const OpcUaVariant& value)
{
    DataPacketPtr dp;
    if (value.isString())
    {
        const auto convertedValue = value.toString();
        dp = daq::BinaryDataPacket(nullptr, outputSignalDescriptor, convertedValue.size());
        std::memcpy(dp.getRawData(), convertedValue.data(), convertedValue.size());
    }
    else if (value.isInteger())
    {
        dp = daq::DataPacket(outputSignalDescriptor, 1);
        *(static_cast<int64_t*>(dp.getRawData())) = value.toInteger();
    }
    else if (value.isReal())
    {
        if (value.getValue().type->typeKind == UA_TYPES_FLOAT)
        {
            dp = daq::DataPacket(outputSignalDescriptor, 1);
            *(static_cast<float*>(dp.getRawData())) = value.toFloat();
        }
        else if (value.getValue().type->typeKind == UA_TYPES_DOUBLE)
        {
            dp = daq::DataPacket(outputSignalDescriptor, 1);
            *(static_cast<double*>(dp.getRawData())) = value.toDouble();
        }
    }
    return dp;
}

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
