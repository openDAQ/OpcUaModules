#include "opcuashared/opcuadatavalue.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA

OpcUaVariant OpcUaDataValue::getValue() const
{
    return OpcUaVariant(OpcUaObject<UA_DataValue>::getValue().value, true);
}

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

END_NAMESPACE_OPENDAQ_OPCUA
