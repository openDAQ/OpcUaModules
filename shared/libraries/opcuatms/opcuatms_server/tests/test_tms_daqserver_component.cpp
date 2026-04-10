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

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    auto lambdaMain = [&](const OpcUaSession& session, bool read, bool write, bool execute)
    {
        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, nodeId.getPtr(), &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, nodeId.getPtr(), &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, nodeId.getPtr(), &session), execute);
    };

    lambdaMain(test_helpers::createSessionCommon("common"), false, false, false);
    lambdaMain(test_helpers::createSessionReader("reader"), true, false, false);
    lambdaMain(test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaMain(test_helpers::createSessionExecutor("executor"), true, false, true);
    lambdaMain(test_helpers::createSessionAdmin("admin"), true, true, true);
}
