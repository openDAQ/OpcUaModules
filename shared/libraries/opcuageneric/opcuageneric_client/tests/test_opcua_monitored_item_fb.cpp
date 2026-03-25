#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/common.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include <opcuageneric_client/opcuageneric.h>
#include <testutils/testutils.h>
#include "opcuageneric_client/constants.h"
#include "opcuaservertesthelper.h"
#include "test_daq_test_helper.h"

namespace daq::opcua::generic
{
    class GenericOpcuaMonitoredItemHelper : public DaqTestHelper
    {
    public:
        daq::FunctionBlockPtr fb;
        OpcUaServerTestHelper testHelper;

        void CreateMonitoredItemFB(std::string nodeId, uint32_t index, uint32_t interval = 100)
        {
            auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID, nodeId);
            config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, index);
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

    EXPECT_EQ(defaultConfig.getAllProperties().getCount(), 4u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE).getValueType(), CoreType::ctInt);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE).asPtr<IInteger>(),
              static_cast<int>(OpcUaMonitoredItemFbImpl::NodeIDType::String));
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID_TYPE).getVisible());

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_NODE_ID));
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID).getValueType(), CoreType::ctString);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID).asPtr<IString>().getLength(), 0u);
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_NODE_ID).getVisible());

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
        config.addProperty(StringProperty(PROPERTY_NAME_OPCUA_NODE_ID, String("unknownNodeId")));
        ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
        EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
        ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), errStatus());
        device.removeFunctionBlock(fb);
    }

    {
        auto config = PropertyObject();
        config.addProperty(StringProperty(PROPERTY_NAME_OPCUA_NODE_ID, String(".i32")));
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
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID, ".i32");
    config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, 1);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, 100);
    ASSERT_NO_THROW(fb = device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config));
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 2u);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
}

TEST_P(GenericOpcuaMonitoredItemPTest, ReadValue)
{
    constexpr uint32_t interval = 50;
    StartUp();
    auto param = GetParam();

    CreateMonitoredItemFB(param.first.getIdentifier(), param.first.getNamespaceIndex(), interval);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
    std::visit(
        [&](auto& templateParam)
        {
            using T = std::decay_t<decltype(templateParam)>;

            OpcUaVariant variant;
            if constexpr (std::is_same_v<T, std::string>)
                variant = OpcUaVariant(templateParam.c_str());
            else
                variant.setScalar(templateParam);

            ASSERT_NO_THROW(testHelper.writeNode(param.first, variant));

            std::this_thread::sleep_for(std::chrono::milliseconds(interval * 3));
            ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"), okStatus());
            auto val = fb.getSignals()[0].getLastValue();

            if constexpr (std::is_same_v<T, double>)
                ASSERT_DOUBLE_EQ(val, templateParam);
            else if constexpr (std::is_same_v<T, float>)
                ASSERT_FLOAT_EQ(val, templateParam);
            else
                ASSERT_EQ(val, templateParam);
        },
        param.second);
}

INSTANTIATE_TEST_SUITE_P(ReadNumericValue,
                         GenericOpcuaMonitoredItemPTest,
                         ::testing::Values(std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui8"), H{uint8_t{88}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i8"), H{int8_t{-88}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui16"), H{uint16_t{456}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i16"), H{int16_t{-456}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui32"), H{uint32_t{85535}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i32"), H{int32_t{-85535}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".ui64"), H{uint64_t{8055535}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".i64"), H{int64_t{-8055535}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".d"), H{double{123.456789}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".f"), H{float{float(1) / 3}}},
                                           std::pair<OpcUaNodeId, H>{OpcUaNodeId(1, ".s"), H{std::string{"String with a value"}}}),
                         ParamNameGenerator);