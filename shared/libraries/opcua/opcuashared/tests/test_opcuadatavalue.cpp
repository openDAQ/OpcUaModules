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

END_NAMESPACE_OPENDAQ_OPCUA
