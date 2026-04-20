#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coretypes/common.h>
#include <coretypes/search_filter_factory.h>
#include <opcuageneric_client/opcuageneric.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/instance_factory.h>
#include <testutils/testutils.h>
#include "opcuageneric_client/constants.h"
#include "opcuageneric_client/generic_client_device_impl.h"
#include "opcuaservertesthelper.h"
#include "test_daq_test_helper.h"
#include <chrono>
#include <thread>

namespace daq::opcua::generic
{
class GenericOpcuaClientDeviceTest : public testing::Test, public DaqTestHelper
{
public:
    bool waitForConnectionStatus(const std::string& expected, int timeoutMs = 10000)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            try
            {
                auto status = device.getStatusContainer().getStatus("ConnectionStatus");
                if (status == daq::Enumeration("ConnectionStatusType", expected, daqInstance.getContext().getTypeManager()))
                    return true;
            }
            catch (...)
            {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    daq::DevicePtr createDeviceWithShortInterval(const std::string& serverUrl)
    {
        DaqInstanceInit();
        auto client = std::make_shared<daq::opcua::OpcUaClient>(serverUrl);
        device = daq::createWithImplementation<daq::IDevice, daq::opcua::generic::OpcuaGenericClientDeviceImpl>(
            daqInstance.getContext(), daqInstance, nullptr, client, "", "", 100);
        return device;
    }

    daq::FunctionBlockPtr addMonitoredItemFB(const std::string& nodeId, uint32_t nsIndex, uint32_t interval = 100)
    {
        using NT = NodeIDType;
        auto config = device.getAvailableFunctionBlockTypes().get(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME).createDefaultConfig();
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_TYPE, static_cast<int>(NT::String));
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NODE_ID_STRING, nodeId);
        config.setPropertyValue(PROPERTY_NAME_OPCUA_NAMESPACE_INDEX, nsIndex);
        config.setPropertyValue(PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL, interval);
        return device.addFunctionBlock(GENERIC_OPCUA_MONITORED_ITEM_FB_NAME, config);
    }

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

    ASSERT_EQ(defaultConfig.getAllProperties().getCount(), 8u);

    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_USERNAME));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_PASSWORD));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_TS_MODE));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_TYPE));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_NUMERIC));
    ASSERT_TRUE(defaultConfig.hasProperty(PROPERTY_NAME_OPCUA_DEVICE_NAMESPACE_INDEX));

    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_USERNAME).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_PASSWORD).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_TS_MODE).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_TYPE).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING).getValueType(), CoreType::ctString);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_NUMERIC).getValueType(), CoreType::ctInt);
    ASSERT_EQ(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NAMESPACE_INDEX).getValueType(), CoreType::ctInt);

    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_USERNAME), DEFAULT_OPCUA_USERNAME);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_PASSWORD), DEFAULT_OPCUA_PASSWORD);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID), "");
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(),
              static_cast<int>(DomainSource::SourceTimestamp));
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_TYPE).asPtr<IInteger>(),
              static_cast<int>(NodeIDType::String));
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING), "");
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_NUMERIC), 0);
    EXPECT_EQ(defaultConfig.getPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NAMESPACE_INDEX), 0);
}

TEST_F(GenericOpcuaClientDeviceTest, PropertyVisibilityTogglesWithNodeIdType)
{
    using NT = NodeIDType;
    const auto module = CreateModule();
    auto defaultConfig = module.getAvailableDeviceTypes().get("OPCUAGeneric").createDefaultConfig();
    ASSERT_TRUE(defaultConfig.assigned());

    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING).getVisible());
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_NUMERIC).getVisible());

    defaultConfig.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_TYPE, static_cast<int>(NT::Numeric));
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING).getVisible());
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_NUMERIC).getVisible());

    defaultConfig.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_TYPE, static_cast<int>(NT::String));
    EXPECT_TRUE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING).getVisible());
    EXPECT_FALSE(defaultConfig.getProperty(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_NUMERIC).getVisible());
}

TEST_F(GenericOpcuaClientDeviceTest, CreatingDeviceWithDefaultConfig)
{
    const auto instance = DaqInstanceInit();
    const std::string deviceName("open62541-based OPC UA Application");
    daq::GenericDevicePtr<daq::IDevice> device;
    ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842"));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getInfo().getName(), deviceName);
    auto devices = instance.getDevices();
    bool contain = false;
    daq::GenericDevicePtr<daq::IDevice> deviceFromList;
    for (const auto& d : devices)
    {
        contain = (d.getName() == deviceName);
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

    ASSERT_EQ(device.getAllProperties().getCount(), 1u);
    ASSERT_TRUE(device.hasProperty(PROPERTY_NAME_OPCUA_TS_MODE));
}

TEST_F(GenericOpcuaClientDeviceTest, CreatingDeviceWithLocalId)
{
    const auto module = CreateModule();
    const auto instance = DaqInstanceInit();
    const std::string deviceLocalId("myCustomLocalId");
    daq::GenericDevicePtr<daq::IDevice> device;
    auto defaultConfig = module.getAvailableDeviceTypes().get("OPCUAGeneric").createDefaultConfig();
    defaultConfig.setPropertyValue(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, deviceLocalId);
    ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842", defaultConfig));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getLocalId(), deviceLocalId);
    ASSERT_NE(device.getGlobalId().toStdString().find(deviceLocalId), std::string::npos);
}

TEST_F(GenericOpcuaClientDeviceTest, CreatingDeviceWithRootDeviceId)
{
    using NT = NodeIDType;
    using namespace daq::opcua::helper::constants;

    const auto module = CreateModule();
    const auto instance = DaqInstanceInit();

    daq::GenericDevicePtr<daq::IDevice> device;
    auto config = module.getAvailableDeviceTypes().get("OPCUAGeneric").createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_TYPE, static_cast<int>(NT::String));
    config.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING, DI_DEVICE_STRING_ID);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NAMESPACE_INDEX, TEST_NS);
    ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842", config));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    const std::string expectedLocalId = std::string(EXPECTED_MANUFACTURER) + "_" + EXPECTED_SERIAL;
    ASSERT_EQ(device.getLocalId(), expectedLocalId);
    ASSERT_NE(device.getGlobalId().toStdString().find(expectedLocalId), std::string::npos);
}

TEST_F(GenericOpcuaClientDeviceTest, CreatingDeviceWithApplicationUri)
{
    const auto module = CreateModule();
    const auto instance = DaqInstanceInit();
    const std::string deviceLocalId("urn:open62541.server.application");
    daq::GenericDevicePtr<daq::IDevice> device;
    auto defaultConfig = module.getAvailableDeviceTypes().get("OPCUAGeneric").createDefaultConfig();

    ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842", defaultConfig));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    ASSERT_EQ(device.getLocalId(), deviceLocalId);
    ASSERT_NE(device.getGlobalId().toStdString().find(deviceLocalId), std::string::npos);
}

TEST_F(GenericOpcuaClientDeviceTest, CreatingDeviceWithoutApplicationUri)
{
    const auto module = CreateModule();
    const auto instance = DaqInstanceInit();
    testHelper.stop();
    testHelper.onTweakConfig(
        [&](UA_ServerConfig* config)
        {
            UA_ApplicationDescription_clear(&config->applicationDescription);
            config->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;
        });
    testHelper.startServer();
    daq::GenericDevicePtr<daq::IDevice> device;
    auto defaultConfig = module.getAvailableDeviceTypes().get("OPCUAGeneric").createDefaultConfig();

    auto isNumber = [](const std::string& s)
    {
        if (s.empty())
            return false;

        for (char c : s)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return false;
        }
        return true;
    };

    ASSERT_NO_THROW(device = instance.addDevice("daq.opcua.generic://127.0.0.1:4842", defaultConfig));
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", instance.getContext().getTypeManager()));
    const auto& expectedPrefix = GENERIC_OPCUA_CLIENT_DEVICE_NAME;
    ASSERT_NE(device.getLocalId().toStdString().find(expectedPrefix), std::string::npos);

    const auto tmp = std::string(device.getLocalId().toStdString(), device.getLocalId().toStdString().find(expectedPrefix) + strlen(expectedPrefix));
    ASSERT_TRUE(isNumber(tmp));
    ASSERT_NE(device.getGlobalId().toStdString().find(expectedPrefix), std::string::npos);
}

TEST_F(GenericOpcuaClientDeviceTest, RemovingDevice)
{
    const auto instance = DaqInstanceInit();
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

TEST_F(GenericOpcuaClientDeviceTest, CheckDeviceNameLocalId)
{
    StartUp();
    EXPECT_EQ(device.getLocalId().toStdString(), std::string("urn:open62541.server.application"));
    EXPECT_EQ(device.getName().toStdString(), std::string("open62541-based OPC UA Application"));
}

TEST_F(GenericOpcuaClientDeviceTest, ReconnectMonitor_StatusBecomesReconnectingAfterServerStop)
{
    DaqInstanceInit();
    createDeviceWithShortInterval(testHelper.getServerUrl());
    ASSERT_TRUE(waitForConnectionStatus("Connected"));

    testHelper.stop();

    ASSERT_TRUE(waitForConnectionStatus("Reconnecting"));
}

TEST_F(GenericOpcuaClientDeviceTest, ReconnectMonitor_ReconnectsAfterServerRestart)
{
    DaqInstanceInit();
    createDeviceWithShortInterval(testHelper.getServerUrl());
    ASSERT_TRUE(waitForConnectionStatus("Connected"));

    testHelper.stop();
    ASSERT_TRUE(waitForConnectionStatus("Reconnecting"));

    testHelper.startServer();
    ASSERT_TRUE(waitForConnectionStatus("Connected"));
}

TEST_F(GenericOpcuaClientDeviceTest, ReconnectMonitor_StopsCleanlyOnDeviceRemoval)
{
    DaqInstanceInit();
    device = daqInstance.addDevice("daq.opcua.generic://127.0.0.1:4842");
    ASSERT_EQ(device.getStatusContainer().getStatus("ComponentStatus"),
              daq::Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));

    const auto t0 = std::chrono::steady_clock::now();
    ASSERT_NO_THROW(daqInstance.removeDevice(device));
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

    // stopReconnectMonitor() uses cv.notify_all() so join should be near-instant
    ASSERT_LT(elapsedMs, 1000);
}

TEST_F(GenericOpcuaClientDeviceTest, TimestampModeFromConfigIsAppliedToDevice)
{
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    ASSERT_TRUE(device.hasProperty(PROPERTY_NAME_OPCUA_TS_MODE));
    EXPECT_EQ(device.getPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE).asPtr<IInteger>(),
              static_cast<int>(DomainSource::ServerTimestamp));
}

TEST_F(GenericOpcuaClientDeviceTest, TimestampModeChangePropagatesToMultipleFBs)
{
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    auto fb1 = addMonitoredItemFB(".i32", 1);
    auto fb2 = addMonitoredItemFB(".i64", 1);

    ASSERT_EQ(fb1.getSignals(daq::search::Any()).getCount(), 2u);
    ASSERT_EQ(fb2.getSignals(daq::search::Any()).getCount(), 2u);

    device.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DomainSource::None));

    EXPECT_EQ(fb1.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb1.getSignals()[0].getDomainSignal().assigned());
    EXPECT_EQ(fb2.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb2.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaClientDeviceTest, TimestampModeNewFBInheritsCurrentDeviceMode)
{
    StartUp(buildDeviceConfig(DomainSource::SourceTimestamp));

    device.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DomainSource::None));

    auto fb = addMonitoredItemFB(".i32", 1);

    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));
    EXPECT_EQ(fb.getSignals(daq::search::Any()).getCount(), 1u);
    EXPECT_FALSE(fb.getSignals()[0].getDomainSignal().assigned());
}

TEST_F(GenericOpcuaClientDeviceTest, RemovedFBIsIgnoredOnSubsequentTimestampModeChange)
{
    StartUp(buildDeviceConfig(DomainSource::ServerTimestamp));

    auto fb = addMonitoredItemFB(".i32", 1);
    ASSERT_EQ(fb.getStatusContainer().getStatus("ComponentStatus"),
              Enumeration("ComponentStatusType", "Ok", daqInstance.getContext().getTypeManager()));

    ASSERT_NO_THROW(device.removeFunctionBlock(fb));
    ASSERT_NO_THROW(device.setPropertyValue(PROPERTY_NAME_OPCUA_TS_MODE, static_cast<int>(DomainSource::None)));    
}

TEST_F(GenericOpcuaClientDeviceTest, DeviceInfoFilledFromDeviceTypeNode)
{
    using NT = NodeIDType;
    using namespace daq::opcua::helper::constants;

    auto config = CreateModule().getAvailableDeviceTypes().get("OPCUAGeneric").createDefaultConfig();
    config.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_TYPE, static_cast<int>(NT::String));
    config.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NODE_ID_STRING, DI_DEVICE_STRING_ID);
    config.setPropertyValue(PROPERTY_NAME_OPCUA_DEVICE_NAMESPACE_INDEX, TEST_NS);

    ASSERT_NO_THROW(StartUp(config));

    auto info = device.getInfo();
    EXPECT_EQ(info.getManufacturer().toStdString(), EXPECTED_MANUFACTURER);
    EXPECT_EQ(info.getManufacturerUri().toStdString(), EXPECTED_MANUFACTURER_URI);
    EXPECT_EQ(info.getModel().toStdString(), EXPECTED_MODEL);
    EXPECT_EQ(info.getHardwareRevision().toStdString(), EXPECTED_HW_REVISION);
    EXPECT_EQ(info.getSoftwareRevision().toStdString(), EXPECTED_SW_REVISION);
    EXPECT_EQ(info.getDeviceRevision().toStdString(), EXPECTED_DEV_REVISION);
    EXPECT_EQ(info.getSerialNumber().toStdString(), EXPECTED_SERIAL);
    EXPECT_EQ(info.getProductCode().toStdString(), EXPECTED_PRODUCT_CODE);
    EXPECT_EQ(info.getDeviceManual().toStdString(), EXPECTED_DEVICE_MANUAL);
    EXPECT_EQ(info.getDeviceClass().toStdString(), EXPECTED_DEVICE_CLASS);
    EXPECT_EQ(info.getProductInstanceUri().toStdString(), EXPECTED_PRODUCT_INSTANCE_URI);
    EXPECT_EQ(info.getAssetId().toStdString(), EXPECTED_ASSET_ID);
    EXPECT_EQ(info.getName().toStdString(), EXPECTED_COMPONENT_NAME);
    EXPECT_EQ(info.getRevisionCounter(), EXPECTED_REVISION_CNT);
}
