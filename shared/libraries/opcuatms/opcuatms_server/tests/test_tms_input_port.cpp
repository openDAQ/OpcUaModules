#include <coreobjects/authentication_provider_factory.h>
#include <gtest/gtest.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuatms_server/objects/tms_server_input_port.h>
#include <opcuatms_server/objects/tms_server_signal.h>
#include <open62541/daqbsp_nodeids.h>
#include <opendaq/context_factory.h>
#include <opendaq/input_port_factory.h>
#include <opendaq/input_port_ptr.h>
#include <opendaq/logger_factory.h>
#include <opendaq/range_factory.h>
#include <opendaq/scheduler_factory.h>
#include <opendaq/signal_factory.h>
#include <opendaq/tags_private_ptr.h>
#include "test_helpers.h"
#include "test_input_port_notifications.h"
#include "tms_server_test.h"

using namespace daq;
using namespace opcua::tms;
using namespace opcua;

class TmsInputPortTest : public TmsServerObjectTest, public testing::Test
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

    InputPortPtr createInputPort()
    {
        auto port = InputPort(ctx, nullptr, "Port");
        port.getTags().asPtr<ITagsPrivate>().add("Port");
        return port;
    }
};

TEST_F(TmsInputPortTest, Create)
{
    InputPortPtr inputPort = createInputPort();
    auto tmsInputPort = TmsServerInputPort(inputPort, this->getServer(), ctx, tmsCtx);
}

TEST_F(TmsInputPortTest, Register)
{
    InputPortPtr inputPort = createInputPort();
    auto serverInputPort = TmsServerInputPort(inputPort, this->getServer(), ctx, tmsCtx);
    auto nodeId = serverInputPort.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));
}

TEST_F(TmsInputPortTest, ConnectedToReference)
{
    const auto logger = Logger();
    const auto scheduler = Scheduler(logger);
    const auto authenticationProvider = AuthenticationProvider();
    const auto context = Context(scheduler, logger, nullptr, nullptr, authenticationProvider);

    SignalPtr signal = Signal(context, nullptr, "sig");

    auto serverSignal = TmsServerSignal(signal, this->getServer(), ctx, tmsCtx);
    auto signalNodeId = serverSignal.registerOpcUaNode();

    InputPortNotificationsPtr inputPortNotification = TestInputPortNotifications();

    InputPortConfigPtr inputPort = InputPort(context, nullptr, "TestInputPort");
    inputPort.setListener(inputPortNotification);
    inputPort.connect(signal);

    auto serverInputPort = TmsServerInputPort(inputPort, this->getServer(), ctx, tmsCtx);
    auto inputPortNodeId = serverInputPort.registerOpcUaNode();

    ASSERT_NO_THROW(serverInputPort.createNonhierarchicalReferences());

    ASSERT_TRUE(this->getClient()->nodeExists(signalNodeId));
    ASSERT_TRUE(this->getClient()->nodeExists(inputPortNodeId));

    OpcUaServerNode inputPortNode(*this->getServer(), inputPortNodeId);
    auto connectedToNodes = inputPortNode.browse(OpcUaNodeId(NAMESPACE_DAQBSP, UA_DAQBSPID_CONNECTEDTOSIGNAL));
    ASSERT_EQ(connectedToNodes.size(), 1u);
    ASSERT_EQ(connectedToNodes[0]->getNodeId(), signalNodeId);
}

TEST_F(TmsInputPortTest, Permissions)
{
    InputPortPtr inputPort = createInputPort();
    auto tmsObj = TmsServerInputPort(inputPort, this->getServer(), ctx, tmsCtx);
    inputPort.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    const auto nodeId = tmsObj.registerOpcUaNode();
    const auto connNodeID = tmsObj.getConnectNodeId();
    const auto disconFbNodeID = tmsObj.getDisconnectNodeId();

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
    lambdaTemplate(nodeId.getPtr(), test_helpers::createSessionExecutor("executor"), true, false, true);    // <-
    lambdaTemplate(nodeId.getPtr(), test_helpers::createSessionAdmin("admin"), true, true, true);

    lambdaTemplate(connNodeID.getPtr(), test_helpers::createSessionCommon("common"), false, false, false);
    lambdaTemplate(connNodeID.getPtr(), test_helpers::createSessionReader("reader"), true, false, false);
    lambdaTemplate(connNodeID.getPtr(), test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaTemplate(connNodeID.getPtr(), test_helpers::createSessionExecutor("executor"), true, false, false);   // <-
    lambdaTemplate(connNodeID.getPtr(), test_helpers::createSessionAdmin("admin"), true, true, true);

    lambdaTemplate(disconFbNodeID.getPtr(), test_helpers::createSessionCommon("common"), false, false, false);
    lambdaTemplate(disconFbNodeID.getPtr(), test_helpers::createSessionReader("reader"), true, false, false);
    lambdaTemplate(disconFbNodeID.getPtr(), test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaTemplate(disconFbNodeID.getPtr(), test_helpers::createSessionExecutor("executor"), true, false, false);    // <-
    lambdaTemplate(disconFbNodeID.getPtr(), test_helpers::createSessionAdmin("admin"), true, true, true);
}
