#include <gtest/gtest.h>
#include <opcuatms_server/objects/tms_server_daqserver_component.h>
#include "tms_server_test.h"
#include <opendaq/mock/advanced_components_setup_utils.h>
#include "test_helpers.h"

using namespace daq;
using namespace opcua::tms;
using namespace opcua;

class TmsDaqServerComponentTest : public TmsServerObjectTest, public testing::Test
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

    ServerPtr createDaqServerComponent(const ContextPtr& context, const StringPtr& localId = "srv")
    {
        const auto daqServerComponent = createWithImplementation<IServer, test_utils::MockSrvImpl>(context, nullptr, localId);
        return daqServerComponent;
    }
};

TEST_F(TmsDaqServerComponentTest, Create)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx);
    auto tmsServerDaqServerComponent = TmsServerDaqServerComponent(serverDaqServerComponent, this->getServer(), ctx, tmsCtx);
}

TEST_F(TmsDaqServerComponentTest, Register)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx);
    auto tmsServerDaqServerComponent = TmsServerDaqServerComponent(serverDaqServerComponent, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsServerDaqServerComponent.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));
}

TEST_F(TmsDaqServerComponentTest, Permissions)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx);
    serverDaqServerComponent.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    auto tmsObj = TmsServerDaqServerComponent(serverDaqServerComponent, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsObj.registerOpcUaNode();
    const auto enableDiscoveryNodeID = tmsObj.getEnableDiscoveryNodeId();
    const auto disableDiscoveryNodeID = tmsObj.getDisableDiscoveryNodeId();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    auto lambdaTemplate = [&](const UA_NodeId* id, const OpcUaSession& session, bool read, bool write, bool execute)
    {
        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, id, &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, id, &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, id, &session), execute);
    };

    lambdaTemplate(nodeId.getPtr(), test_helpers::createSessionCommon("common"), false, false, false);
    lambdaTemplate(nodeId.getPtr(), test_helpers::createSessionReader("reader"), true, false, false);
    lambdaTemplate(nodeId.getPtr(), test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaTemplate(nodeId.getPtr(), test_helpers::createSessionExecutor("executor"), true, false, true);
    lambdaTemplate(nodeId.getPtr(), test_helpers::createSessionAdmin("admin"), true, true, true);

    lambdaTemplate(enableDiscoveryNodeID.getPtr(), test_helpers::createSessionCommon("common"), false, false, false);
    lambdaTemplate(enableDiscoveryNodeID.getPtr(), test_helpers::createSessionReader("reader"), true, false, false);
    lambdaTemplate(enableDiscoveryNodeID.getPtr(), test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaTemplate(enableDiscoveryNodeID.getPtr(), test_helpers::createSessionExecutor("executor"), true, false, false);
    lambdaTemplate(enableDiscoveryNodeID.getPtr(), test_helpers::createSessionAdmin("admin"), true, true, true);

    lambdaTemplate(disableDiscoveryNodeID.getPtr(), test_helpers::createSessionCommon("common"), false, false, false);
    lambdaTemplate(disableDiscoveryNodeID.getPtr(), test_helpers::createSessionReader("reader"), true, false, false);
    lambdaTemplate(disableDiscoveryNodeID.getPtr(), test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaTemplate(disableDiscoveryNodeID.getPtr(), test_helpers::createSessionExecutor("executor"), true, false, false);
    lambdaTemplate(disableDiscoveryNodeID.getPtr(), test_helpers::createSessionAdmin("admin"), true, true, true);
}
