#include <coreobjects/property_object_factory.h>
#include <coreobjects/unit_factory.h>
#include <gtest/gtest.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuatms_server/objects/tms_server_device.h>
#include <open62541/daqdevice_nodeids.h>
#include <opendaq/instance_factory.h>
#include <opendaq/mock/mock_device_module.h>
#include <opendaq/mock/mock_fb_module.h>
#include <opendaq/mock/mock_physical_device.h>
#include "test_helpers.h"
#include "tms_server_test.h"

using namespace daq;
using namespace opcua::tms;
using namespace opcua;
using namespace std::chrono_literals;

class TmsDeviceTest : public TmsServerObjectTest, public testing::Test
{
public:
    void SetUp() override
    {
        TmsServerObjectTest::Init();
    }

    void TearDown() override
    {
        TmsServerObjectTest::Clear();
    }
};

TEST_F(TmsDeviceTest, Create)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsDevice = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
}

TEST_F(TmsDeviceTest, Register)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsDevice = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsDevice.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));
}

TEST_F(TmsDeviceTest, SubDevices)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsDevice = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsDevice.registerOpcUaNode();

    ASSERT_EQ(test_helpers::BrowseSubDevices(client, nodeId).size(), 2u);
}

TEST_F(TmsDeviceTest, FunctionBlock)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsDevice = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsDevice.registerOpcUaNode();

    auto functionBlockNodeId = getChildNodeId(nodeId, "FB");
    ASSERT_EQ(test_helpers::BrowseFunctionBlocks(client, functionBlockNodeId).size(), 1u);
}

TEST_F(TmsDeviceTest, Property)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    const auto sampleRateProp =
        FloatPropertyBuilder("SampleRate", 100.0).setUnit(Unit("Hz")).setMinValue(1.0).setMaxValue(1000000.0).build();

    device.addProperty(sampleRateProp);

    auto tmsDevice = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsDevice.registerOpcUaNode();

    auto sampleRateNodeId = this->getChildNodeId(nodeId, "SampleRate");
    ASSERT_FALSE(sampleRateNodeId.isNull());

    auto srValue = this->getServer()->readValue(sampleRateNodeId);
    ASSERT_TRUE(srValue.hasScalarType<UA_Double>());
    ASSERT_DOUBLE_EQ(srValue.readScalar<UA_Double>(), 100.0);

    device.setPropertyValue("SampleRate", 14.0);

    srValue = this->getServer()->readValue(sampleRateNodeId);
    ASSERT_TRUE(srValue.hasScalarType<UA_Double>());
    ASSERT_DOUBLE_EQ(srValue.readScalar<UA_Double>(), 14.0);

    this->getServer()->writeValue(sampleRateNodeId, OpcUaVariant(22.2));

    srValue = this->getServer()->readValue(sampleRateNodeId);
    ASSERT_TRUE(srValue.hasScalarType<UA_Double>());
    ASSERT_DOUBLE_EQ(srValue.readScalar<UA_Double>(), 22.2);
}

TEST_F(TmsDeviceTest, Components)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsDevice = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsDevice.registerOpcUaNode();

    auto devices = test_helpers::BrowseSubDevices(client, nodeId);
    auto componentA = getChildNodeId(devices[1], "componentA");
    ASSERT_FALSE(componentA.isNull());
    auto componentA1 = getChildNodeId(componentA, "componentA1");
    ASSERT_FALSE(componentA1.isNull());
    auto componentB = getChildNodeId(devices[1], "componentB");
    ASSERT_FALSE(componentB.isNull());
}

TEST_F(TmsDeviceTest, Permissions)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    const auto addFbNodeID = tmsObj.getAddFunctionBlockNodeId();
    const auto removeFbNodeID = tmsObj.getRemoveFunctionBlockNodeId();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    auto lambdaMain = [&](const OpcUaSession& session, bool read, bool write, bool execute)
    {
        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, nodeId.getPtr(), &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, nodeId.getPtr(), &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, nodeId.getPtr(), &session), execute);
    };

    auto lambdaSpecial = [&](const OpcUaSession& session, bool read, bool write, bool execute)
    {
        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, addFbNodeID.getPtr(), &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, addFbNodeID.getPtr(), &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, addFbNodeID.getPtr(), &session), execute);

        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, removeFbNodeID.getPtr(), &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, removeFbNodeID.getPtr(), &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, removeFbNodeID.getPtr(), &session), execute);
    };

    lambdaMain(test_helpers::createSessionCommon("common"), false, false, false);
    lambdaMain(test_helpers::createSessionReader("reader"), true, false, false);
    lambdaMain(test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaMain(test_helpers::createSessionExecutor("executor"), true, false, true);
    lambdaMain(test_helpers::createSessionAdmin("admin"), true, true, true);

    lambdaSpecial(test_helpers::createSessionCommon("common"), false, false, false);
    lambdaSpecial(test_helpers::createSessionReader("reader"), true, false, false);
    lambdaSpecial(test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaSpecial(test_helpers::createSessionExecutor("executor"), true, false, false);
    lambdaSpecial(test_helpers::createSessionAdmin("admin"), true, true, true);
}

