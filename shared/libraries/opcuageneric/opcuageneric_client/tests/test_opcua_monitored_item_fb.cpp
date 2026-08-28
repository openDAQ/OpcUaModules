#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/common.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include <opcuageneric_client/generic_client_device_impl.h>
#include <opcuageneric_client/opcuageneric.h>
#include <testutils/testutils.h>
#include "opcuageneric_client/constants.h"
#include "opcuaservertesthelper.h"
#include "opendaq/reader_factory.h"
#include "test_daq_test_helper.h"
#include "timer.h"
#include <chrono>
#include <limits>
#include <thread>

#define ASSERT_DOUBLE_NE(val1, val2) ASSERT_GT(std::abs((val1) - (val2)), 1e-9)

#define ASSERT_FLOAT_NE(val1, val2) ASSERT_GT(std::fabs((val1) - (val2)), 1e-6f)

namespace daq::opcua::generic
{
    class GenericOpcuaMonitoredItemHelper : public DaqTestHelper
    {
    public:
        using DS = DomainSource;
        daq::FunctionBlockPtr fb;
        OpcUaServerTestHelper testHelper;

        void CreateMonitoredItemFB(std::string nodeId, uint32_t index, uint32_t interval = 100)
        {
            using NT = NodeIDType;
            auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::String));
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, nodeId);
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, index);
            config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, interval);

            ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        }

        void CreateMonitoredItemFB(uint32_t numericNodeId, uint32_t nsIndex, uint32_t interval = 100)
        {
            using NT = NodeIDType;
            auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::Numeric));
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC, numericNodeId);
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, nsIndex);
            config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, interval);

            ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        }

        void CreateMonitoredItemFB(daq::PropertyObjectPtr config)
        {
            ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        }

        auto okStatus()
        {
            return Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager());
        }

        auto errStatus()
        {
            return Enumeration("ComponentStatusType", "Error", daqInstance.getContext().getTypeManager());
        }

        auto getTime()
        {
            using namespace std::chrono;
            return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
        }

        auto readValueWithTout(daq::SignalPtr sig, size_t ms, const daq::BaseObjectPtr prevVal = nullptr)
        {
            daq::BaseObjectPtr value;
            ::helper::utils::Timer timer(ms);
            do
            {
                value = sig.getLastValue();
            } while ((prevVal.assigned() ? value == prevVal : !value.assigned()) && !timer.expired());

            return value;
        };

    protected:
        void SetUp()
        {
            testHelper.startServer();
        }

        void TearDown()
        {
            if (fb.assigned())
                device.removeFunctionBlock(fb);
            testHelper.stop();
        }
    };

    class GenericOpcuaMonitoredItemTest : public testing::Test, public GenericOpcuaMonitoredItemHelper
    {
    protected:
        void SetUp() override
        {
            testing::Test::SetUp();
            GenericOpcuaMonitoredItemHelper::SetUp();
        }
        void TearDown() override
        {
            GenericOpcuaMonitoredItemHelper::TearDown();
            testing::Test::TearDown();
        }
    };

    using H = std::variant<std::string, double, float, int64_t, uint64_t, int32_t, uint32_t, int16_t, uint16_t, int8_t, uint8_t>;

    template <typename T>
    struct HelperValueType
    {
        using type = T;
    };

    // clang-format off
template<typename T> struct TypeName { static std::string Get() { return "unknown"; } };
template<> struct TypeName<float> { static std::string Get() { return "float"; } };
template<> struct TypeName<double> { static std::string Get() { return "double"; } };
template<> struct TypeName<int64_t> { static std::string Get() { return "int64_t"; } };
template<> struct TypeName<uint64_t> { static std::string Get() { return "uint64_t"; } };
template<> struct TypeName<int32_t> { static std::string Get() { return "int32_t"; } };
template<> struct TypeName<uint32_t> { static std::string Get() { return "uint32_t"; } };
template<> struct TypeName<int16_t> { static std::string Get() { return "int16_t"; } };
template<> struct TypeName<uint16_t> { static std::string Get() { return "uint16_t"; } };
template<> struct TypeName<int8_t> { static std::string Get() { return "int8_t"; } };
template<> struct TypeName<uint8_t> { static std::string Get() { return "uint8_t"; } };
template<> struct TypeName<std::string> { static std::string Get() { return "string"; } };
// clang-format on

std::string ParamNameGenerator(const testing::TestParamInfo<std::pair<OpcUaNodeId, H>>& info)
{
    return std::visit(
        [](auto& h)
        {
            using T = typename HelperValueType<std::decay_t<decltype(h)>>::type;
            std::string name = "Type_" + TypeName<T>::Get();
            return name;
        },
        info.param.second);
}
class GenericOpcuaMonitoredItemPTest : public ::testing::TestWithParam<std::pair<OpcUaNodeId, H>>, public GenericOpcuaMonitoredItemHelper
{
protected:
    void SetUp() override
    {
        testing::Test::SetUp();
        GenericOpcuaMonitoredItemHelper::SetUp();
    }
    void TearDown() override
    {
        GenericOpcuaMonitoredItemHelper::TearDown();
        testing::Test::TearDown();
    }
};

}  // namespace daq::modules::mqtt_streaming_module

using namespace daq;
using namespace daq::opcua;
using namespace daq::opcua::generic;

TEST_F(GenericOpcuaMonitoredItemTest, DefaultConfig)
{
    daq::PropertyObjectPtr defaultConfig = OpcUaMonitoredItemFbImpl::CreateType().createDefaultConfig();

    ASSERT_TRUE(defaultConfig.assigned());

    EXPECT_EQ(defaultConfig.getAllProperties().getCount(), 6u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE).getValueType(), CoreType::ctInt);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE).asPtr<IInteger>(),
              static_cast<int>(NodeIDType::String));
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID).getValueType(), CoreType::ctString);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID).asPtr<IString>().getLength(), 0u);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING).getValueType(), CoreType::ctString);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING).asPtr<IString>().getLength(), 0u);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC).getValueType(), CoreType::ctInt);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC).asPtr<IInteger>(), 0);
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX).getValueType(), CoreType::ctInt);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX).asPtr<IInteger>(), 0);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).getValueType(), CoreType::ctInt);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL)
                  .asPtr<IInteger>()
                  .getValue(DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL),
              DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).getVisible());
}

TEST_F(GenericOpcuaMonitoredItemTest, CustomLocalIdIsUsed)
{
    StartUp();
    const std::string myLocalId = "myMonitoredItem";
    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, myLocalId);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    EXPECT_EQ(fb.getLocalId().toStdString(), myLocalId);
}

TEST_F(GenericOpcuaMonitoredItemTest, EmptyLocalIdAutoGenerated)
{
    StartUp();
    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    EXPECT_NE(fb.getLocalId().toStdString().find(OPCUA_LOCAL_MONITORED_ITEM_FB_ID_PREFIX), std::string::npos);
}

TEST_F(GenericOpcuaMonitoredItemTest, CustomLocalIdSignalNames)
{
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));
    const std::string myLocalId = "myMonitoredItem";
    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, myLocalId);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    ASSERT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    EXPECT_EQ(fb.getSignals()[0].getName().toStdString(), myLocalId + "ValueSignal");
    EXPECT_EQ(fb.getSignals()[0].getDomainSignal().getName().toStdString(), myLocalId + "DomainSignal");
}

TEST_F(GenericOpcuaMonitoredItemTest, LocalIdNotPresentAsFBProperty)
{
    StartUp();
    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, "myMonitoredItem");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    EXPECT_FALSE(fb.hasProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID));
}

TEST_F(GenericOpcuaMonitoredItemTest, DuplicateLocalIdFallsBackToAutoGenerated)
{
    StartUp();
    const std::string myLocalId = "myMonitoredItem";

    auto config1 = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config1.setPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, myLocalId);
    config1.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config1.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    daq::FunctionBlockPtr fb1;
    ASSERT_NO_THROW(fb1 = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config1));
    EXPECT_EQ(fb1.getLocalId().toStdString(), myLocalId);

    auto config2 = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config2.setPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, myLocalId);
    config2.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i64");
    config2.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config2));
    EXPECT_NE(fb.getLocalId().toStdString(), myLocalId);
    EXPECT_NE(fb.getLocalId().toStdString().find(OPCUA_LOCAL_MONITORED_ITEM_FB_ID_PREFIX), std::string::npos);
}

TEST_F(GenericOpcuaMonitoredItemTest, CreationWithDefaultConfig)
{
    StartUp();
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME));
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, CreationWithPartialConfig)
{
    StartUp();
    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING, String("unknownNodeId")));
        ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
        device.removeFunctionBlock(fb);
    }

    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING, String(".i32")));
        config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1));
        ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
        device.removeFunctionBlock(fb);
    }
    fb = nullptr;
}

TEST_F(GenericOpcuaMonitoredItemTest, CreationWithCustomConfig)
{
    StartUp();
    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 100);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
}

// The caller may hand the very same config object to several addFunctionBlock calls.
TEST_F(GenericOpcuaMonitoredItemTest, AddFbWithReusedConfigObject)
{
    StartUp();
    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);

    daq::FunctionBlockPtr first;
    ASSERT_NO_THROW(first = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    ASSERT_EQ(first.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_NE(fb.getLocalId(), first.getLocalId());

    device.removeFunctionBlock(first);
}

// Properties the function block knows nothing about must be ignored, not rejected.
TEST_F(GenericOpcuaMonitoredItemTest, AddFbWithUnknownPropertiesInConfig)
{
    StartUp();
    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    config.addProperty(StringProperty("SomeForeignProperty", "value"));

    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_FALSE(fb.hasProperty("SomeForeignProperty"));
}

TEST_F(GenericOpcuaMonitoredItemTest, TwoFbCreation)
{
    StartUp();
    {
        daq::FunctionBlockPtr fb;

        auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32");
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
        config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 100);
        ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    }
    {
        daq::FunctionBlockPtr fb;

        auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i64");
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
        config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 100);
        ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    }
    auto fbs = device.getFunctionBlocks();
    ASSERT_EQ(fbs.getCount(), 2u);
}

TEST_P(GenericOpcuaMonitoredItemPTest, ReadValue)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    StartUp();
    auto param = GetParam();

    CreateMonitoredItemFB(param.first.getIdentifier(), param.first.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    std::visit(
        [&](auto& templateParam)
        {
            using T = std::decay_t<decltype(templateParam)>;

            OpcUaDataValue dataValue;
            if constexpr (std::is_same_v<T, std::string>)
            {
                UA_String myString = UA_STRING_ALLOC(templateParam.c_str());
                dataValue.setScalar(myString);
                UA_String_clear(&myString);
            }
            else
            {
                dataValue.setScalar(templateParam);
            }

            daq::BaseObjectPtr prevVal;
            daq::BaseObjectPtr val;
            {
                // before writing
                // waiting to be sure that FB has read initial value
                prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
                ASSERT_TRUE(prevVal.assigned());
                {
                    // check that the initial and target values are different
                    if constexpr (std::is_same_v<T, double>)
                        ASSERT_DOUBLE_NE(prevVal.asPtr<INumber>().getValue<T>(T(0)), templateParam);
                    else if constexpr (std::is_same_v<T, float>)
                        ASSERT_FLOAT_NE(prevVal.asPtr<INumber>().getValue<T>(T(0)), templateParam);
                    else if constexpr (std::is_same_v<T, std::string>)
                        ASSERT_NE(prevVal, templateParam);
                    else
                        ASSERT_NE(prevVal.asPtr<INumber>().getValue<T>(T(0)), templateParam);
                }
            }

            // write new value to the node
            ASSERT_NO_THROW(testHelper.writeDataValueNode(param.first, dataValue));

            {
                // after writing
                ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
                val = readValueWithTout(fb.getSignals()[0], interval * multiplier, prevVal);
                {
                    // check that the target and read values are the same
                    if constexpr (std::is_same_v<T, double>)
                        ASSERT_DOUBLE_EQ(val.asPtr<INumber>().getValue<T>(T(0)), templateParam);
                    else if constexpr (std::is_same_v<T, float>)
                        ASSERT_FLOAT_EQ(val.asPtr<INumber>().getValue<T>(T(0)), templateParam);
                    else if constexpr (std::is_same_v<T, std::string>)
                        ASSERT_EQ(val, templateParam);
                    else
                        ASSERT_EQ(val.asPtr<INumber>().getValue<T>(T(0)), templateParam);
                }
            }
        },
        param.second);
}

INSTANTIATE_TEST_SUITE_P(
    ReadNumericValue,
    GenericOpcuaMonitoredItemPTest,
    ::testing::Values(std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui8"), H{uint8_t{std::numeric_limits<uint8_t>::max()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i8"), H{int8_t{std::numeric_limits<uint8_t>::min()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui16"), H{uint16_t{std::numeric_limits<uint16_t>::max()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i16"), H{int16_t{std::numeric_limits<uint16_t>::min()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui32"), H{uint32_t{std::numeric_limits<uint32_t>::max()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i32"), H{int32_t{std::numeric_limits<int32_t>::min()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui64"), H{uint64_t{std::numeric_limits<uint64_t>::max()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i64"), H{int64_t{std::numeric_limits<int64_t>::min()}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".d"), H{double{123.456789}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".f"), H{float{float(-85) / 3}}},
                      std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".s"), H{std::string{"String with a value"}}}),
    ParamNameGenerator);

TEST_F(GenericOpcuaMonitoredItemTest, ReadValueWithServerTimestampUsingLastValue)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, ".i64");
    const auto value = int64_t{std::numeric_limits<int64_t>::min()};
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());

    const OpcUaVariant variant(value);

    // before writing
    // waiting to be sure that FB has read initial value
    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_TRUE(prevVal.assigned());

    const auto timeBefore = getTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    ASSERT_NO_THROW(testHelper.writeValueNode(nodeId, variant));

    // after writing
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    // the FB needs time to read the new value from the node
    // read the last value until it becomes different from the initial or until the timer expires
    daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], interval * multiplier, prevVal);

    auto domainVal = domainSig.getLastValue();
    const auto timeAfter = getTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    // check that the target and read values are the same
    ASSERT_EQ(val.asPtr<INumber>().getValue<int64_t>(int64_t(0)), value);

    // check the ts is between start and stop points
    ASSERT_TRUE(domainVal.assigned());
    EXPECT_GE(timeAfter, domainVal.asPtr<INumber>().getValue<uint64_t>(uint64_t(0)));
    EXPECT_LE(timeBefore, domainVal.asPtr<INumber>().getValue<uint64_t>(uint64_t(0)));
}

TEST_F(GenericOpcuaMonitoredItemTest, ReadValueWithSourceTimestampUsingLastValue)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, ".i64");
    const auto value = int64_t{std::numeric_limits<int64_t>::min()};
    StartUp(buildDeviceConfig(DomainSource::SourceTimestamp));

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());

    OpcUaDataValue dataValue;
    dataValue.setScalar(value);

    // before writing
    // waiting to be sure that FB has read initial value
    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_TRUE(prevVal.assigned());

    const auto time = getTime();

    dataValue.getValue().hasSourceTimestamp = true;
    dataValue.getValue().sourceTimestamp = OpcUaDataValue::fromUnixTimeUs(time);
    ASSERT_NO_THROW(testHelper.writeDataValueNode(nodeId, dataValue));

    // after writing
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    // the FB needs time to read the new value from the node
    // read the last value until it becomes different from the initial or until the timer expires
    daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], interval * multiplier, prevVal);

    auto domainVal = domainSig.getLastValue();

    // check that the target and read values are the same
    ASSERT_EQ(val.asPtr<INumber>().getValue<int64_t>(int64_t(0)), value);

    // check that the target and read TSes are the same
    ASSERT_TRUE(domainVal.assigned());
    EXPECT_EQ(time, domainVal.asPtr<INumber>().getValue<uint64_t>(uint64_t(0)));
}

TEST_F(GenericOpcuaMonitoredItemTest, ReadValueWithLocalSystemTimestampUsingLastValue)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, ".i64");
    const auto value = int64_t{std::numeric_limits<int64_t>::min()};
    StartUp(buildDeviceConfig(DomainSource::LocalSystemTimestamp));

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());

    const OpcUaVariant variant(value);

    // before writing
    // waiting to be sure that FB has read initial value
    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_TRUE(prevVal.assigned());

    const auto timeBefore = getTime();

    ASSERT_NO_THROW(testHelper.writeValueNode(nodeId, variant));

    // after writing
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    // the FB needs time to read the new value from the node
    // read the last value until it becomes different from the initial or until the timer expires
    daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], interval * multiplier, prevVal);

    auto domainVal = domainSig.getLastValue();
    const auto timeAfter = getTime();

    // check that the target and read values are the same
    ASSERT_EQ(val.asPtr<INumber>().getValue<int64_t>(int64_t(0)), value);

    // check the ts is between start and stop points
    ASSERT_TRUE(domainVal.assigned());
    EXPECT_GE(timeAfter, domainVal.asPtr<INumber>().getValue<uint64_t>(uint64_t(0)));
    EXPECT_LE(timeBefore, domainVal.asPtr<INumber>().getValue<uint64_t>(uint64_t(0)));
}

TEST_F(GenericOpcuaMonitoredItemTest, ReadValueWithServerTimestampUsingTailReader)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, ".i64");
    const auto value = int64_t{std::numeric_limits<int64_t>::min()};
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());

    const OpcUaVariant variant(value);

    // before writing
    // waiting to be sure that FB has read initial value
    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_TRUE(prevVal.assigned());

    const auto timeBefore = getTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    auto reader = TailReaderBuilder()
                      .setSignal(fb.getSignals()[0])
                      .setHistorySize(1)
                      .setValueReadType(SampleType::Int64)
                      .setDomainReadType(SampleType::UInt64)
                      .setSkipEvents(true)
                      .build();

    ASSERT_NO_THROW(testHelper.writeValueNode(nodeId, variant));

    // after writing
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    // the FB needs time to read the new value from the node
    // read the last value until it becomes different from the initial or until the timer expires

    SizeT count{1};
    int64_t values{};
    uint64_t domain{};
    ::helper::utils::Timer timer(interval * multiplier);
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        count = 1;
        reader.readWithDomain(&values, &domain, &count);
    } while ((count == 0 || value == prevVal) && !timer.expired());

    const auto timeAfter = getTime();

    // check that the target and read values are the same
    ASSERT_EQ(values, value);

    // check the ts is between start and stop points
    EXPECT_GE(timeAfter, domain);
    EXPECT_LE(timeBefore, domain);
}

TEST_F(GenericOpcuaMonitoredItemTest, ReadValueWithSourceTimestampUsingTailReader)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, ".i64");
    const auto value = int64_t{std::numeric_limits<int64_t>::min()};
    StartUp(buildDeviceConfig(DomainSource::SourceTimestamp));

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());

    OpcUaDataValue dataValue;
    dataValue.setScalar(value);

    // before writing
    // waiting to be sure that FB has read initial value
    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_TRUE(prevVal.assigned());

    const auto time = getTime();

    auto reader = TailReaderBuilder()
                      .setSignal(fb.getSignals()[0])
                      .setHistorySize(1)
                      .setValueReadType(SampleType::Int64)
                      .setDomainReadType(SampleType::UInt64)
                      .setSkipEvents(true)
                      .build();

    dataValue.getValue().hasSourceTimestamp = true;
    dataValue.getValue().sourceTimestamp = OpcUaDataValue::fromUnixTimeUs(time);
    ASSERT_NO_THROW(testHelper.writeDataValueNode(nodeId, dataValue));

    // after writing
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    // the FB needs time to read the new value from the node
    // read the last value until it becomes different from the initial or until the timer expires

    SizeT count{1};
    int64_t values{};
    uint64_t domain{};
    ::helper::utils::Timer timer(interval * multiplier);
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        count = 1;
        reader.readWithDomain(&values, &domain, &count);
    } while ((count == 0 || value == prevVal) && !timer.expired());


    // check that the target and read values are the same
    ASSERT_EQ(values, value);

    // check that the target and read TSes are the same
    EXPECT_EQ(time, domain);
}

TEST_F(GenericOpcuaMonitoredItemTest, ReadProtectedValue)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, ".pi64");
    const auto value = int64_t{std::numeric_limits<int64_t>::min()};
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());

    OpcUaDataValue dataValue;
    dataValue.setScalar(value);

    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_FALSE(prevVal.assigned());

    ASSERT_NO_THROW(testHelper.writeDataValueNode(nodeId, dataValue));

    // after writing
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());

    daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], interval * multiplier, prevVal);
    ASSERT_FALSE(val.assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, ReadValueWithLocalSystemTimestampUsingTailReader)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, ".i64");
    const auto value = int64_t{std::numeric_limits<int64_t>::min()};
    StartUp(buildDeviceConfig(DomainSource::LocalSystemTimestamp));

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());

    const OpcUaVariant variant(value);

    // waiting to be sure that FB has read initial value
    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_TRUE(prevVal.assigned());

    const auto timeBefore = getTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    auto reader = TailReaderBuilder()
                      .setSignal(fb.getSignals()[0])
                      .setHistorySize(1)
                      .setValueReadType(SampleType::Int64)
                      .setDomainReadType(SampleType::UInt64)
                      .setSkipEvents(true)
                      .build();

    ASSERT_NO_THROW(testHelper.writeValueNode(nodeId, variant));

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    SizeT count{1};
    int64_t values{};
    uint64_t domain{};
    ::helper::utils::Timer timer(interval * multiplier);
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        count = 1;
        reader.readWithDomain(&values, &domain, &count);
    } while ((count == 0 || value == prevVal) && !timer.expired());

    const auto timeAfter = getTime();

    ASSERT_EQ(values, value);

    EXPECT_GE(timeAfter, domain);
    EXPECT_LE(timeBefore, domain);
}

TEST_F(GenericOpcuaMonitoredItemTest, TsModeNoneCreatesSingleSignal)
{
    StartUp(buildDeviceConfig(DomainSource::None));

    CreateMonitoredItemFB(".i32", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureTsModeTogglesDomainSignal)
{
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    CreateMonitoredItemFB(".i32", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    EXPECT_TRUE(fb.getSignals()[0].getDomainSignal().assigned());

    device.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DS::None));

    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb.getSignals()[0].getDomainSignal().assigned());

    device.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DS::ServerTimestamp));

    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    EXPECT_TRUE(fb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureNodeIdFromInvalidToValid)
{
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    CreateMonitoredItemFB("nonExistent", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], 300);
    EXPECT_TRUE(val.assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureNodeIdFromValidToInvalid)
{
    StartUp();

    CreateMonitoredItemFB(".i32", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string("nonExistent"));

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureNamespaceIndex)
{
    StartUp();

    CreateMonitoredItemFB(".i32", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 0);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, SignalDescriptorSampleTypeMatchesOpcUaDataType)
{
    StartUp();

    const std::vector<std::pair<OpcUaNodeId, SampleType>> cases = {
        {OpcUaNodeId(1, ".f"),   SampleType::Float32},
        {OpcUaNodeId(1, ".d"),   SampleType::Float64},
        {OpcUaNodeId(1, ".i32"), SampleType::Int32},
        {OpcUaNodeId(1, ".i64"), SampleType::Int64},
        {OpcUaNodeId(1, ".s"),   SampleType::String},
        {OpcUaNodeId(1, ".dt"),  SampleType::Int64},
        {OpcUaNodeId(1, ".utc"), SampleType::Int64},
    };

    for (const auto& [nodeId, expectedType] : cases)
    {
        CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), 50);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
        readValueWithTout(fb.getSignals()[0], 150);
        EXPECT_EQ(fb.getSignals()[0].getDescriptor().getSampleType(), expectedType);
        device.removeFunctionBlock(fb);
        fb = nullptr;
    }
}

TEST_F(GenericOpcuaMonitoredItemTest, ReadDateTimeValue)
{
    StartUp();

    const std::vector<std::pair<OpcUaNodeId, UA_DateTime>> cases = {
        {OpcUaNodeId(1, ".dt"),  UA_DateTime_fromUnixTime(1700000000)},
        {OpcUaNodeId(1, ".utc"), UA_DateTime_fromUnixTime(1700000001)},
    };

    for (const auto& [nodeId, expected] : cases)
    {
        CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), 50);

        EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

        const daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], 300);
        ASSERT_TRUE(val.assigned());

        // the descriptor is adjusted only once the first value has been read
        EXPECT_EQ(fb.getSignals()[0].getDescriptor().getSampleType(), SampleType::Int64);

        // the value is forwarded as-is: 100 ns ticks since 1601-01-01
        EXPECT_EQ(val.asPtr<INumber>().getValue<int64_t>(int64_t(0)), static_cast<int64_t>(expected));

        device.removeFunctionBlock(fb);
        fb = nullptr;
    }
}

TEST_F(GenericOpcuaMonitoredItemTest, UnsupportedDataTypeNode)
{
    StartUp();

    // .b is a BOOLEAN node — not in supportedDataTypes
    CreateMonitoredItemFB(".b", 1, 10);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());

    daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], 300);
    EXPECT_FALSE(val.assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, FolderNode)
{
    StartUp();

    // "f1" ns=1 is an ObjectFolder, not a VARIABLE node
    CreateMonitoredItemFB("f1", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, ZeroSamplingInterval)
{
    StartUp();

    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 0);

    CreateMonitoredItemFB(config);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 10);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 0);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, NegativeSamplingInterval)
{
    StartUp();

    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, -5);

    CreateMonitoredItemFB(config);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());

    // The negative value must not wrap into a huge unsigned interval: removing the FB has to
    // detach it from the scheduler promptly instead of waiting out a weeks-long deadline.
    const auto t0 = std::chrono::steady_clock::now();
    ASSERT_NO_THROW(device.removeFunctionBlock(fb));
    fb = nullptr;
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(elapsedMs, 2 * DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL);
}

TEST_F(GenericOpcuaMonitoredItemTest, TooLargeSamplingInterval)
{
    StartUp();

    auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, static_cast<Int>(std::numeric_limits<uint32_t>::max()) + 1);

    CreateMonitoredItemFB(config);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, PropertyVisibilityTogglesWithNodeIdType)
{
    using NT = NodeIDType;
    daq::PropertyObjectPtr defaultConfig = OpcUaMonitoredItemFbImpl::CreateType().createDefaultConfig();

    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING).getVisible());
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC).getVisible());

    defaultConfig.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::Numeric));
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING).getVisible());
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC).getVisible());

    defaultConfig.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::String));
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING).getVisible());
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC).getVisible());
}

TEST_F(GenericOpcuaMonitoredItemTest, NumericNodeIdCreation)
{
    StartUp();

    CreateMonitoredItemFB(1001, 1);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, NumericNodeIdCreationInvalidNode)
{
    StartUp();

    CreateMonitoredItemFB(9999, 1);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, NumericNodeIdReadValue)
{
    constexpr uint32_t interval = 50;
    constexpr uint32_t multiplier = 50;
    const OpcUaNodeId nodeId(1, uint32_t{1001});
    StartUp();

    CreateMonitoredItemFB(1001, 1, interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    daq::BaseObjectPtr prevVal = readValueWithTout(fb.getSignals()[0], interval * multiplier);
    ASSERT_TRUE(prevVal.assigned());

    const OpcUaVariant variant(int32_t{42});
    ASSERT_NO_THROW(testHelper.writeValueNode(nodeId, variant));

    daq::BaseObjectPtr val = readValueWithTout(fb.getSignals()[0], interval * multiplier, prevVal);
    ASSERT_NE(val, prevVal);
    EXPECT_EQ(val.asPtr<INumber>().getValue<int32_t>(0), 42);
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureNodeIdTypeStringToNumeric)
{
    using NT = NodeIDType;
    StartUp();

    CreateMonitoredItemFB(".i32", 1, 100);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::Numeric));
    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC, 1001);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureNodeIdTypeNumericToString)
{
    using NT = NodeIDType;
    StartUp();

    CreateMonitoredItemFB(1001, 1, 100);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::String));
    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
}


namespace
{
// Number of samples delivered on a signal, counted without draining the reader.
daq::StreamReaderPtr makeCountingReader(const daq::SignalPtr& signal)
{
    return daq::StreamReaderBuilder()
        .setSignal(signal)
        .setValueReadType(daq::SampleType::Int64)
        .setDomainReadType(daq::SampleType::UInt64)
        .setSkipEvents(true)
        .build();
}
}

TEST_F(GenericOpcuaMonitoredItemTest, RemoveFunctionBlockWhileSampling)
{
    StartUp();

    // A short interval keeps the scheduler busy on this item, so removal is likely to land while a
    // sample is in progress.
    for (int i = 0; i < 10; ++i)
    {
        daq::FunctionBlockPtr localFb;
        auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
        config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 1);

        ASSERT_NO_THROW(localFb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ASSERT_NO_THROW(device.removeFunctionBlock(localFb));
    }

    EXPECT_EQ(device.getFunctionBlocks().getCount(), 0u);
}

TEST_F(GenericOpcuaMonitoredItemTest, ChangedSamplingIntervalTakesEffect)
{
    constexpr uint32_t slowInterval = 400;
    constexpr uint32_t fastInterval = 20;
    StartUp();

    CreateMonitoredItemFB(std::string(".i32"), 1, slowInterval);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto reader = makeCountingReader(fb.getSignals()[0]);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const auto slowCount = reader.getAvailableCount();
    EXPECT_LE(slowCount, 4u);

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, fastInterval);

    // The new interval is picked up at the next deadline shift, so within one old interval.
    std::this_thread::sleep_for(std::chrono::milliseconds(slowInterval + 500));
    EXPECT_GE(reader.getAvailableCount(), slowCount + 10u);
}
