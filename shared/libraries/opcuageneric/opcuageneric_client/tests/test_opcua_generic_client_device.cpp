#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/common.h>
#include <opcuageneric_client/opcuageneric.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/instance_factory.h>
#include <testutils/testutils.h>
#include "opcuageneric_client/constants.h"
#include "opcuaservertesthelper.h"
#include "test_daq_test_helper.h"

namespace daq::opcua::generic
{
class GenericOpcuaClientDeviceTest : public testing::Test, public DaqTestHelper
{
protected:
    void SetUp() override
    {
        testing::Test::SetUp();
        testHelper.startServer();
    }
    void TearDown() override
    {
        testHelper.stop();
        testing::Test::TearDown();
    }
    OpcUaServerTestHelper testHelper;
};
} // namespace daq::modules::mqtt_streaming_module

using namespace daq;
using namespace daq::opcua::generic;

TEST_F(GenericOpcuaClientDeviceTest, DefaultDeviceConfig)
{
    const auto module = CreateModule();

    DictPtr<IString, IDeviceType> deviceTypes;
    ASSERT_NO_THROW(deviceTypes = module.getAvailableDeviceTypes());
    ASSERT_EQ(deviceTypes.getCount(), 1u);

    ASSERT_TRUE(deviceTypes.hasKey("OPCUAGeneric"));
    auto defaultConfig = deviceTypes.get("OPCUAGeneric").createDefaultConfig();
    ASSERT_TRUE(defaultConfig.assigned());

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 5u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_HOST));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_PORT));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_PATH));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_USERNAME));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_PASSWORD));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_HOST).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_PORT).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_PATH).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_USERNAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_PASSWORD).getValueType(), CoreType::ctString);

    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_HOST), DEFAULT_OPCUA_HOST);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_PORT), DEFAULT_OPCUA_PORT);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_PATH), DEFAULT_OPCUA_PATH);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_USERNAME), DEFAULT_OPCUA_USERNAME);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_PASSWORD), DEFAULT_OPCUA_PASSWORD);
}

TEST_F(GenericOpcuaClientDeviceTest, CreatingDeviceWithDefaultConfig)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842"));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getInfo().getName(), GENERIC_OPCUA_CLIENT_DEVICE_NAME);
    auto devices = instance.getDevices();
    bool contain = false;
    daq::GenericDevicePtr<daq::IDevice> deviceFromList;
    for (const auto& d : devices)
    {
        contain = (d.getName() == GENERIC_OPCUA_CLIENT_DEVICE_NAME);
        if (contain)
        {
            deviceFromList = d;
            break;
        }
    }
    ASSERT_TRUE(contain);
    ASSERT_TRUE(deviceFromList.assigned());
    ASSERT_EQ(deviceFromList.getInfo().getName(), device.getInfo().getName());
    ASSERT_TRUE(deviceFromList == device);
}

TEST_F(GenericOpcuaClientDeviceTest, RemovingDevice)
{
    const auto instance = Instance();
    daq::GenericDevicePtr<daq::IDevice> device;
    {
        ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842"));
        ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
        ASSERT_NO_THROW(instance.removeDevice(device));
    }

    {
        ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842"));
        ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
                  Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
        ASSERT_NO_THROW(instance.removeDevice(device));
    }
}

TEST_F(GenericOpcuaClientDeviceTest, CheckDeviceFunctionalBlocks)
{
    StartUp();
    daq::DictPtr<daq::IString, daq::IFunctionBlockType> fbTypes;
    ASSERT_NO_THROW(fbTypes = device.getAvailableFunctionBlockTypes());
    ASSERT_GE(fbTypes.getCount(), 1);
    ASSERT_TRUE(fbTypes.hasKey(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME));
}
