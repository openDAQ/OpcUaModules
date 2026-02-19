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

#define EXPECT_THROW_WITH_STATUS(statement, status) \
try { \
        statement; \
        FAIL() << "Expected exception!"; \
} catch (const OpcUaException& e) { \
        EXPECT_EQ(status, e.getStatusCode()); \
} catch (...) { \
        FAIL() << "Wrong exception type thrown"; \
}

using namespace daq;
using namespace opcua::tms;
using namespace opcua;
using namespace std::chrono_literals;

class TmsUserAccessTest : public TmsServerObjectTest
{
public:
    void SetUp() override
    {
        testing::Test::SetUp();

        server = std::make_shared<daq::opcua::OpcUaServer>();
        server->setPort(4840);
        server->setAllowBrowsingNodeCallback(daq::opcua::tms::TmsServerObject::allowBrowsingNodeCallback);
        server->setGetUserAccessLevelCallback(daq::opcua::tms::TmsServerObject::getUserAccessLevelCallback);
        server->setGetUserRightsMaskCallback(daq::opcua::tms::TmsServerObject::getUserRightsMaskCallback);
        server->setGetUserExecutableCallback(daq::opcua::tms::TmsServerObject::getUserExecutableCallback);
        server->setAuthenticationProvider(StaticAuthenticationProvider(true, test_helpers::CreateUsers()));
        server->start();

        client = CreateAndConnectTestClient();

        ctx = daq::NullContext();
        tmsCtx = std::make_shared<daq::opcua::tms::TmsServerContext>(ctx, nullptr);
    }
    void TearDown() override
    {
        ctx = nullptr;
        tmsCtx = nullptr;

        client.reset();
        server.reset();
        testing::Test::TearDown();
    }
};

TEST_F(TmsUserAccessTest, ConnectAnonymousNotAllowed)
{
    server->setAuthenticationProvider(StaticAuthenticationProvider(false, test_helpers::CreateUsers()));

    auto instance = test_helpers::SetupInstance(false);
    auto device = instance.getRootDevice();

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();

    EXPECT_THROW_WITH_STATUS(auto client = CreateAndConnectTestClient(), UA_STATUSCODE_BADUSERACCESSDENIED);
}

TEST_F(TmsUserAccessTest, ConnectAnonymousAllowed)
{
    auto instance = test_helpers::SetupInstance(true);
    auto device = instance.getRootDevice();

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();

    ASSERT_NO_THROW(auto client = CreateAndConnectTestClient());
}

TEST_F(TmsUserAccessTest, ConnectUser)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();

    ASSERT_NO_THROW(auto client = CreateAndConnectTestClient("commonUser", "commonUserPass"));
}

TEST_F(TmsUserAccessTest, ConnectWrongUser)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();

    EXPECT_THROW_WITH_STATUS(auto client = CreateAndConnectTestClient("wrongUser", "wrongUserPass"), UA_STATUSCODE_BADUSERACCESSDENIED);
}

TEST_F(TmsUserAccessTest, ConnectWrongPassword)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();

    EXPECT_THROW_WITH_STATUS(auto client = CreateAndConnectTestClient("commonUser", "wrongUserPass"), UA_STATUSCODE_BADUSERACCESSDENIED);
}

TEST_F(TmsUserAccessTest, AccessBrowse)
{
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    ASSERT_NO_THROW(client = CreateAndConnectTestClient("commonUser", "commonUserPass"));

    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));


    EXPECT_THROW_WITH_STATUS(this->browseNode(nodeId), UA_STATUSCODE_BADUSERACCESSDENIED);

    //auto srValue = this->getServer()->readValue(sampleRateNodeId);
}

