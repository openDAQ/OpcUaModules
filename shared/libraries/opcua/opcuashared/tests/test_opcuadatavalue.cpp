#include <open62541/types_generated_handling.h>
#include "gtest/gtest.h"
#include "opcuashared/opcuadatavalue.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA

using OpcUaDataValueTest = testing::Test;

TEST_F(OpcUaDataValueTest, CreateWithInt)
{
    UA_DataValue dataValue;
    UA_DataValue_init(&dataValue);

    dataValue.status = UA_STATUSCODE_BADAGGREGATELISTMISMATCH;
    dataValue.hasStatus = true;
    UA_Int64 val = 0;

    UA_Variant_setScalarCopy(&dataValue.value, &val, &UA_TYPES[UA_TYPES_INT64]);
    dataValue.hasValue = true;

    OpcUaDataValue value(dataValue, true);

    ASSERT_TRUE(value.isInteger());
    ASSERT_EQ(value.getStatusCode(), UA_STATUSCODE_BADAGGREGATELISTMISMATCH);

    UA_DataValue_clear(&dataValue);
}

TEST_F(OpcUaDataValueTest, CreateWithIntRawDataValue)
{
    UA_DataValue dataValue;
    UA_DataValue_init(&dataValue);

    dataValue.status = UA_STATUSCODE_BADAGGREGATELISTMISMATCH;
    dataValue.hasStatus = true;
    UA_Int64 val = 0;

    UA_Variant_setScalarCopy(&dataValue.value, &val, &UA_TYPES[UA_TYPES_INT64]);
    dataValue.hasValue = true;

    OpcUaDataValue value(dataValue, true);

    const UA_DataValue& rawDataValue = value.getDataValue();
    ASSERT_EQ(rawDataValue.value.data, dataValue.value.data);

    UA_DataValue_clear(&dataValue);
}

TEST_F(OpcUaDataValueTest, TestNoCopyBehaviour)
{
    UA_DataValue dataValue;
    UA_DataValue_init(&dataValue);

    dataValue.status = UA_STATUSCODE_BADAGGREGATELISTMISMATCH;
    dataValue.hasStatus = true;

    UA_Int64* val = UA_Int64_new();
    *val = 1;

    UA_Variant_setScalar(&dataValue.value, val, &UA_TYPES[UA_TYPES_INT64]);
    dataValue.hasValue = true;

    OpcUaDataValue value(dataValue, true);
    ASSERT_EQ(value.toInteger(), 1);
    *val = 2;
    ASSERT_EQ(value.toInteger(), 2);

    ASSERT_EQ(value.getValue().value.data, dataValue.value.data);

    UA_DataValue_clear(&dataValue);
}

TEST_F(OpcUaDataValueTest, TestCopyBehaviour)
{
    UA_DataValue dataValue;
    UA_DataValue_init(&dataValue);

    dataValue.status = UA_STATUSCODE_BADAGGREGATELISTMISMATCH;
    dataValue.hasStatus = true;

    UA_Int64* val = UA_Int64_new();
    *val = 1;

    UA_Variant_setScalar(&dataValue.value, val, &UA_TYPES[UA_TYPES_INT64]);
    dataValue.hasValue = true;

    OpcUaDataValue value(dataValue);
    ASSERT_EQ(value.toInteger(), 1);
    *val = 2;
    ASSERT_EQ(value.toInteger(), 1);

    ASSERT_NE(value.getValue().value.data, dataValue.value.data);

    UA_DataValue_clear(&dataValue);
    ASSERT_EQ(value.toInteger(), 1);
    value.setScalar(UA_Int64(5));
    ASSERT_EQ(value.toInteger(), 5);
}

static bool isDateTimeOf(const void* val, const UA_DataType* type)
{
    UA_DataValue dataValue;
    UA_DataValue_init(&dataValue);

    UA_Variant_setScalarCopy(&dataValue.value, val, type);
    dataValue.hasValue = true;

    OpcUaDataValue value(dataValue, true);
    const bool result = value.isDateTime();

    UA_DataValue_clear(&dataValue);
    return result;
}

TEST_F(OpcUaDataValueTest, IsDateTime)
{
    const UA_DateTime dateTime = UA_DateTime_fromUnixTime(1700000000);
    ASSERT_TRUE(isDateTimeOf(&dateTime, &UA_TYPES[UA_TYPES_DATETIME]));

    // UtcTime is a subtype of DateTime carrying its own UA_DataType entry
    const UA_UtcTime utcTime = UA_DateTime_fromUnixTime(1700000001);
    ASSERT_TRUE(isDateTimeOf(&utcTime, &UA_TYPES[UA_TYPES_UTCTIME]));

    const UA_Int64 int64Value = 1700000000;
    ASSERT_FALSE(isDateTimeOf(&int64Value, &UA_TYPES[UA_TYPES_INT64]));

    const UA_Double doubleValue = 1.5;
    ASSERT_FALSE(isDateTimeOf(&doubleValue, &UA_TYPES[UA_TYPES_DOUBLE]));

    ASSERT_FALSE(OpcUaDataValue().isDateTime());
}

TEST_F(OpcUaDataValueTest, DateTimeValueToUnixEpoch)
{
    const auto unixUsOf = [](UA_DateTime date, const UA_DataType* type)
    {
        UA_DataValue dataValue;
        UA_DataValue_init(&dataValue);

        UA_Variant_setScalarCopy(&dataValue.value, &date, type);
        dataValue.hasValue = true;

        OpcUaDataValue value(dataValue, true);
        const int64_t result = value.getDateTimeValueUnixEpoch();

        UA_DataValue_clear(&dataValue);
        return result;
    };

    ASSERT_EQ(unixUsOf(UA_DateTime_fromUnixTime(1700000000), &UA_TYPES[UA_TYPES_DATETIME]), 1700000000LL * 1000000);
    ASSERT_EQ(unixUsOf(UA_DateTime_fromUnixTime(1700000001), &UA_TYPES[UA_TYPES_UTCTIME]), 1700000001LL * 1000000);

    // the UNIX epoch itself, and a date before it, which must stay negative rather than wrap
    ASSERT_EQ(unixUsOf(UA_DATETIME_UNIX_EPOCH, &UA_TYPES[UA_TYPES_DATETIME]), 0);
    ASSERT_EQ(unixUsOf(UA_DateTime_fromUnixTime(-1), &UA_TYPES[UA_TYPES_DATETIME]), -1000000);

    // 0 ticks is 1601-01-01, not a null timestamp, when it comes from a node's value
    ASSERT_EQ(unixUsOf(0, &UA_TYPES[UA_TYPES_DATETIME]), -11644473600LL * 1000000);

    ASSERT_EQ(OpcUaDataValue().getDateTimeValueUnixEpoch(), 0);
}

END_NAMESPACE_OPENDAQ_OPCUA
