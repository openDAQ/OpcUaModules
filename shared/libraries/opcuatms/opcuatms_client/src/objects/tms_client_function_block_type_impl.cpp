#include <opcuatms_client/objects/tms_client_function_block_type_impl.h>
#include <opcuatms/converters/struct_converter.h>
#include <opcuatms_client/objects/tms_client_property_object_factory.h>
#include <opcuatms/converters/property_object_conversion_utils.h>
#include <opcuatms/core_types_utils.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

FunctionBlockTypeOptions ReadFunctionBlockTypeOptions(const TmsClientContextPtr& clientContext,
                                                      const opcua::OpcUaNodeId& nodeId)
{
    FunctionBlockTypeOptions options;

    const auto browser = clientContext->getReferenceBrowser();
    const auto client = clientContext->getClient();

    const auto readOption = [&browser, &client, &nodeId](const std::string& name, OpcUaVariant& value)
    {
        if (!browser->hasReference(nodeId, name))
            return false;

        value = client->readValue(browser->getChildNodeId(nodeId, name));
        return !value.isNull();
    };

    OpcUaVariant value;

    if (readOption("AlwaysEmptyInput", value) && value.isBool())
        options.alwaysEmptyInput = value.readScalar<UA_Boolean>() ? True : False;

    if (readOption("Singleton", value) && value.isBool())
        options.singleton = value.readScalar<UA_Boolean>() ? True : False;

    if (readOption("CommonSettingsTypeId", value) && value.isString())
        options.commonSettingsTypeId = ConvertToDaqCoreString(value.readScalar<UA_String>());

    return options;
}

TmsClientFunctionBlockTypeImpl::TmsClientFunctionBlockTypeImpl(const ContextPtr& context,
                                                               const TmsClientContextPtr& tmsContext,
                                                               const opcua::OpcUaNodeId& nodeId)
    : TmsClientObjectImpl(context, tmsContext, nodeId)
    , FunctionBlockTypeImpl("", "", "", nullptr)
{
    readAttributes();
}

ErrCode TmsClientFunctionBlockTypeImpl::getId(IString** id)
{
    OPENDAQ_PARAM_NOT_NULL(id);

    *id = type.getId().detach();
    return OPENDAQ_SUCCESS;
}

ErrCode TmsClientFunctionBlockTypeImpl::getName(IString** name)
{
    OPENDAQ_PARAM_NOT_NULL(name);

    *name = type.getName().detach();
    return OPENDAQ_SUCCESS;
}

ErrCode TmsClientFunctionBlockTypeImpl::getDescription(IString** description)
{
    OPENDAQ_PARAM_NOT_NULL(description);

    *description = type.getDescription().detach();
    return OPENDAQ_SUCCESS;
}

ErrCode TmsClientFunctionBlockTypeImpl::createDefaultConfig(IPropertyObject** defaultConfig)
{
    OPENDAQ_PARAM_NOT_NULL(defaultConfig);

    auto clone = PropertyObjectConversionUtils::ClonePropertyObject(this->defaultConfig);
    *defaultConfig = clone.detach();
    return OPENDAQ_SUCCESS;
}

ErrCode TmsClientFunctionBlockTypeImpl::getAlwaysEmptyInput(Bool* alwaysEmpty)
{
    OPENDAQ_PARAM_NOT_NULL(alwaysEmpty);

    *alwaysEmpty = options.alwaysEmptyInput;
    return OPENDAQ_SUCCESS;
}

ErrCode TmsClientFunctionBlockTypeImpl::getSingleton(Bool* singleton)
{
    OPENDAQ_PARAM_NOT_NULL(singleton);

    *singleton = options.singleton;
    return OPENDAQ_SUCCESS;
}

ErrCode TmsClientFunctionBlockTypeImpl::getCommonSettingsTypeId(IString** typeId)
{
    OPENDAQ_PARAM_NOT_NULL(typeId);

    *typeId = options.commonSettingsTypeId.addRefAndReturn();
    return OPENDAQ_SUCCESS;
}

void TmsClientFunctionBlockTypeImpl::readAttributes()
{
    const auto value = client->readValue(nodeId);
    this->type = VariantConverter<IFunctionBlockType>::ToDaqObject(value);
    this->options = ReadFunctionBlockTypeOptions(clientContext, nodeId);

    const auto defaultConfigId = getNodeId("DefaultConfig");
    this->defaultConfig = TmsClientPropertyObject(daqContext, clientContext, defaultConfigId);
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
