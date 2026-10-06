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
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
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
            // SamplingInterval is not part of the config: the FB starts with the device's DefaultSamplingInterval
            device.setPropertyValue(PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL, interval);

            ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        }

        void CreateMonitoredItemFB(uint32_t numericNodeId, uint32_t nsIndex, uint32_t interval = 100)
        {
            using NT = NodeIDType;
            auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::Numeric));
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC, numericNodeId);
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, nsIndex);
            // SamplingInterval is not part of the config: the FB starts with the device's DefaultSamplingInterval
            device.setPropertyValue(PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL, interval);

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

        // A node written with an explicit source timestamp keeps reporting it until the next write.
        template <typename T>
        void writeWithSourceTimestamp(const OpcUaNodeId& nodeId, T value, uint64_t sourceTimestampUs)
        {
            OpcUaDataValue dataValue;
            dataValue.setScalar(value);
            dataValue.getValue().hasSourceTimestamp = true;
            dataValue.getValue().sourceTimestamp = OpcUaDataValue::fromUnixTimeUs(sourceTimestampUs);
            ASSERT_NO_THROW(testHelper.writeDataValueNode(nodeId, dataValue));
        }

        static bool waitForDomainValue(const daq::SignalPtr& domainSig, uint64_t expected, std::chrono::milliseconds timeout)
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            while (true)
            {
                const auto value = domainSig.getLastValue();
                if (value.assigned() && value.asPtr<INumber>().getValue<uint64_t>(uint64_t(0)) == expected)
                    return true;
                if (std::chrono::steady_clock::now() >= deadline)
                    return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

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

    EXPECT_EQ(defaultConfig.getAllProperties().getCount(), 5u);

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

    // inherited from the device, not part of the config
    EXPECT_FALSE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_TS_MODE));
    EXPECT_FALSE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL));
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
    StartUp(DomainSource::ServerTimestamp);
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
        ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    }
    {
        daq::FunctionBlockPtr fb;

        auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i64");
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
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
    StartUp(DomainSource::ServerTimestamp);

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
    StartUp(DomainSource::SourceTimestamp);

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
    StartUp(DomainSource::LocalSystemTimestamp);

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
    StartUp(DomainSource::ServerTimestamp);

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
    StartUp(DomainSource::SourceTimestamp);

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
    StartUp(DomainSource::ServerTimestamp);

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
    StartUp(DomainSource::LocalSystemTimestamp);

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
    StartUp(DomainSource::None);

    CreateMonitoredItemFB(".i32", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureTsModeTogglesDomainSignal)
{
    StartUp(DomainSource::ServerTimestamp);

    CreateMonitoredItemFB(".i32", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    EXPECT_TRUE(fb.getSignals()[0].getDomainSignal().assigned());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DS::None));

    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb.getSignals()[0].getDomainSignal().assigned());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DS::ServerTimestamp));

    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    EXPECT_TRUE(fb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, FbTsModeAndSamplingIntervalInConfigAreIgnored)
{
    StartUp(DomainSource::None, 250);

    auto config = PropertyObject();
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NodeIDType::String)));
    config.addProperty(StringProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32"));
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1));
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DS::LocalSystemTimestamp)));
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 150));
    CreateMonitoredItemFB(config);

    // the initial values always come from the device
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(), static_cast<int>(DS::None));
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).asPtr<IInteger>(), 250);
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 1u);
}

TEST_F(GenericOpcuaMonitoredItemTest, FbTypeDefaultConfigHasNoTsModeAndSamplingInterval)
{
    StartUp(DomainSource::None, 250);

    auto defaultConfig = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
    EXPECT_FALSE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_TS_MODE));
    EXPECT_FALSE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL));
}

TEST_F(GenericOpcuaMonitoredItemTest, FbTsModeInheritDevTsModeWithPlainPartialConfig)
{
    StartUp(DomainSource::None);

    auto config = PropertyObject();
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NodeIDType::String)));
    config.addProperty(StringProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32"));
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1));
    CreateMonitoredItemFB(config);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(), static_cast<int>(DS::None));
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, FbTsModeInheritDevTsModeWithoutConfig)
{
    StartUp(DomainSource::LocalSystemTimestamp);

    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME));

    // no node ID is configured, so the FB is in error, but the timestamp mode must still be inherited
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(), static_cast<int>(DS::LocalSystemTimestamp));
}

TEST_F(GenericOpcuaMonitoredItemTest, FbTsModeKeptOnOtherPropertyChange)
{
    StartUp(DomainSource::None);

    CreateMonitoredItemFB(".i32", 1);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DS::ServerTimestamp));
    ASSERT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 200);

    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(), static_cast<int>(DS::ServerTimestamp));
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    EXPECT_TRUE(fb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaMonitoredItemTest, FbTsModeInheritDevTsMode)
{
    StartUp(DomainSource::None);

    CreateMonitoredItemFB(".i32", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    ASSERT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(),
              static_cast<int>(DomainSource::None));
    device.setPropertyValue(PROPERTY_NAME_OPCUA_DEFAULT_TS_MODE, static_cast<int>(DS::LocalSystemTimestamp));

    ASSERT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(),
              static_cast<int>(DomainSource::None));
    daq::FunctionBlockPtr firstFb = fb;

    CreateMonitoredItemFB(".d", 1, 100);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    ASSERT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(),
              static_cast<int>(DomainSource::LocalSystemTimestamp));
    device.setPropertyValue(PROPERTY_NAME_OPCUA_DEFAULT_TS_MODE, static_cast<int>(DS::ServerTimestamp));

    ASSERT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(),
              static_cast<int>(DomainSource::LocalSystemTimestamp));
    ASSERT_EQ(firstFb.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(),
              static_cast<int>(DomainSource::None));
}

TEST_F(GenericOpcuaMonitoredItemTest, FbSamplingIntervalInheritDevSamplingInterval)
{
    StartUp(DomainSource::ServerTimestamp, 250);

    // default config from the device's FB type, SamplingInterval left untouched
    auto makeConfig = [this](const std::string& nodeId)
    {
        auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, nodeId);
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
        return config;
    };

    CreateMonitoredItemFB(makeConfig(".i32"));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).asPtr<IInteger>(), 250);
    daq::FunctionBlockPtr firstFb = fb;

    device.setPropertyValue(PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL, 400);
    EXPECT_EQ(firstFb.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).asPtr<IInteger>(), 250);

    CreateMonitoredItemFB(makeConfig(".d"));
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).asPtr<IInteger>(), 400);
    EXPECT_EQ(firstFb.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).asPtr<IInteger>(), 250);
}

TEST_F(GenericOpcuaMonitoredItemTest, FbSamplingIntervalInheritDevSamplingIntervalWithPlainPartialConfig)
{
    StartUp(DomainSource::ServerTimestamp, 250);

    auto config = PropertyObject();
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NodeIDType::String)));
    config.addProperty(StringProperty(PROPERTY_NAME_OPCUA_NODE_ID_STRING, ".i32"));
    config.addProperty(IntProperty(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1));
    CreateMonitoredItemFB(config);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).asPtr<IInteger>(), 250);
}

TEST_F(GenericOpcuaMonitoredItemTest, FbSamplingIntervalInheritDevSamplingIntervalWithoutConfig)
{
    StartUp(DomainSource::ServerTimestamp, 250);

    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME));

    // no node ID is configured, so the FB is in error, but the sampling interval must still be inherited
    EXPECT_EQ(fb.getPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL).asPtr<IInteger>(), 250);
}

TEST_F(GenericOpcuaMonitoredItemTest, ReconfigureNodeIdFromInvalidToValid)
{
    StartUp(DomainSource::ServerTimestamp);

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
        const auto descriptor = fb.getSignals()[0].getDescriptor();
        EXPECT_EQ(descriptor.getSampleType(), SampleType::Int64);

        // a DateTime node gets openDAQ's time descriptor
        EXPECT_EQ(descriptor.getUnit().getSymbol(), "s");
        EXPECT_EQ(descriptor.getTickResolution(), Ratio(1, 1'000'000));
        EXPECT_EQ(descriptor.getOrigin(), "1970-01-01T00:00:00Z");

        // OPC UA 100 ns ticks since 1601-01-01 are rebased to us since the UNIX epoch
        const int64_t expectedUnixUs = (expected - UA_DATETIME_UNIX_EPOCH) / UA_DATETIME_USEC;
        EXPECT_EQ(val.asPtr<INumber>().getValue<int64_t>(int64_t(0)), expectedUnixUs);

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

    CreateMonitoredItemFB(config);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 0);

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

    CreateMonitoredItemFB(config);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, -5);

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

    CreateMonitoredItemFB(config);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, static_cast<Int>(std::numeric_limits<uint32_t>::max()) + 1);

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
    device.setPropertyValue(PROPERTY_NAME_OPCUA_DEFAULT_SAMPLING_INTERVAL, 1);
    for (int i = 0; i < 10; ++i)
    {
        daq::FunctionBlockPtr localFb;
        auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);

        ASSERT_NO_THROW(localFb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ASSERT_NO_THROW(device.removeFunctionBlock(localFb));
    }

    EXPECT_EQ(device.getFunctionBlocks().getCount(), 0u);
}

namespace
{
// Tells when the scheduler revalidates it. Items are revalidated in registration order, so one of
// these registered after a block is revalidated once the block has been.
class RevalidationProbe : public ISampledItem
{
public:
    uint32_t getSamplingInterval() const override
    {
        return std::numeric_limits<uint32_t>::max();
    }

    void processSample() override
    {
    }

    void onConnectionRestored() override
    {
        revalidated.set_value();
    }

    void onSchedulerDestroyed() override
    {
    }

    std::promise<void> revalidated;
};

// A block driven by a scheduler of its own, which lets a test ask for a revalidation without taking
// the server down.
struct StandaloneMonitoredItem
{
    StandaloneMonitoredItem(const daq::ContextPtr& context, const std::shared_ptr<OpcUaClient>& client, uint32_t samplingIntervalMs = 20)
    {
        const auto type = OpcUaMonitoredItemFbImpl::CreateType();
        auto config = type.createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, std::string(".i32"));
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);

        block = createWithImplementation<IFunctionBlock, OpcUaMonitoredItemFbImpl>(
            context, nullptr, type, client, "", DomainSource::SourceTimestamp, samplingIntervalMs, &scheduler, config);
        scheduler.registerItem(static_cast<OpcUaMonitoredItemFbImpl*>(block.getObject()), block);
        scheduler.registerItem(&after);
        scheduler.start();
    }

    // Returns once the block has been revalidated, or false if that did not happen in time. One-shot.
    bool revalidate(std::chrono::milliseconds timeout)
    {
        scheduler.onReconnected();
        return after.revalidated.get_future().wait_for(timeout) == std::future_status::ready;
    }

    RevalidationProbe after;
    SamplingScheduler scheduler{nullptr};
    daq::FunctionBlockPtr block;
};
}

TEST_F(GenericOpcuaMonitoredItemTest, RevalidationDoesNotNeedConfigLock)
{
    constexpr auto patience = std::chrono::seconds(5);

    DaqInstanceInit();
    auto client = std::make_shared<OpcUaClient>(testHelper.getServerUrl());
    ASSERT_NO_THROW(client->connect());
    StandaloneMonitoredItem item(daqInstance.getContext(), client);

    bool revalidated;
    {
        // Whoever holds this lock, e.g. a property write or a removal in progress, must not be able to
        // hold up the scheduler thread, and with it the sampling of every other block of the device.
        const auto lock = item.block.asPtr<IPropertyObjectInternal>(true).getRecursiveLockGuard();
        revalidated = item.revalidate(patience);
    }
    EXPECT_TRUE(revalidated) << "onConnectionRestored() is waiting for the config lock of its block";

    ASSERT_NO_THROW(item.block.remove());
}

TEST_F(GenericOpcuaMonitoredItemTest, RevalidationKeepsConfigError)
{
    constexpr auto patience = std::chrono::seconds(5);

    DaqInstanceInit();
    auto client = std::make_shared<OpcUaClient>(testHelper.getServerUrl());
    ASSERT_NO_THROW(client->connect());
    StandaloneMonitoredItem item(daqInstance.getContext(), client);

    item.block.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 0);
    const auto statuses = item.block.getStatusContainer();
    ASSERT_EQ(statuses.getStatus("ComponentStatus"), errStatus());
    const auto message = statuses.getStatusMessage("ComponentStatus");

    // A reconnect says nothing about the properties: they are not read again, so what was wrong with
    // them before is still wrong.
    ASSERT_TRUE(item.revalidate(patience));
    EXPECT_EQ(statuses.getStatus("ComponentStatus"), errStatus());
    EXPECT_EQ(statuses.getStatusMessage("ComponentStatus"), message);
}

namespace
{
// Keeps one processSample() of a block in progress: the data callback of a reader on the block's signal
// runs inside of it, on the scheduler thread, and does not return until release() is called.
// To be created once the signal has its descriptor, i.e. after the first sample. The packet that announces
// a descriptor is sent by openDAQ with the lock of the signal held, and a removal needs that lock, so it
// would wait for the callback inside of openDAQ.
class SampleInProgress
{
public:
    explicit SampleInProgress(const daq::FunctionBlockPtr& block)
        : releasedFuture(released.get_future().share())
        , reader(makeCountingReader(block.getSignals()[0]))
    {
        reader.setOnDataAvailable(
            [this]
            {
                if (std::this_thread::get_id() == owner || parked.exchange(true))
                    return;

                entered.set_value();
                releasedFuture.wait();
            });
    }

    ~SampleInProgress()
    {
        release();
    }

    bool waitUntilEntered(std::chrono::milliseconds timeout)
    {
        return entered.get_future().wait_for(timeout) == std::future_status::ready;
    }

    void release()
    {
        if (!releaseRequested.exchange(true))
            released.set_value();
    }

private:
    const std::thread::id owner{std::this_thread::get_id()};
    std::atomic<bool> parked{false};
    std::atomic<bool> releaseRequested{false};
    std::promise<void> entered;
    std::promise<void> released;
    std::shared_future<void> releasedFuture;
    daq::StreamReaderPtr reader;
};

// Runs `remove` while a sample of the block is in progress and checks that it returns without waiting for
// that sample. A removal that waited could never be made from, or be waited for by, the code that runs
// inside of the sample: core event handlers and packet callbacks of an application.
void expectRemovalDoesNotWaitForSample(const daq::FunctionBlockPtr& block, const std::function<void()>& remove)
{
    constexpr auto patience = std::chrono::seconds(5);

    SampleInProgress sample(block);
    ASSERT_TRUE(sample.waitUntilEntered(patience));

    auto removed = std::async(std::launch::async, remove);
    EXPECT_EQ(removed.wait_for(patience), std::future_status::ready) << "the removal waits for the sample in progress";

    sample.release();
    EXPECT_NO_THROW(removed.get());
}
}

TEST_F(GenericOpcuaMonitoredItemTest, RemoveFunctionBlockDoesNotWaitForSampleInProgress)
{
    StartUp();
    CreateMonitoredItemFB(".i32", 1, 20);
    const daq::FunctionBlockPtr block = fb;
    fb = nullptr;  // removed by the test itself
    ASSERT_TRUE(readValueWithTout(block.getSignals()[0], 5000).assigned());

    expectRemovalDoesNotWaitForSample(block, [&] { device.removeFunctionBlock(block); });
}

TEST_F(GenericOpcuaMonitoredItemTest, RemoveDoesNotWaitForSampleInProgress)
{
    StartUp();
    CreateMonitoredItemFB(".i32", 1, 20);
    const daq::FunctionBlockPtr block = fb;
    fb = nullptr;  // removed by the test itself
    ASSERT_TRUE(readValueWithTout(block.getSignals()[0], 5000).assigned());

    // Not through the device, so that the block has to take care of it on its own.
    expectRemovalDoesNotWaitForSample(block, [&] { block.remove(); });
}

TEST_F(GenericOpcuaMonitoredItemTest, BlockOutlivesSampleInProgress)
{
    constexpr auto patience = std::chrono::seconds(5);

    StartUp();
    CreateMonitoredItemFB(".i32", 1, 20);
    daq::FunctionBlockPtr block = fb;
    fb = nullptr;  // removed by the test itself
    ASSERT_TRUE(readValueWithTout(block.getSignals()[0], 5000).assigned());
    const daq::WeakRefPtr<daq::IFunctionBlock> weak(block);

    SampleInProgress sample(block);
    ASSERT_TRUE(sample.waitUntilEntered(patience));

    // Nothing waits for the sample any more, so nothing but the scheduler keeps the block from being
    // destroyed under it: the device lets go of the block here, and the test right after.
    device.removeFunctionBlock(block);
    block.release();
    EXPECT_TRUE(weak.getRef().assigned()) << "the block was destroyed while the scheduler was calling it";

    sample.release();
    const auto deadline = std::chrono::steady_clock::now() + patience;
    while (weak.getRef().assigned() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(weak.getRef().assigned()) << "the scheduler did not let go of the block after the sample";
}

TEST_F(GenericOpcuaMonitoredItemTest, ChangedSamplingIntervalTakesEffect)
{
    constexpr uint32_t slowInterval = 500;
    constexpr uint32_t fastInterval = 20;
    constexpr daq::SizeT packets = 6;
    StartUp();

    CreateMonitoredItemFB(std::string(".i32"), 1, slowInterval);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto reader = makeCountingReader(fb.getSignals()[0]);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const auto slowCount = reader.getAvailableCount();
    EXPECT_LE(slowCount, 3u);

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, fastInterval);

    // Time the same packets at the new interval. At the old one they would need six times 500 ms, and
    // the first of them still waits out the pending old deadline. Comparing the measured time against
    // the old interval pits the runner against itself, so a slow machine cannot fail this spuriously.
    const auto start = std::chrono::steady_clock::now();
    const bool arrived = waitForPackets(reader, slowCount + packets, std::chrono::seconds(20));
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    ASSERT_TRUE(arrived);
    EXPECT_LT(elapsedMs, packets * slowInterval);
}

TEST_F(GenericOpcuaMonitoredItemTest, UnchangedSourceTimestampIsNotRepublished)
{
    constexpr uint32_t interval = 20;
    constexpr auto quietWindow = std::chrono::milliseconds(15 * interval);
    constexpr auto patience = std::chrono::seconds(10);
    const OpcUaNodeId nodeId(1, ".i64");
    StartUp(DomainSource::SourceTimestamp);

    const uint64_t firstTs = getTime();
    writeWithSourceTimestamp(nodeId, int64_t{1}, firstTs);

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());
    ASSERT_TRUE(waitForDomainValue(domainSig, firstTs, patience));

    auto reader = makeCountingReader(fb.getSignals()[0]);

    // The node is still read every interval, but its timestamp does not change, so nothing is published.
    std::this_thread::sleep_for(quietWindow);
    EXPECT_EQ(reader.getAvailableCount(), 0u);
    EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    const uint64_t secondTs = firstTs + 1000;
    writeWithSourceTimestamp(nodeId, int64_t{2}, secondTs);

    // A new timestamp is published exactly once.
    ASSERT_TRUE(waitForPackets(reader, 1u, patience));
    std::this_thread::sleep_for(quietWindow);
    ASSERT_EQ(reader.getAvailableCount(), 1u);

    SizeT count{1};
    int64_t value{};
    uint64_t domain{};
    reader.readWithDomain(&value, &domain, &count);
    ASSERT_EQ(count, 1u);
    EXPECT_EQ(value, 2);
    EXPECT_EQ(domain, secondTs);
}

TEST_F(GenericOpcuaMonitoredItemTest, NewValueWithUnchangedTimestampIsNotPublished)
{
    constexpr uint32_t interval = 20;
    constexpr auto quietWindow = std::chrono::milliseconds(15 * interval);
    constexpr auto patience = std::chrono::seconds(10);
    const OpcUaNodeId nodeId(1, ".i64");
    StartUp(DomainSource::SourceTimestamp);

    const uint64_t ts = getTime();
    writeWithSourceTimestamp(nodeId, int64_t{1}, ts);

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());
    ASSERT_TRUE(waitForDomainValue(domainSig, ts, patience));

    auto reader = makeCountingReader(fb.getSignals()[0]);

    writeWithSourceTimestamp(nodeId, int64_t{2}, ts);

    std::this_thread::sleep_for(quietWindow);
    EXPECT_EQ(reader.getAvailableCount(), 0u);
    EXPECT_EQ(fb.getSignals()[0].getLastValue().asPtr<INumber>().getValue<int64_t>(int64_t(0)), 1);
    EXPECT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
}

TEST_F(GenericOpcuaMonitoredItemTest, SamplingIntervalChangeDoesNotRepublishUnchangedTimestamp)
{
    constexpr uint32_t interval = 20;
    constexpr uint32_t otherInterval = 10;
    constexpr auto quietWindow = std::chrono::milliseconds(15 * interval);
    constexpr auto patience = std::chrono::seconds(10);
    const OpcUaNodeId nodeId(1, ".i64");
    StartUp(DomainSource::SourceTimestamp);

    const uint64_t ts = getTime();
    writeWithSourceTimestamp(nodeId, int64_t{1}, ts);

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);
    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());
    ASSERT_TRUE(waitForDomainValue(domainSig, ts, patience));

    auto reader = makeCountingReader(fb.getSignals()[0]);

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, otherInterval);

    std::this_thread::sleep_for(quietWindow);
    EXPECT_EQ(reader.getAvailableCount(), 0u);
}

TEST_F(GenericOpcuaMonitoredItemTest, NodeIdChangeDoesNotPublishSampleWithSameTimestamp)
{
    constexpr uint32_t interval = 20;
    constexpr auto quietWindow = std::chrono::milliseconds(15 * interval);
    constexpr auto patience = std::chrono::seconds(10);
    const OpcUaNodeId firstNodeId(1, ".i64");
    const OpcUaNodeId secondNodeId(1, ".i32");
    StartUp(DomainSource::SourceTimestamp);

    // Both nodes report the same source timestamp.
    const uint64_t ts = getTime();
    writeWithSourceTimestamp(firstNodeId, int64_t{1}, ts);
    writeWithSourceTimestamp(secondNodeId, int32_t{2}, ts);

    CreateMonitoredItemFB(firstNodeId.getIdentifier(), firstNodeId.getNamespaceIndex(), interval);
    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());
    ASSERT_TRUE(waitForDomainValue(domainSig, ts, patience));

    auto reader = makeCountingReader(fb.getSignals()[0]);

    fb.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, secondNodeId.getIdentifier());
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    std::this_thread::sleep_for(quietWindow);
    EXPECT_EQ(reader.getAvailableCount(), 0u);
}

TEST_F(GenericOpcuaMonitoredItemTest, TimestampModeChangePublishesSampleAgain)
{
    constexpr uint32_t interval = 20;
    constexpr auto patience = std::chrono::seconds(10);
    const OpcUaNodeId nodeId(1, ".i64");
    StartUp(DomainSource::SourceTimestamp);

    const uint64_t ts = getTime();
    writeWithSourceTimestamp(nodeId, int64_t{1}, ts);

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);
    ASSERT_TRUE(waitForDomainValue(fb.getSignals()[0].getDomainSignal(), ts, patience));

    // Dropping the domain signal and creating it again starts over: the unchanged node is published once more.
    fb.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DomainSource::None));
    fb.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DomainSource::SourceTimestamp));

    auto domainSig = fb.getSignals()[0].getDomainSignal();
    ASSERT_TRUE(domainSig.assigned());
    EXPECT_TRUE(waitForDomainValue(domainSig, ts, patience));
}

TEST_F(GenericOpcuaMonitoredItemTest, LocalSystemTimestampPublishesUnchangedNodeEveryInterval)
{
    constexpr uint32_t interval = 20;
    constexpr daq::SizeT packets = 6;
    constexpr auto patience = std::chrono::seconds(10);
    const OpcUaNodeId nodeId(1, ".i64");
    StartUp(DomainSource::LocalSystemTimestamp);

    // Neither the value nor the server-side timestamp changes; the local time of every read does.
    writeWithSourceTimestamp(nodeId, int64_t{1}, getTime());

    CreateMonitoredItemFB(nodeId.getIdentifier(), nodeId.getNamespaceIndex(), interval);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());

    // The value descriptor is only known once the first sample is in; a reader created before that
    // would count the samples but could not read them.
    ASSERT_TRUE(readValueWithTout(fb.getSignals()[0], patience.count() * 1000).assigned());

    auto reader = makeCountingReader(fb.getSignals()[0]);
    ASSERT_TRUE(waitForPackets(reader, packets, patience));

    SizeT count{packets};
    int64_t values[packets]{};
    uint64_t domain[packets]{};
    reader.readWithDomain(values, domain, &count);
    ASSERT_EQ(count, packets);
    for (daq::SizeT i = 1; i < packets; ++i)
        EXPECT_NE(domain[i], domain[i - 1]);
}
