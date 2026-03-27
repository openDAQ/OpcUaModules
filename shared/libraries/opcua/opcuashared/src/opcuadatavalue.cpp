#include "opcuashared/opcuadatavalue.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA

UA_StatusCode OpcUaDataValue::getStatusCode() const
{
    if (!getDataValue().hasStatus)
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    return OpcUaObject<UA_DataValue>::getValue().status;
}

bool OpcUaDataValue::isStatusOK() const
{
    if (!getDataValue().hasStatus)
        return false;
    return (getStatusCode() == UA_STATUSCODE_GOOD);
}

bool OpcUaDataValue::hasServerTimestamp() const
{
    return getDataValue().hasServerTimestamp;
}

bool OpcUaDataValue::hasSourceTimestamp() const
{
    return getDataValue().hasSourceTimestamp;
}

UA_DateTime OpcUaDataValue::getServerTimestampUnixEpoch() const
{
    return (hasServerTimestamp()) ? toUnixTimeUs(getDataValue().serverTimestamp) : 0;
}

UA_DateTime OpcUaDataValue::getSourceTimestampUnixEpoch() const
{
    return (hasSourceTimestamp()) ? toUnixTimeUs(getDataValue().sourceTimestamp) : 0;
}

const UA_DataValue& OpcUaDataValue::getDataValue() const
{
    return OpcUaObject<UA_DataValue>::getValue();
}

bool OpcUaDataValue::hasValue() const
{
    return getDataValue().hasValue;
}

uint64_t OpcUaDataValue::toUnixTimeUs(UA_DateTime date)
{
    if (date == 0)
        return 0;
    return static_cast<uint64_t>((date - UA_DATETIME_UNIX_EPOCH) / UA_DATETIME_USEC);
}

UA_DateTime OpcUaDataValue::fromUnixTimeUs(uint64_t date)
{
    if (date == 0)
        return 0;
    return static_cast<UA_DateTime>(date * UA_DATETIME_USEC + UA_DATETIME_UNIX_EPOCH);
}

bool OpcUaDataValue::isInteger() const
{
    return VariantUtils::IsInteger(this->value.value);
}

bool OpcUaDataValue::isString() const
{
    return VariantUtils::HasScalarType<UA_String>(value.value) ||
           VariantUtils::HasScalarType<UA_LocalizedText>(value.value);
}

bool OpcUaDataValue::isDouble() const
{
    return VariantUtils::HasScalarType<UA_Double>(value.value);
}

bool OpcUaDataValue::isNull() const
{
    return VariantUtils::isNull(value.value);
}

bool OpcUaDataValue::isReal() const
{
    return VariantUtils::isReal(value.value);
}

bool OpcUaDataValue::isNumber() const
{
    return isInteger() || isReal();
}

std::string OpcUaDataValue::toString() const
{
    return VariantUtils::ToString(value.value);
}

int64_t OpcUaDataValue::toInteger() const
{
    return VariantUtils::ToNumber(value.value);
}
END_NAMESPACE_OPENDAQ_OPCUA
