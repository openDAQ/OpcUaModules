#include "opcuashared/opcuavariant.h"
#include "opcuashared/opcuacommon.h"
#include <cctype>
#include <open62541/types_generated_handling.h>


BEGIN_NAMESPACE_OPENDAQ_OPCUA

using namespace daq::opcua::utils;

OpcUaVariant::OpcUaVariant()
    : OpcUaObject<UA_Variant>()
{
}

OpcUaVariant::OpcUaVariant(const uint16_t& value)
{
    UA_Variant_setScalarCopy(&this->value, &value, &UA_TYPES[UA_TYPES_UINT16]);
}

OpcUaVariant::OpcUaVariant(const uint32_t& value)
{
    UA_Variant_setScalarCopy(&this->value, &value, &UA_TYPES[UA_TYPES_UINT32]);
}

OpcUaVariant::OpcUaVariant(const int32_t& value)
    : OpcUaVariant()
{
    UA_Variant_setScalarCopy(&this->value, &value, &UA_TYPES[UA_TYPES_INT32]);
}

OpcUaVariant::OpcUaVariant(const int64_t& value)
    : OpcUaVariant()
{
    UA_Variant_setScalarCopy(&this->value, &value, &UA_TYPES[UA_TYPES_INT64]);
}

OpcUaVariant::OpcUaVariant(const char* value)
    : OpcUaVariant()
{
    UA_String* newString = UA_String_new();
    *newString = UA_STRING_ALLOC(value);
    UA_Variant_setScalar(&this->value, newString, &UA_TYPES[UA_TYPES_STRING]);
}

OpcUaVariant::OpcUaVariant(const double& value)
    : OpcUaVariant()
{
    UA_Variant_setScalarCopy(&this->value, &value, &UA_TYPES[UA_TYPES_DOUBLE]);
}

OpcUaVariant::OpcUaVariant(const bool& value)
    : OpcUaVariant()
{
    UA_Variant_setScalarCopy(&this->value, &value, &UA_TYPES[UA_TYPES_BOOLEAN]);
}

OpcUaVariant::OpcUaVariant(const OpcUaNodeId& value)
    : OpcUaVariant()
{
    UA_Variant_setScalarCopy(&this->value, value.getPtr(), &UA_TYPES[UA_TYPES_NODEID]);
}

OpcUaVariant::OpcUaVariant(const UA_DataType* type, size_t dimension)
    : OpcUaVariant()
{
    value.type = type;
    if (dimension > 1)
    {
        value.arrayLength = dimension;
        value.arrayDimensions = static_cast<UA_UInt32*>(UA_Array_new(1, type));
        value.arrayDimensions[0] = UA_UInt32(dimension);
        value.arrayDimensionsSize = 1;
    }
}

OpcUaVariant::OpcUaVariant(const double& genericValue, const UA_DataType& originalType)
    : OpcUaVariant()
{
    UA_StatusCode status = ToUaVariant(genericValue, originalType.typeId, &this->value);
    if (status != UA_STATUSCODE_GOOD)
        throw OpcUaVariableConversionError(status);
}


void OpcUaVariant::setValue(UA_Variant&& value)
{
    OpcUaObject<UA_Variant>::setValue(std::move(value));
}

void OpcUaVariant::setValue(const UA_Variant& value, bool shallowCopy)
{
    OpcUaObject<UA_Variant>::setValue(value, shallowCopy);
}

bool OpcUaVariant::isInteger() const
{
    return VariantUtils::IsInteger(this->value);
}

bool OpcUaVariant::isString() const
{
    return VariantUtils::HasScalarType<UA_String>(value) ||
           VariantUtils::HasScalarType<UA_LocalizedText>(value);
}

bool OpcUaVariant::isDouble() const
{
    return VariantUtils::HasScalarType<UA_Double>(value);
}

bool OpcUaVariant::isBool() const
{
    return VariantUtils::HasScalarType<UA_Boolean>(value);
}

bool OpcUaVariant::isNodeId() const
{
    return VariantUtils::HasScalarType<UA_NodeId>(value);
}

bool OpcUaVariant::isNull() const
{
    return VariantUtils::isNull(value);
}

bool OpcUaVariant::isReal() const
{
    return VariantUtils::isReal(value);
}

bool OpcUaVariant::isNumber() const
{
    return isInteger() || isReal();
}

bool OpcUaVariant::isScalar() const
{
    return VariantUtils::IsScalar(value);
}

bool OpcUaVariant::isVector() const
{
    return VariantUtils::IsVector(value);
}

std::string OpcUaVariant::toString() const
{
    return VariantUtils::ToString(value);
}

int64_t OpcUaVariant::toInteger() const
{
    return VariantUtils::ToNumber(this->value);
}

double OpcUaVariant::toDouble() const
{
    return readScalar<UA_Double>();
}

float OpcUaVariant::toFloat() const
{
    return readScalar<UA_Float>();
}

bool OpcUaVariant::toBool() const
{
    return readScalar<UA_Boolean>();
}

OpcUaNodeId OpcUaVariant::toNodeId() const
{
    return VariantUtils::ToNodeId(this->value);
}

// VariantUtils

bool VariantUtils::IsScalar(const UA_Variant& value)
{
    return UA_Variant_isScalar(&value);
}

bool VariantUtils::IsVector(const UA_Variant& value)
{
    return !IsScalar(value);
}

bool VariantUtils::IsInteger(const UA_Variant& value)
{
    if (value.type && value.type->typeId.namespaceIndex == 0)  // built-in types
    {
        switch (value.type->typeKind)
        {
            case UA_TYPES_SBYTE:
            case UA_TYPES_BYTE:
            case UA_TYPES_INT16:
            case UA_TYPES_UINT16:
            case UA_TYPES_INT32:
            case UA_TYPES_UINT32:
            case UA_TYPES_INT64:
            case UA_TYPES_UINT64:
                return true;
            default:
                return false;
        }
    }
    return false;
}

bool VariantUtils::isReal(const UA_Variant& value)
{
    if (value.type == NULL)
        return false;

    switch (value.type->typeKind)
    {
        case UA_TYPES_FLOAT:
        case UA_TYPES_DOUBLE:
            return true;
        default:
            return false;
    }
}

bool VariantUtils::isNull(const UA_Variant& value)
{
    return UA_Variant_isEmpty(&value);
}

std::string VariantUtils::ToString(const UA_Variant& value)
{
    std::string result;
    if (IsType<UA_LocalizedText>(value))
    {
        result = utils::ToStdString(ReadScalar<UA_LocalizedText>(value).text);
    }
    else if (IsType<UA_QualifiedName>(value))
    {
        result = utils::ToStdString(ReadScalar<UA_QualifiedName>(value).name);
    }
    else
    {
        result = utils::ToStdString(ReadScalar<UA_String>(value));
    }
    return result;
}

int64_t VariantUtils::ToNumber(const UA_Variant& value)
{
    switch (value.type->typeKind)
    {
        case UA_TYPES_SBYTE:
            return ReadScalar<UA_SByte>(value);
        case UA_TYPES_BYTE:
            return ReadScalar<UA_Byte>(value);
        case UA_TYPES_INT16:
            return ReadScalar<UA_Int16>(value);
        case UA_TYPES_UINT16:
            return ReadScalar<UA_UInt16>(value);
        case UA_TYPES_INT32:
        case UA_TYPES_ENUMERATION:
            return ReadScalar<UA_Int32>(value);
        case UA_TYPES_UINT32:
            return ReadScalar<UA_UInt32>(value);
        case UA_TYPES_INT64:
            return ReadScalar<UA_Int64>(value);
        case UA_TYPES_UINT64:
            return ReadScalar<UA_UInt64>(value);

        default:
            throw std::runtime_error("Type not supported!");
    }
}

OpcUaNodeId VariantUtils::ToNodeId(const UA_Variant& value)
{
    UA_NodeId nodeId = ReadScalar<UA_NodeId>(value);
    return OpcUaNodeId(nodeId);
}


void VariantUtils::ToInt32Variant(OpcUaVariant& variant)
{
    if (!variant.isNumber())
        throw OpcUaException(UA_STATUSCODE_BADTYPEMISMATCH, "Variant does not contain a numeric type.");

    UA_Int32 value = (UA_Int32) variant.toInteger();
    variant.setScalar<UA_Int32>(value);
}

void VariantUtils::ToInt64Variant(OpcUaVariant& variant)
{
    if (!variant.isNumber())
        throw OpcUaException(UA_STATUSCODE_BADTYPEMISMATCH, "Variant does not contain a numeric type.");

    UA_Int64 value = (UA_Int64) variant.toInteger();
    variant.setScalar<UA_Int64>(value);
}

END_NAMESPACE_OPENDAQ_OPCUA
