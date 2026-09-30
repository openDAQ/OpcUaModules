#include <opcuatms_server/objects/tms_server_function_block_type.h>
#include <opcuatms/converters/variant_converter.h>
#include <opcuatms_server/objects/tms_server_property_object.h>
#include <opcuaserver/opcuaaddnodeparams.h>
#include <open62541/daqbsp_nodeids.h>

using namespace daq::opcua;

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

TmsServerFunctionBlockType::TmsServerFunctionBlockType(const FunctionBlockTypePtr& object,
                                                       const OpcUaServerPtr& server,
                                                       const ContextPtr& context,
                                                       const TmsServerContextPtr& tmsContext)
    : Super(object, server, context, tmsContext)
{
}

std::string TmsServerFunctionBlockType::getBrowseName()
{
    return object.getId();
}

std::string TmsServerFunctionBlockType::getDisplayName()
{
    return object.getId();
}

std::string TmsServerFunctionBlockType::getDescription()
{
    return object.getDescription();
}

OpcUaNodeId TmsServerFunctionBlockType::getTmsTypeId()
{
    return OpcUaNodeId(UA_NS0ID_BASEDATAVARIABLETYPE);
}

void TmsServerFunctionBlockType::addChildNodes()
{
    Super::addChildNodes();
    addDefaultConfigNode();
    AddOptionNodes(server, nodeId, object, this);
}

void TmsServerFunctionBlockType::AddOptionNodes(const OpcUaServerPtr& server,
                                                const OpcUaNodeId& parentNodeId,
                                                const FunctionBlockTypePtr& type,
                                                void* nodeContext)
{
    if (!type.assigned())
        return;

    const auto addNode = [&server, &parentNodeId, nodeContext](const std::string& name,
                                                               const UA_DataType& dataType,
                                                               const OpcUaVariant& value)
    {
        AddVariableNodeParams params(OpcUaNodeId(0), parentNodeId);
        params.setBrowseName(name);
        params.nodeContext = nodeContext;
        params.setDataType(OpcUaNodeId(dataType.typeId));
        params.typeDefinition = OpcUaNodeId(UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE));
        params.attr->accessLevel = UA_ACCESSLEVELMASK_READ;
        params.attr->writeMask = 0;
        params.attr->value = value.copyAndGetDetachedValue();
        server->addVariableNode(params);
    };

    addNode("AlwaysEmptyInput", UA_TYPES[UA_TYPES_BOOLEAN], OpcUaVariant(IsTrue(type.getAlwaysEmptyInput())));
    addNode("Singleton", UA_TYPES[UA_TYPES_BOOLEAN], OpcUaVariant(IsTrue(type.getSingleton())));

    const StringPtr commonSettingsTypeId = type.getCommonSettingsTypeId();
    if (commonSettingsTypeId.assigned())
        addNode("CommonSettingsTypeId", UA_TYPES[UA_TYPES_STRING], OpcUaVariant(commonSettingsTypeId.getCharPtr()));
}

void TmsServerFunctionBlockType::configureVariableNodeAttributes(OpcUaObject<UA_VariableAttributes>& attr)
{
    Super::configureVariableNodeAttributes(attr);

    attr->dataType = UA_TYPES_DAQBSP[UA_TYPES_DAQBSP_FUNCTIONBLOCKINFOSTRUCTURE].typeId;
    attr->accessLevel = UA_ACCESSLEVELMASK_READ;
    attr->writeMask = 0;

    const auto defaultValue = VariantConverter<IFunctionBlockType>::ToVariant(object);
    attr->value = defaultValue.copyAndGetDetachedValue();
}

bool TmsServerFunctionBlockType::checkPermission(const Permission permission, const UA_NodeId* const nodeId, const OpcUaSession* const sessionContext)
{
    return true;
}

void TmsServerFunctionBlockType::addDefaultConfigNode()
{
    auto defaultConfig = object.createDefaultConfig();

    if (!defaultConfig.assigned())
        return;

    defaultConfig.freeze();

    tmsDefaultConfig = std::make_shared<TmsServerPropertyObject>(defaultConfig, server, daqContext, tmsContext, "DefaultConfig");
    tmsDefaultConfig->registerOpcUaNode(nodeId);
}


END_NAMESPACE_OPENDAQ_OPCUA_TMS
