#include <gtest/gtest.h>
#include <opcuashared/opcuacommon.h>
#include <opcuatms_server/objects/tms_server_device.h>
#include <opendaq/instance_factory.h>
#include <tms_object_test.h>
#include "open62541/client_highlevel.h"
#include "test_helpers.h"
#include "tms_server_test.h"

#define EXPECT_THROW_WITH_STATUS(statement, status) \
    try                                             \
    {                                               \
        statement;                                  \
        FAIL() << "Expected exception!";            \
    }                                               \
    catch (const OpcUaException& e)                 \
    {                                               \
        EXPECT_EQ(status, e.getStatusCode());       \
    }                                               \
    catch (...)                                     \
    {                                               \
        FAIL() << "Wrong exception type thrown";    \
    }

using namespace daq;
using namespace opcua::tms;

class TmsServerObjectTestWithParameterizedServer : public TmsServerObjectTest
{
public:
    void Init() override
    {
        using namespace daq::opcua::tms;
        server = std::make_shared<daq::opcua::OpcUaServer>();
        server->setPort(4840);
        server->setAllowBrowsingNodeCallback(TmsServerObject::allowBrowsingNodeCallback);
        server->setGetUserAccessLevelCallback(TmsServerObject::getUserAccessLevelCallback);
        server->setGetUserRightsMaskCallback(TmsServerObject::getUserRightsMaskCallback);
        server->setGetUserExecutableCallback(TmsServerObject::getUserExecutableCallback);
        server->setAuthenticationProvider(StaticAuthenticationProvider(true, test_helpers::CreateUsers()));
        server->start();

        client = CreateAndConnectTestClient();
        ctx = daq::NullContext();
        tmsCtx = std::make_shared<daq::opcua::tms::TmsServerContext>(ctx, nullptr);
    }
};

class TmsUserAccessTest : public TmsServerObjectTestWithParameterizedServer, public testing::Test
{
public:
    void SetUp() override
    {
        TmsServerObjectTestWithParameterizedServer::Init();
    }
    void TearDown() override
    {
        TmsServerObjectTestWithParameterizedServer::Clear();
    }
};
namespace
{
    struct UserAccessTestParams
    {
        std::string login;
        std::string password;
        bool connectAllowed;
        bool readPermission;
        bool writePermission;
        bool executePermission;
    };
}

class ConnectionPTest : public ::testing::TestWithParam<UserAccessTestParams>, public TmsServerObjectTestWithParameterizedServer
{
public:
    void SetUp() override
    {
        TmsServerObjectTestWithParameterizedServer::Init();
    }
    void TearDown() override
    {
        TmsServerObjectTestWithParameterizedServer::Clear();
    }
};

class AsseccPTest : public ::testing::TestWithParam<UserAccessTestParams>, public TmsServerObjectTestWithParameterizedServer
{
public:
    void SetUp() override
    {
        TmsServerObjectTestWithParameterizedServer::Init();
        adminClient = CreateAndConnectTestClient("adminUser", "adminUserPass");
    }
    void TearDown() override
    {
        TmsServerObjectTestWithParameterizedServer::Clear();
        adminClient.reset();
    }

    OpcUaNodeId getChildNodeIdAsAdmin(const OpcUaNodeId& parent, const std::string& browseName)
    {
        OpcUaObject<UA_BrowseRequest> br;
        br->requestedMaxReferencesPerNode = 0;
        br->nodesToBrowse = UA_BrowseDescription_new();
        br->nodesToBrowseSize = 1;
        br->nodesToBrowse[0].nodeId = parent.copyAndGetDetachedValue();
        br->nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

        OpcUaObject<UA_BrowseResponse> result = UA_Client_Service_browse(adminClient->getUaClient(), *br);

        if (result->resultsSize == 0)
            return OpcUaNodeId(UA_NODEID_NULL);

        auto references = result->results[0].references;
        auto referenceCount = result->results[0].referencesSize;

        for (size_t i = 0; i < referenceCount; i++)
        {
            auto reference = references[i];
            std::string refBrowseName = daq::opcua::utils::ToStdString(reference.browseName.name);
            if (refBrowseName == browseName)
                return OpcUaNodeId(reference.nodeId.nodeId);
        }

        return OpcUaNodeId(UA_NODEID_NULL);
    }

    auto readWriteMask(daq::opcua::OpcUaNodeId nodeId)
    {
        UA_UInt32 value;
        CheckStatusCodeException(UA_Client_readWriteMaskAttribute(client->getUaClient(), *nodeId, &value));
        return value;
    }

    auto readUserWriteMask(daq::opcua::OpcUaNodeId nodeId)
    {
        UA_UInt32 value;
        CheckStatusCodeException(UA_Client_readUserWriteMaskAttribute(client->getUaClient(), *nodeId, &value));
        return value;
    }

protected:
    daq::opcua::OpcUaClientPtr adminClient;
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

TEST_P(ConnectionPTest, UserConnectTest)
{
    const auto userParams = GetParam();
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    if (userParams.connectAllowed)
    {
        ASSERT_NO_THROW(auto client = CreateAndConnectTestClient(userParams.login, userParams.password));
    }
    else
    {
        EXPECT_THROW_WITH_STATUS(auto client = CreateAndConnectTestClient(userParams.login, userParams.password),
                                 UA_STATUSCODE_BADUSERACCESSDENIED);
    }
}

INSTANTIATE_TEST_SUITE_P(UserParameters,
                         ConnectionPTest,
                         ::testing::Values(UserAccessTestParams{"", "", true},
                                           UserAccessTestParams{"commonUser", "commonUserPass", true},
                                           UserAccessTestParams{"wrongUser", "wrongUserPass", false},
                                           UserAccessTestParams{"commonUser", "wrongUserPass", false}));

TEST_P(AsseccPTest, AccessBrowse)
{
    const auto userParams = GetParam();
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    ASSERT_NO_THROW(client = CreateAndConnectTestClient(userParams.login, userParams.password));

    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    if (userParams.readPermission)
    {
        ASSERT_NO_THROW(this->browseNode(nodeId));
    }
    else
    {
        EXPECT_THROW_WITH_STATUS(this->browseNode(nodeId), UA_STATUSCODE_BADUSERACCESSDENIED);
    }
}

TEST_P(AsseccPTest, AccessRead)
{
    constexpr double VALUE_0 = 13.0;
    const auto userParams = GetParam();
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();
    const auto freqProp = FloatPropertyBuilder("Frequency", VALUE_0).setUnit(Unit("Hz")).setMinValue(1).setMaxValue(1000.0).build();

    device.addProperty(freqProp);
    ASSERT_NO_THROW(client = CreateAndConnectTestClient(userParams.login, userParams.password));
    decltype(client) adminClient;
    ASSERT_NO_THROW(adminClient = CreateAndConnectTestClient("adminUser", "adminUserPass"));

    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    const auto freqNodeId = this->getChildNodeIdAsAdmin(nodeId, "Frequency");
    ASSERT_NE(freqNodeId, OpcUaNodeId(UA_NODEID_NULL));
    if (userParams.readPermission)
    {
        OpcUaVariant res;
        ASSERT_NO_THROW(res = getClient()->readValue(freqNodeId));
        ASSERT_TRUE(res.hasScalarType<UA_Double>());
        ASSERT_DOUBLE_EQ(res.readScalar<UA_Double>(), VALUE_0);
    }
    else
    {
        EXPECT_THROW_WITH_STATUS(getClient()->readValue(freqNodeId), UA_STATUSCODE_BADUSERACCESSDENIED);
    }
}

TEST_P(AsseccPTest, AccessWriteValue)
{
    constexpr double VALUE_0 = 13.0;
    constexpr double VALUE_1 = 213.0;
    const auto userParams = GetParam();
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();
    const auto freqProp = FloatPropertyBuilder("Frequency", VALUE_0).setUnit(Unit("Hz")).setMinValue(1).setMaxValue(1000.0).build();

    device.addProperty(freqProp);
    ASSERT_NO_THROW(client = CreateAndConnectTestClient(userParams.login, userParams.password));

    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    const auto freqNodeId = this->getChildNodeIdAsAdmin(nodeId, "Frequency");
    ASSERT_NE(freqNodeId, OpcUaNodeId(UA_NODEID_NULL));
    if (userParams.writePermission)
    {
        OpcUaVariant res;
        // read initial value
        ASSERT_NO_THROW(res = getClient()->readValue(freqNodeId));
        ASSERT_TRUE(res.hasScalarType<UA_Double>());
        ASSERT_DOUBLE_EQ(res.readScalar<UA_Double>(), VALUE_0);
        // write new value
        ASSERT_NO_THROW(getClient()->writeValue(freqNodeId, OpcUaVariant(VALUE_1)));
        // read back new value
        ASSERT_NO_THROW(res = getClient()->readValue(freqNodeId));
        ASSERT_TRUE(res.hasScalarType<UA_Double>());
        ASSERT_DOUBLE_EQ(res.readScalar<UA_Double>(), VALUE_1);
    }
    else
    {
        EXPECT_THROW_WITH_STATUS(getClient()->writeValue(freqNodeId, OpcUaVariant(VALUE_1)), UA_STATUSCODE_BADUSERACCESSDENIED);
    }
}

TEST_P(AsseccPTest, AccessWriteAttr)
{
    const std::string DISPLAY_NAME = "newDisptayName";
    const std::string DESCRIPTION = "newDescription";
    const auto userParams = GetParam();
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    ASSERT_NO_THROW(client = CreateAndConnectTestClient(userParams.login, userParams.password));

    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    // read write masks
    uint32_t writeMask = 0;
    uint32_t userWriteMask = 0;
    ASSERT_NO_THROW(writeMask = readWriteMask(nodeId));
    ASSERT_TRUE((writeMask & UA_WRITEMASK_DISPLAYNAME) != 0);
    ASSERT_TRUE((writeMask & UA_WRITEMASK_DESCRIPTION) != 0);
    ASSERT_NO_THROW(userWriteMask = readUserWriteMask(nodeId));

    if (userParams.writePermission)
    {
        {
            ASSERT_TRUE((userWriteMask & UA_WRITEMASK_DISPLAYNAME) != 0);
            // write display name
            ASSERT_NO_THROW(getClient()->writeDisplayName(nodeId, DISPLAY_NAME));
            // read back display name
            std::string newDisplayName;
            ASSERT_NO_THROW(newDisplayName = getClient()->readDisplayName(nodeId));
            ASSERT_EQ(newDisplayName, DISPLAY_NAME);
        }
        {
            ASSERT_TRUE((userWriteMask & UA_WRITEMASK_DESCRIPTION) != 0);
            // write description
            ASSERT_NO_THROW(getClient()->writeDescription(nodeId, DESCRIPTION));
            // read back description
            std::string newDescription;
            ASSERT_NO_THROW(newDescription = getClient()->readDescription(nodeId));
            ASSERT_EQ(newDescription, DESCRIPTION);
        }
    }
    else
    {
        ASSERT_FALSE((userWriteMask & UA_WRITEMASK_DISPLAYNAME) != 0);
        ASSERT_FALSE((userWriteMask & UA_WRITEMASK_DESCRIPTION) != 0);
        EXPECT_THROW_WITH_STATUS(getClient()->writeDisplayName(nodeId, DISPLAY_NAME), UA_STATUSCODE_BADUSERACCESSDENIED);
        EXPECT_THROW_WITH_STATUS(getClient()->writeDescription(nodeId, DESCRIPTION), UA_STATUSCODE_BADUSERACCESSDENIED);
    }
}

TEST_P(AsseccPTest, AccessExecute)
{
    const auto userParams = GetParam();
    auto instance = test_helpers::SetupInstance();
    auto device = instance.getRootDevice();

    ASSERT_NO_THROW(client = CreateAndConnectTestClient(userParams.login, userParams.password));

    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerDevice(device, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    for (const auto& methodName : {"BeginUpdate", "EndUpdate"})
    {
        const auto methodNodeId = this->getChildNodeIdAsAdmin(nodeId, methodName);
        ASSERT_NE(methodNodeId, OpcUaNodeId(UA_NODEID_NULL));
        if (userParams.executePermission && userParams.writePermission)
        {
            OpcUaCallMethodRequest callRequest = OpcUaCallMethodRequest(methodNodeId, nodeId, 0);
            OpcUaObject<UA_CallMethodResult> callResult = getClient()->callMethod(callRequest);
            ASSERT_TRUE(OPCUA_STATUSCODE_SUCCEEDED(callResult->statusCode));
            ASSERT_TRUE(callResult->outputArgumentsSize == 0);
        }
        else
        {
            const auto callResult = getClient()->callMethod(OpcUaCallMethodRequest(methodNodeId, nodeId, 0));
            ASSERT_FALSE(OPCUA_STATUSCODE_SUCCEEDED(callResult->statusCode));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(UserParameters,
                         AsseccPTest,
                         ::testing::Values(UserAccessTestParams{"", "", true, false, false, false},
                                           UserAccessTestParams{"commonUser", "commonUserPass", true, false, false, false},
                                           UserAccessTestParams{"readerUser", "readerUserPass", true, true, false, false},
                                           UserAccessTestParams{"writerUser", "writerUserPass", true, true, true, false},
                                           UserAccessTestParams{"executorUser", "executorUserPass", true, true, false, true},
                                           UserAccessTestParams{"adminUser", "adminUserPass", true, true, true, true}));
