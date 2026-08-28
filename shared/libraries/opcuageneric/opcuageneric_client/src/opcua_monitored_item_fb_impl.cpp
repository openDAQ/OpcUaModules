#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/property_helper.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include "opendaq/binary_data_packet_factory.h"
#include "opendaq/packet_factory.h"
#include <chrono>
#include <limits>
#include <utility>

#define DISABLE_NODE_DATATYPE_VALIDATION

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

std::atomic<int> OpcUaMonitoredItemFbImpl::localIndex = 0;

std::unordered_map<OpcUaNodeId, daq::SampleType> OpcUaMonitoredItemFbImpl::supportedDataTypeNodeIds = {
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
    {OpcUaNodeId(0, UA_NS0ID_STRING), daq::SampleType::String},
    {OpcUaNodeId(0, UA_NS0ID_LOCALIZEDTEXT), daq::SampleType::String},
    {OpcUaNodeId(0, UA_NS0ID_QUALIFIEDNAME), daq::SampleType::String},
    {OpcUaNodeId(0, UA_NS0ID_DATETIME), daq::SampleType::Int64}};

std::unordered_map<UA_DataTypeKind, daq::SampleType> OpcUaMonitoredItemFbImpl::supportedDataTypeKinds = {
    {UA_DATATYPEKIND_FLOAT, daq::SampleType::Float32},
    {UA_DATATYPEKIND_DOUBLE, daq::SampleType::Float64},
    {UA_DATATYPEKIND_SBYTE, daq::SampleType::Int8},
    {UA_DATATYPEKIND_BYTE, daq::SampleType::UInt8},
    {UA_DATATYPEKIND_INT16, daq::SampleType::Int16},
    {UA_DATATYPEKIND_UINT16, daq::SampleType::UInt16},
    {UA_DATATYPEKIND_INT32, daq::SampleType::Int32},
    {UA_DATATYPEKIND_UINT32, daq::SampleType::UInt32},
    {UA_DATATYPEKIND_INT64, daq::SampleType::Int64},
    {UA_DATATYPEKIND_UINT64, daq::SampleType::UInt64},
    {UA_DATATYPEKIND_STRING, daq::SampleType::String},
    {UA_DATATYPEKIND_LOCALIZEDTEXT, daq::SampleType::String},
    {UA_DATATYPEKIND_QUALIFIEDNAME, daq::SampleType::String},
    {UA_DATATYPEKIND_DATETIME, daq::SampleType::Int64}};

std::unordered_map<UA_DataTypeKind, OpcUaNodeId> OpcUaMonitoredItemFbImpl::dataTypeKindToDataTypeNodeId = {
    {UA_DATATYPEKIND_FLOAT, OpcUaNodeId(0, UA_NS0ID_FLOAT)},
    {UA_DATATYPEKIND_DOUBLE, OpcUaNodeId(0, UA_NS0ID_DOUBLE)},
    {UA_DATATYPEKIND_SBYTE, OpcUaNodeId(0, UA_NS0ID_SBYTE)},
    {UA_DATATYPEKIND_BYTE, OpcUaNodeId(0, UA_NS0ID_BYTE)},
    {UA_DATATYPEKIND_INT16, OpcUaNodeId(0, UA_NS0ID_INT16)},
    {UA_DATATYPEKIND_UINT16, OpcUaNodeId(0, UA_NS0ID_UINT16)},
    {UA_DATATYPEKIND_INT32, OpcUaNodeId(0, UA_NS0ID_INT32)},
    {UA_DATATYPEKIND_UINT32, OpcUaNodeId(0, UA_NS0ID_UINT32)},
    {UA_DATATYPEKIND_INT64, OpcUaNodeId(0, UA_NS0ID_INT64)},
    {UA_DATATYPEKIND_UINT64, OpcUaNodeId(0, UA_NS0ID_UINT64)},
    {UA_DATATYPEKIND_STRING, OpcUaNodeId(0, UA_NS0ID_STRING)},
    {UA_DATATYPEKIND_LOCALIZEDTEXT, OpcUaNodeId(0, UA_NS0ID_LOCALIZEDTEXT)},
    {UA_DATATYPEKIND_QUALIFIEDNAME, OpcUaNodeId(0, UA_NS0ID_QUALIFIEDNAME)},
    {UA_DATATYPEKIND_DATETIME, OpcUaNodeId(0, UA_NS0ID_DATETIME)}};

OpcUaMonitoredItemFbImpl::OpcUaMonitoredItemFbImpl(const ContextPtr& ctx,
                                                   const ComponentPtr& parent,
                                                   const FunctionBlockTypePtr& type,
                                                   daq::opcua::OpcUaClientPtr client,
                                                   const std::string& localId,
                                                   DomainSource defaultDomainSource,
                                                   SamplingScheduler* scheduler,
                                                   const PropertyObjectPtr& config)
    : FunctionBlock(type, ctx, parent, localId.empty() ? generateLocalId() : localId)
    , client(client)
    , scheduler(scheduler)
    , statuses(std::make_shared<utils::StatusContainer>())
{
    initComponentStatus();
    initStatusContainer();
    if (config.assigned())
        initProperties(property_helper::populateDefaultConfig(type.createDefaultConfig(), config));
    else
        initProperties(type.createDefaultConfig());

    this->config.domainSource = defaultDomainSource;

    validateNode();
    adjustSignalDescriptor();
    createSignal();
    updateStatuses();
}

OpcUaMonitoredItemFbImpl::~OpcUaMonitoredItemFbImpl()
{
    detachFromScheduler();
}

void OpcUaMonitoredItemFbImpl::removed()
{
    detachFromScheduler();
    FunctionBlock::removed();
}

void OpcUaMonitoredItemFbImpl::detachFromScheduler()
{
    // Returns only once an in-progress processSample() on this item has finished, so the object can
    // be torn down afterwards.
    if (auto* sched = std::exchange(scheduler, nullptr); sched != nullptr)
        sched->unregisterItem(this);
}

void OpcUaMonitoredItemFbImpl::initStatusContainer()
{
    configErr = statuses->addStatus("Config");
    nodeValidationErr = statuses->addStatus("Node validation");
    valueValidationErr = statuses->addStatus("Value validation");
    responseValidationErr = statuses->addStatus("Response validation");
    exceptionErr = statuses->addStatus("Exception");
}

FunctionBlockTypePtr OpcUaMonitoredItemFbImpl::CreateType()
{
    auto defaultConfig = PropertyObject();
    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, String(""))
                .setDescription("Specifies a local ID for the monitored item. This is used to identify the monitored item within the "
                                "device. This property is optional and can be left empty. If not set, a local ID will be generated.");
        defaultConfig.addProperty(builder.build());
    }
    {
        auto builder =
            SelectionPropertyBuilder(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, List<IString>("Numeric", "String"), static_cast<int>(NodeIDType::String))
                .setDescription("Defines the type of the NodeID of the OPCUA node to monitor. By default it is set to String.");
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder =
            StringPropertyBuilder(PROPERTY_NAME_OPCUA_NODE_ID_STRING, String(""))
                .setDescription(fmt::format("Specifies the NodeID of the OPCUA node to monitor. Used when \"{}\" is set to String.",
                                            PROPERTY_NAME_OPCUA_NODE_ID_TYPE))
                .setVisible(EvalValue("$NodeIDType == 1"));
        defaultConfig.addProperty(builder.build());
    }

    {
        auto builder =
            IntPropertyBuilder(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC, Integer(0))
                .setDescription(fmt::format("Specifies the numeric NodeID of the OPCUA node to monitor. Used when \"{}\" is set to Numeric.",
                                            PROPERTY_NAME_OPCUA_NODE_ID_TYPE))
                .setVisible(EvalValue("$NodeIDType == 0"));
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

    const auto fbType = FunctionBlockType(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME,
                                          GENERIC_OPCUA_MONITORED_ITEM_FB_NAME,
                                          "Monitors a specified OPCUA node and outputs the value and timestamp as signals.",
                                          defaultConfig);
    return fbType;
}

void OpcUaMonitoredItemFbImpl::setDomainSource(DomainSource domainSource)
{
    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);
    if (config.domainSource != domainSource)
    {
        auto prevConfig = config;
        config.domainSource = domainSource;
        reconfigureSignal(prevConfig);
    }
}

std::string OpcUaMonitoredItemFbImpl::generateLocalId()
{
    return std::string(OPCUA_LOCAL_MONITORED_ITEM_FB_ID_PREFIX + std::to_string(localIndex++));
}

void OpcUaMonitoredItemFbImpl::adjustSignalDescriptor()
{
    auto lockProcessing = std::scoped_lock(processingMutex);
    if (nodeValidationErr.ok() && supportedDataTypeNodeIds.count(nodeDataType) != 0)
    {
        outputSignalDescriptor = DataDescriptorBuilder().setSampleType(supportedDataTypeNodeIds[nodeDataType]).build();
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
        if (propName.toStdString() == PROPERTY_NAME_OPCUA_MI_LOCAL_ID)
            continue;
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
    using namespace property_helper;

    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);

    configErr.reset();

    const auto tmpNodeIdType = readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_NODE_ID_TYPE, 0);
    const auto nodeIdType = (tmpNodeIdType == static_cast<int>(NodeIDType::Numeric)) ? NodeIDType::Numeric : NodeIDType::String;
    const auto namespaceIndex = readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 0);

    if (nodeIdType == NodeIDType::String)
    {
        const auto nodeIdString = readProperty<std::string, IString>(objPtr, PROPERTY_NAME_OPCUA_NODE_ID_STRING, "");
        if (nodeIdString.empty())
            configErr.add(fmt::format("\"{}\" property is empty!", PROPERTY_NAME_OPCUA_NODE_ID_STRING));
        else
            config.nodeId = OpcUaNodeId{static_cast<uint16_t>(namespaceIndex), nodeIdString};
    }
    else
    {
        const auto nodeIdNumeric = static_cast<uint32_t>(readProperty<int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC, 0));
        config.nodeId = OpcUaNodeId{static_cast<uint16_t>(namespaceIndex), nodeIdNumeric};
    }

    const auto samplingInterval =
        readProperty<Int, IInteger>(objPtr, PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL);
    if (samplingInterval <= 0 || samplingInterval > static_cast<Int>(std::numeric_limits<uint32_t>::max()))
    {
        configErr.add(fmt::format("Invalid value for the \"{}\" property! Sampling interval must be a positive integer.",
                                  PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL));
        samplingIntervalMs = DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL;
    }
    else
    {
        samplingIntervalMs = static_cast<uint32_t>(samplingInterval);
    }

    updateStatuses();
}

void OpcUaMonitoredItemFbImpl::propertyChanged()
{
    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);

    statuses->resetAll();

    auto prevConfig = config;
    readProperties();
    validateNode();
    adjustSignalDescriptor();
    reconfigureSignal(prevConfig);
    updateStatuses();
}

void OpcUaMonitoredItemFbImpl::updateStatuses()
{
    if (!statuses->isUpdated())
        return;

    if (!configErr.ok())
    {
        setComponentStatusWithMessage(ComponentStatus::Error, configErr.buildStatusMessage());
    }
    else if (!nodeValidationErr.ok())
    {
        setComponentStatusWithMessage(ComponentStatus::Error, nodeValidationErr.buildStatusMessage());
    }
    else if (!responseValidationErr.ok())
    {
        setComponentStatusWithMessage(ComponentStatus::Error, responseValidationErr.buildStatusMessage());
    }
    else if (!valueValidationErr.ok())
    {
        setComponentStatusWithMessage(ComponentStatus::Error, valueValidationErr.buildStatusMessage());
    }
    else if (!exceptionErr.ok())
    {
        setComponentStatusWithMessage(ComponentStatus::Error, exceptionErr.buildStatusMessage());
    }
    else
    {
        setComponentStatus(ComponentStatus::Ok);
    }
}

void OpcUaMonitoredItemFbImpl::validateNode()
{
    auto lockProcessing = std::scoped_lock(processingMutex);
    try
    {
        nodeValidationErr.reset();
        auto nodeExist = client->nodeExists(config.nodeId);
        if (!nodeExist)
        {
            nodeValidationErr.add(fmt::format("Node {} does not exist", config.nodeId.toString()));
        }
        else if (const auto nodeClass = client->readNodeClass(config.nodeId); nodeClass != UA_NodeClass::UA_NODECLASS_VARIABLE)
        {
            nodeValidationErr.add(fmt::format("Node {} is not a variable node", config.nodeId.toString()));
        }
        else if (const auto accessLevel = client->readAccessLevel(config.nodeId); (accessLevel & UA_ACCESSLEVELMASK_READ) == 0)
        {
            nodeValidationErr.add(fmt::format("There is no read permission to node {}", config.nodeId.toString()));
        }
#ifndef DISABLE_NODE_DATATYPE_VALIDATION
        else if (nodeDataType = client->readDataType(nodeId); supportedDataTypes.count(nodeDataType) == 0)
        {
            nodeValidationErr.add(fmt::format("Node {} has unsupported DataType ({})", nodeId.toString(), nodeDataType.toString()));
        }
#endif
    }
    catch (OpcUaException& ex)
    {
        if (ex.getStatusCode() == UA_STATUSCODE_BADUSERACCESSDENIED)
        {
            nodeValidationErr.add(fmt::format("Access denied for node {}", config.nodeId.toString()));
        }
        else
        {
            nodeValidationErr.add(fmt::format("Exception was thrown while node {} validatiion", config.nodeId.toString()));
        }
    }
}

bool OpcUaMonitoredItemFbImpl::validateResponse(const OpcUaDataValue& value)
{
    auto lockProcessing = std::scoped_lock(processingMutex);
    if (value.getValue().hasStatus && value.getValue().status != UA_STATUSCODE_GOOD)
    {
        responseValidationErr.set(fmt::format("Reading value error: {} - {}. ", value.getValue().status, UA_StatusCode_name(value.getValue().status)));
        return false;
    }
    if (!value.getValue().hasValue)
    {
        responseValidationErr.set(std::string("Reading value error: response without a value."));
        return false;
    }
    if (value.isNull())
    {
        responseValidationErr.set(std::string("Reading value error: response with an empty value."));
        return false;
    }
    if (config.domainSource == DomainSource::ServerTimestamp && (!value.getValue().hasServerTimestamp || value.getValue().serverTimestamp == 0))
    {
        responseValidationErr.set(std::string("Reading value error: there is no required server timestamp"));
        return false;
    }
    if (config.domainSource == DomainSource::SourceTimestamp && (!value.getValue().hasSourceTimestamp || value.getValue().sourceTimestamp == 0))
    {
        responseValidationErr.set(std::string("Reading value error: there is no required source timestamp"));
        return false;
    }

    responseValidationErr.reset();
    return true;
}

bool OpcUaMonitoredItemFbImpl::validateValueDataType(const OpcUaDataValue& value)
{
    auto lockProcessing = std::scoped_lock(processingMutex);
    OpcUaNodeId valueDataType;
    const UA_DataTypeKind typeKind = static_cast<UA_DataTypeKind>(value.getValue().value.type->typeKind);
    if (dataTypeKindToDataTypeNodeId.count(typeKind))
        valueDataType = dataTypeKindToDataTypeNodeId.at(typeKind);

    if (valueDataType != nodeDataType)
    {
        nodeDataType = std::move(valueDataType);
        adjustSignalDescriptor();
        outputSignal.setDescriptor(outputSignalDescriptor);
    }

    bool valid = (supportedDataTypeKinds.count(typeKind) != 0);
    if (valid)
        valueValidationErr.reset();
    else
        valueValidationErr.set(fmt::format("Value has unsupported type ({}).", static_cast<int>(typeKind)));

    return valid;
}

void OpcUaMonitoredItemFbImpl::createSignal()
{
    auto lock = this->getRecursiveConfigLock();
    LOG_I("Creating a signal...");

    outputSignal = createAndAddSignal(OPCUA_VALUE_SIGNAL_LOCAL_ID, outputSignalDescriptor);
    outputSignal.setName(localId.toStdString() + "ValueSignal");
    if (config.domainSource != DomainSource::None)
        outputSignal.setDomainSignal(createDomainSignal());
}

void OpcUaMonitoredItemFbImpl::reconfigureSignal(const FbConfig& prevConfig)
{
    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);

    if (config.domainSource == DomainSource::None)
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
    if (outputSignal.getDescriptor() != outputSignalDescriptor)
    {
        outputSignal.setDescriptor(outputSignalDescriptor);
    }
}

SignalConfigPtr OpcUaMonitoredItemFbImpl::createDomainSignal()
{
    auto lock = this->getRecursiveConfigLock();

    const auto domainSignalDsc = DataDescriptorBuilder()
                                     .setSampleType(SampleType::UInt64)
                                     .setRule(ExplicitDataRule())
                                     .setUnit(Unit("s", -1, "seconds", "time"))
                                     .setTickResolution(Ratio(1, 1'000'000))
                                     .setOrigin("1970-01-01T00:00:00Z")
                                     .setName("Time")
                                     .build();
    outputDomainSignal = createAndAddSignal(OPCUA_TS_SIGNAL_LOCAL_ID, domainSignalDsc, false);
    outputDomainSignal.setName(localId.toStdString() + "DomainSignal");
    return outputDomainSignal;
}

uint32_t OpcUaMonitoredItemFbImpl::getSamplingInterval() const
{
    return samplingIntervalMs.load();
}

void OpcUaMonitoredItemFbImpl::processSample()
{
    {
        auto lockProcessing = std::scoped_lock(processingMutex);
        if (configErr.ok() && nodeValidationErr.ok())
        {
            OpcUaDataValue dataValue;
            try
            {
                dataValue = client->readDataValue(config.nodeId);

                exceptionErr.reset();
                if (validateResponse(dataValue) && validateValueDataType(dataValue))
                {
                    const auto dps = buildDataPacket(dataValue);
                    if (dps.dataPacket.assigned())
                    {
                        if (dps.domainDataPacket.assigned() && outputDomainSignal.assigned())
                            outputDomainSignal.sendPacket(dps.domainDataPacket);
                        outputSignal.sendPacket(dps.dataPacket);
                    }
                    else
                    {
                        valueValidationErr.set(fmt::format("Failed to build a packet for value type ({}).",
                                                           static_cast<int>(dataValue.getValue().value.type->typeKind)));
                    }
                }
            }
            catch (const OpcUaException&)
            {
                exceptionErr.set("Exception while reading.");
            }
            catch (const std::exception& e)
            {
                exceptionErr.set(fmt::format("Exception while reading: {}", e.what()));
            }
            catch (...)
            {
                exceptionErr.set("Unknown exception while reading.");
            }
        }
    }
    updateStatuses();
}

void OpcUaMonitoredItemFbImpl::onConnectionRestored()
{
    auto lock = this->getRecursiveConfigLock();
    auto lockProcessing = std::scoped_lock(processingMutex);

    // The node may have disappeared or changed its data type while the connection was down, so the
    // validation done at construction time is redone against the reconnected server.
    statuses->resetAll();

    validateNode();
    adjustSignalDescriptor();
    reconfigureSignal(config);
    updateStatuses();
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
    else if (value.isInteger() || value.isReal() || value.isDateTime())
    {
        if (dps.domainDataPacket.assigned())
            dps.dataPacket = daq::DataPacketWithDomain(dps.domainDataPacket, outputSignalDescriptor, 1);
        else
            dps.dataPacket = daq::DataPacket(outputSignalDescriptor, 1);

        switch (value.getValue().value.type->typeKind)
        {
            case UA_DATATYPEKIND_SBYTE:
                *(static_cast<int8_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_SByte>();
                break;
            case UA_DATATYPEKIND_BYTE:
                *(static_cast<uint8_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Byte>();
                break;
            case UA_DATATYPEKIND_INT16:
                *(static_cast<int16_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Int16>();
                break;
            case UA_DATATYPEKIND_UINT16:
                *(static_cast<uint16_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_UInt16>();
                break;
            case UA_DATATYPEKIND_INT32:
                *(static_cast<int32_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Int32>();
                break;
            case UA_DATATYPEKIND_UINT32:
                *(static_cast<uint32_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_UInt32>();
                break;
            case UA_DATATYPEKIND_INT64:
                *(static_cast<int64_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Int64>();
                break;
            case UA_DATATYPEKIND_UINT64:
                *(static_cast<uint64_t*>(dps.dataPacket.getRawData())) = value.readScalar<UA_UInt64>();
                break;
            case UA_DATATYPEKIND_DATETIME:
                *(static_cast<int64_t*>(dps.dataPacket.getRawData())) =
                    *static_cast<const UA_DateTime*>(value.getValue().value.data);
                break;
            case UA_DATATYPEKIND_FLOAT:
                *(static_cast<float*>(dps.dataPacket.getRawData())) = value.readScalar<UA_Float>();
                break;
            case UA_DATATYPEKIND_DOUBLE:
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
    else if (config.domainSource == DomainSource::LocalSystemTimestamp)
    {
        const uint64_t epochTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        domainDp = fillDmainPacket(epochTime);
    }

    return domainDp;
}

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
