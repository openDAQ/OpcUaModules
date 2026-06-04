#include <opcua_simple_objects/common.h>
#include <opcua_simple_objects/constants.h>
#include <opcua_simple_objects/signal.h>
#include "coreobjects/property_factory.h"
#include "coreobjects/property_object_factory.h"
#include "opcuaserver/opcuaaddnodeparams.h"
#include "opcuaserver/opcuaserver.h"
#include "opcuashared/opcuadatavalue.h"
#include "opcuashared/opcuanodeid.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_OBJECTS

std::unordered_map<SampleType, uint32_t> SignalNode::converterMap = {{SampleType::Float32, UA_TYPES_FLOAT},
                                                                     {SampleType::Float64, UA_TYPES_DOUBLE},
                                                                     {SampleType::UInt8, UA_TYPES_BYTE},
                                                                     {SampleType::Int8, UA_TYPES_SBYTE},
                                                                     {SampleType::UInt16, UA_TYPES_UINT16},
                                                                     {SampleType::Int16, UA_TYPES_INT16},
                                                                     {SampleType::UInt32, UA_TYPES_UINT32},
                                                                     {SampleType::Int32, UA_TYPES_INT32},
                                                                     {SampleType::UInt64, UA_TYPES_UINT64},
                                                                     {SampleType::Int64, UA_TYPES_INT64},
                                                                     {SampleType::String, UA_TYPES_STRING}};

SignalNode::SignalNode(daq::opcua::OpcUaServerPtr server,
                       const OpcUaNodeId parentNodeId,
                       const SignalPtr& signal,
                       const PropertyObjectPtr& config)
    : server(server)
    , signal(signal)
    , parentNodeId(parentNodeId)

{
    addVariableNode(config);
}

void SignalNode::addVariableNode(const PropertyObjectPtr& config)
{
    OpcUaNodeId variableNodeId(namespaceIndex, signal.getGlobalId().toStdString());
    AddVariableNodeParams params(variableNodeId, parentNodeId);

    auto sigDesc = signal.getDescriptor();
    if (!sigDesc.assigned())
        DAQ_THROW_EXCEPTION(UninitializedException, "Signal descriptor is not assigned. Cannot determine data type for OPC UA variable node.");

    const auto dataType = convertSampleTypeToDataTypeId(sigDesc.getSampleType());
    if (dataType.isNull())
        DAQ_THROW_EXCEPTION(InvalidTypeException, "Signal sample type is not defined or not supported.");

    params.setDataType(dataType);

    std::string browseName;
    if (config.hasProperty("BrowseName"))
    {
        browseName = config.getPropertyValue("BrowseName").asPtr<IString>().toStdString();
    }
    if (browseName.empty())
    {
        browseName = signal.getGlobalId().toStdString();
        if (browseName.front() == '/')
            browseName.erase(0, 1);
        std::replace(browseName.begin(), browseName.end(), '/', '-');
    }
    params.setBrowseName(browseName);

    params.attr->displayName = UA_LOCALIZEDTEXT_ALLOC(DEFAULT_LOCALE, signal.getName().toStdString().c_str());
    params.attr->description = UA_LOCALIZEDTEXT_ALLOC(DEFAULT_LOCALE, signal.getDescription().toStdString().c_str());
    params.attr->writeMask = UA_ATTRIBUTEWRITEMASK_NONE;
    params.attr->userWriteMask = UA_ATTRIBUTEWRITEMASK_NONE;
    params.attr->accessLevel = UA_ACCESSLEVELMASK_READ;
    params.attr->userAccessLevel = UA_ACCESSLEVELMASK_READ;
    params.attr->valueRank = UA_VALUERANK_SCALAR;
    params.attr->historizing = 0;


    params.referenceTypeId = OpcUaNodeId(UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY));
    params.typeDefinition = OpcUaNodeId(UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE));
    params.nodeContext = this;
    this->variableNodeId = server->addVariableNode(params);
}

OpcUaNodeId SignalNode::convertSampleTypeToDataTypeId(const SampleType sampleType) const
{
    OpcUaNodeId result;
    if (converterMap.count(sampleType) > 0)
        return OpcUaNodeId(UA_TYPES[converterMap[sampleType]].typeId);
    else
        return OpcUaNodeId();
}

PropertyObjectPtr SignalNode::createDefaultConfig()
{
    auto config = PropertyObject();

    config.addProperty(StringProperty("BrowseName", ""));
    return config;
}

void SignalNode::process()
{
    BaseObjectPtr lastValue;
    const BaseObjectPtr timestamp = signal.getLastValueWithTimestamp(lastValue);
    if (!lastValue.assigned())
        return;

    auto sigDesc = signal.getDescriptor();
    if (!sigDesc.assigned())
        return;
    const auto st = sigDesc.getSampleType();
    const auto dataType = convertSampleTypeToDataTypeId(st);
    if (dataType.isNull())
        return;

    auto variant = toVariant(lastValue, st);

    OpcUaDataValue dataValue;
    dataValue.getValue().value = variant.getDetachedValue();
    dataValue.getValue().hasValue = true;

    if (const auto tsInt = timestamp.asPtrOrNull<IInteger>(); tsInt.assigned())
    {
        dataValue.getValue().hasSourceTimestamp = true;
        dataValue.getValue().sourceTimestamp = OpcUaDataValue::fromUnixTimeUs(static_cast<uint64_t>(static_cast<Int>(tsInt)));
    }

    try {
        server->writeDataValue(variableNodeId, dataValue);
    } catch (OpcUaException& ex) {

    }
}

OpcUaVariant SignalNode::toVariant(const BaseObjectPtr& lastValue, SampleType sampleType) const
{
    OpcUaVariant variant;
    if (!lastValue.assigned())
        return variant;

    switch (sampleType)
    {
        case SampleType::Float32:
            variant.setScalar<UA_Float>(static_cast<UA_Float>(lastValue.asPtr<IFloat>()));
            break;
        case SampleType::Float64:
            variant.setScalar<UA_Double>(static_cast<UA_Double>(lastValue.asPtr<IFloat>()));
            break;
        case SampleType::Int8:
            variant.setScalar<UA_SByte>(static_cast<UA_SByte>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::UInt8:
            variant.setScalar<UA_Byte>(static_cast<UA_Byte>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::Int16:
            variant.setScalar<UA_Int16>(static_cast<UA_Int16>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::UInt16:
            variant.setScalar<UA_UInt16>(static_cast<UA_UInt16>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::Int32:
            variant.setScalar<UA_Int32>(static_cast<UA_Int32>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::UInt32:
            variant.setScalar<UA_UInt32>(static_cast<UA_UInt32>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::Int64:
            variant.setScalar<UA_Int64>(static_cast<UA_Int64>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::UInt64:
            variant.setScalar<UA_UInt64>(static_cast<UA_UInt64>(lastValue.asPtr<IInteger>()));
            break;
        case SampleType::String:
        {
            const auto iStr = lastValue.asPtr<IString>();
            variant = OpcUaVariant(std::string(iStr.getCharPtr(), iStr.getLength()).c_str());
        }
        break;
        default:
            break;
    }
    return variant;
}

END_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_OBJECTS