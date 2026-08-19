#include <opcuatms_client/objects/tms_client_property_factory.h>
#include <opcuatms_client/objects/tms_client_object_impl.h>
#include <opcuatms/core_types_utils.h>
#include <coreobjects/coercer_factory.h>
#include <coreobjects/eval_value_factory.h>
#include <coreobjects/validator_factory.h>
#include <coreobjects/property_factory.h>
#include <coretypes/exceptions.h>
#include <opcuatms/converters/variant_converter.h>
#include <opcuatms/converters/selection_converter.h>
#include <open62541/daqbt_nodeids.h>
#include <opendaq/custom_log.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

using namespace daq::opcua;

namespace details
{
    enum class PropertyField
    {
        CoercionExpression = 0,
        ValidationExpression,
        DefaultValue,
        IsReadOnly,
        IsVisible,
        Unit,
        MaxValue,
        MinValue,
        SuggestedValues,
        SelectionValues
    };

    static std::unordered_map<std::string, PropertyField> stringToPropertyFieldEnum{
        {"CoercionExpression", PropertyField::CoercionExpression},
        {"ValidationExpression", PropertyField::ValidationExpression},
        {"DefaultValue", PropertyField::DefaultValue},
        {"IsReadOnly", PropertyField::IsReadOnly},
        {"IsVisible", PropertyField::IsVisible},
        {"Unit", PropertyField::Unit},
        {"MaxValue", PropertyField::MaxValue},
        {"MinValue", PropertyField::MinValue},
        {"SuggestedValues", PropertyField::SuggestedValues},
        {"SelectionValues", PropertyField::SelectionValues},
    };

    static CoreType readValueType(const TmsClientContextPtr& clientContext, const ContextPtr& daqContext, const opcua::OpcUaNodeId& nodeId)
    {
        const auto reader = clientContext->getAttributeReader();
        const auto dataType = reader->getValue(nodeId, UA_ATTRIBUTEID_DATATYPE).toNodeId();
        const auto enumerationTypeId = OpcUaNodeId(0, UA_NS0ID_ENUMERATION);

        if (clientContext->getReferenceBrowser()->isSubtypeOf(dataType, enumerationTypeId))
            return ctEnumeration;

        const auto variant = reader->getValue(nodeId, UA_ATTRIBUTEID_VALUE);
        const auto object = VariantConverter<IBaseObject>::ToDaqObject(variant, daqContext);
        return object.getCoreType();
    }

    static void applyEvalExpressionField(const PropertyBuilderPtr& propBuilder,
                                          PropertyField propertyField,
                                          const StringPtr& evalStr,
                                          int64_t commonAccessLevel,
                                          bool isExecutableProperty,
                                          bool commonExecutable)
    {
        switch (propertyField)
        {
            case PropertyField::DefaultValue:
                propBuilder.setDefaultValue(EvalValue(evalStr));
                break;

            case PropertyField::IsReadOnly:
                if ((commonAccessLevel & UA_ACCESSLEVELMASK_WRITE) != 0)
                    propBuilder.setReadOnly(EvalValue(evalStr).asPtr<IBoolean>());
                else
                    propBuilder.setReadOnly(true);
                break;

            case PropertyField::IsVisible:
                if (!isExecutableProperty || commonExecutable)
                    propBuilder.setVisible(EvalValue(evalStr).asPtr<IBoolean>());
                else
                    propBuilder.setVisible(false);
                break;

            case PropertyField::Unit:
                propBuilder.setUnit(EvalValue(evalStr).asPtr<IUnit>());
                break;

            case PropertyField::MaxValue:
                propBuilder.setMaxValue(EvalValue(evalStr).asPtr<INumber>());
                break;

            case PropertyField::MinValue:
                propBuilder.setMinValue(EvalValue(evalStr).asPtr<INumber>());
                break;

            case PropertyField::SuggestedValues:
                propBuilder.setSuggestedValues(EvalValue(evalStr).asPtr<IList>());
                break;

            case PropertyField::SelectionValues:
                propBuilder.setSelectionValues(EvalValue(evalStr));
                break;
            case PropertyField::CoercionExpression:
            case PropertyField::ValidationExpression:
                break;
        }
    }

    static void applyDirectValueField(const PropertyBuilderPtr& propBuilder,
                                       PropertyField propertyField,
                                       const ContextPtr& daqContext,
                                       const TmsClientContextPtr& clientContext,
                                       const opcua::OpcUaNodeId& nodeId,
                                       const opcua::OpcUaNodeId& childNodeId,
                                       const StringPtr& name,
                                       const LoggerComponentPtr& loggerComponent,
                                       int64_t commonAccessLevel,
                                       bool isExecutableProperty,
                                       bool commonExecutable)
    {
        const auto reader = clientContext->getAttributeReader();

        switch (propertyField)
        {
            case PropertyField::DefaultValue:
            {
                // ToDo: This is a workarround for devices which are delivering not a default value,
                // even if this is a mandatory property in the openDAQ Standard.
                // However, the SDK creates too strong a requirement, which cannot be
                // met by all the standards or devices to be embraced.
                // In this case the actual value from the first connect is set to it.
                // But, this creates a weak point:
                // SDK stores only values of variables which are != to the device default value.
                // The choosen default value could be not the true default value from the device.
                // So, all in all we aligned on that in future the SDK will also support properties
                // which have not a default value as the device for which the workaround is needed.
                // But this is feature request and is covered with
                // https://blueberrydaq.atlassian.net/browse/TBBAS-1216.
                // But as long as the feature is not implemented this is a valid workarround to get
                // devices working which are deliviering not a default value via the opc-ua interface.
                // Afterwards, the workaround needs to be rolled back.

                auto value = reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE);
                BaseObjectPtr defaultValue;
                if (value.isNull())
                {
                    value = reader->getValue(nodeId, UA_ATTRIBUTEID_VALUE);
                    defaultValue = VariantConverter<IBaseObject>::ToDaqObject(value, daqContext);
                    LOG_W(
                        "Failed to read default value of property {} on OpcUa client. Default value is set to the value at connection time.",
                        name);
                }

                //Special handling for enumerations as this data type is encoded as Int32 in OPCUA
                const auto dataType = reader->getValue(nodeId, UA_ATTRIBUTEID_DATATYPE).toNodeId();
                const auto enumerationTypeId = OpcUaNodeId(0, UA_NS0ID_ENUMERATION);

                if (clientContext->getReferenceBrowser()->isSubtypeOf(dataType, enumerationTypeId))
                {
                    if (value->type != &UA_TYPES[UA_TYPES_INT32])
                        DAQ_THROW_EXCEPTION(ConversionFailedException, "Enumeration node data type is not uint32_t");

                    const auto enumBrowseName = clientContext->getClient()->readBrowseName(dataType);
                    const auto enumType = GetUAEnumerationDataTypeByName(enumBrowseName);
                    OpcUaVariant variant{};
                    UA_Variant_setScalarCopy(&variant.getValue(), value->data, enumType);
                    defaultValue = VariantConverter<IEnumeration>::ToDaqObject(variant, daqContext);
                }
                else
                    defaultValue = VariantConverter<IBaseObject>::ToDaqObject(value, daqContext);

                propBuilder.setDefaultValue(defaultValue);
                break;
            }
            case PropertyField::IsReadOnly:
                if ((commonAccessLevel & UA_ACCESSLEVELMASK_WRITE) != 0)
                    propBuilder.setReadOnly(VariantConverter<IBoolean>::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE)));
                else
                    propBuilder.setReadOnly(true);
                break;
            case PropertyField::IsVisible:
                if (!isExecutableProperty || commonExecutable)
                    propBuilder.setVisible(VariantConverter<IBoolean>::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE)));
                else
                    propBuilder.setVisible(false);
                break;
            case PropertyField::Unit:
                propBuilder.setUnit(VariantConverter<IUnit>::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE)));
                break;
            case PropertyField::MaxValue:
                propBuilder.setMaxValue(VariantConverter<INumber>::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE)));
                break;
            case PropertyField::MinValue:
                propBuilder.setMinValue(VariantConverter<INumber>::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE)));
                break;
            case PropertyField::SuggestedValues:
                propBuilder.setSuggestedValues(
                    VariantConverter<IBaseObject>::ToDaqList(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE), daqContext));
                break;
            case PropertyField::SelectionValues:
                propBuilder.setSelectionValues(
                    SelectionVariantConverter::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE)));
                break;
            case PropertyField::CoercionExpression:
            case PropertyField::ValidationExpression:
                break;
        }
    }

    static void configurePropertyFields(const PropertyBuilderPtr& propBuilder,
                                         const ContextPtr& daqContext,
                                         const TmsClientContextPtr& clientContext,
                                         const opcua::OpcUaNodeId& nodeId,
                                         const StringPtr& name,
                                         const LoggerComponentPtr& loggerComponent,
                                         CoreType valueType)
    {
        const auto evaluationVariableTypeId = OpcUaNodeId(NAMESPACE_DAQBT, UA_DAQBTID_EVALUATIONVARIABLETYPE);
        const auto& references = clientContext->getReferenceBrowser()->browse(nodeId);
        const auto reader = clientContext->getAttributeReader();

        int64_t userAccessLevel = reader->getValue(nodeId, UA_ATTRIBUTEID_USERACCESSLEVEL).toInteger();
        int64_t accessLevel = reader->getValue(nodeId, UA_ATTRIBUTEID_ACCESSLEVEL).toInteger();
        int64_t commonAccessLevel = userAccessLevel & accessLevel;

        propBuilder.setReadOnly((commonAccessLevel & UA_ACCESSLEVELMASK_WRITE) == 0);

        bool isExecutableProperty = (valueType == CoreType::ctFunc || valueType == CoreType::ctProc);
        bool commonExecutable = true;
        if (isExecutableProperty)
        {
            commonExecutable = GetExecutePermission(clientContext, daqContext, nodeId);
            propBuilder.setVisible(commonExecutable);
        }

        for (const auto& [browseName, ref] : references.byBrowseName)
        {
            const auto childNodeId = OpcUaNodeId(ref->nodeId.nodeId);

            if (browseName == "CoercionExpression")
            {
                const auto eval = VariantConverter<IString>::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE));
                if (eval.assigned() && eval.getLength() > 0)
                    propBuilder.setCoercer(Coercer(eval));
            }
            else if (browseName == "ValidationExpression")
            {
                const auto eval = VariantConverter<IString>::ToDaqObject(reader->getValue(childNodeId, UA_ATTRIBUTEID_VALUE));
                if (eval.assigned() && eval.getLength() > 0)
                    propBuilder.setValidator(Validator(eval));
            }
            else if (clientContext->getReferenceBrowser()->isSubtypeOf(ref->typeDefinition.nodeId, evaluationVariableTypeId))
            {
                auto evalId = clientContext->getReferenceBrowser()->getChildNodeId(childNodeId, "EvaluationExpression");

                StringPtr evalStr = VariantConverter<IString>::ToDaqObject(reader->getValue(evalId, UA_ATTRIBUTEID_VALUE));
                if (details::stringToPropertyFieldEnum.count(browseName))
                {
                    const auto propertyField = details::stringToPropertyFieldEnum[browseName];
                    bool strHasValue = evalStr.assigned() && evalStr.getLength() > 0;
                    if (strHasValue)
                        applyEvalExpressionField(propBuilder, propertyField, evalStr, commonAccessLevel, isExecutableProperty, commonExecutable);
                    else
                        applyDirectValueField(propBuilder, propertyField, daqContext, clientContext, nodeId, childNodeId, name, loggerComponent,
                                              commonAccessLevel, isExecutableProperty, commonExecutable);
                }
            }
        }
    }
}

PropertyPtr TmsClientProperty(const ContextPtr& daqContext, const TmsClientContextPtr& ctx, const opcua::OpcUaNodeId& nodeId, const StringPtr& propertyName)
{
    if (!daqContext.getLogger().assigned())
        DAQ_THROW_EXCEPTION(ArgumentNullException, "Logger must not be null");

    const auto loggerComponent = daqContext.getLogger().getOrAddComponent("TmsClientProperty");

    ctx->readObjectAttributes(nodeId);

    const auto reader = ctx->getAttributeReader();

    StringPtr name = propertyName;
    if (!name.assigned())
        name = String(reader->getValue(nodeId, UA_ATTRIBUTEID_DISPLAYNAME).toString());

    const auto description = String(reader->getValue(nodeId, UA_ATTRIBUTEID_DESCRIPTION).toString());
    const auto valueType = details::readValueType(ctx, daqContext, nodeId);

    auto propBuilder = PropertyBuilder(name).setValueType(valueType).setDescription(description);
    details::configurePropertyFields(propBuilder, daqContext, ctx, nodeId, name, loggerComponent, valueType);

    return propBuilder.build();
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
