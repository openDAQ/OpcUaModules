#include <gtest/gtest.h>
#include <opcua_simple_server/simple_server.h>
#include <generic_opcua_test_helper.h>
#include <test_helpers.h>
#include <opendaq/search_filter_factory.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuashared/opcuaendpoint.h>

using SimpleServerTest = testing::Test;

using namespace daq;
using namespace daq::opcua;
using namespace test_helpers;


TEST_F(SimpleServerTest, Create)
{
    auto daqInstance = SetupInstance();
    ASSERT_NO_THROW(GenericServer server(daqInstance));
}

TEST_F(SimpleServerTest, CreateFromDeviceAndContext)
{
    auto daqInstance = SetupInstance();
    ASSERT_NO_THROW(GenericServer server(daqInstance.getRootDevice(), daqInstance.getContext()));
}

TEST_F(SimpleServerTest, StartThrowsWithNullDevice)
{
    auto daqInstance = SetupInstance();
    DevicePtr nullDevice;
    GenericServer server(nullDevice, daqInstance.getContext());
    ASSERT_THROW(server.start(), std::exception);
}

TEST_F(SimpleServerTest, StartThrowsWithNullContext)
{
    auto daqInstance = SetupInstance();
    ContextPtr nullContext;
    GenericServer server(daqInstance.getRootDevice(), nullContext);
    ASSERT_THROW(server.start(), std::exception);
}

TEST_F(SimpleServerTest, StartStop)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    ASSERT_NO_THROW(server.start());
    ASSERT_NO_THROW(server.stop());
}

TEST_F(SimpleServerTest, StopBeforeStart)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    ASSERT_NO_THROW(server.stop());
}

TEST_F(SimpleServerTest, DoubleStop)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    server.start();
    ASSERT_NO_THROW(server.stop());
    ASSERT_NO_THROW(server.stop());
}

TEST_F(SimpleServerTest, DoubleStart)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    server.start();
    ASSERT_THROW(server.start(), std::exception);
    ASSERT_NO_THROW(server.stop());
}

TEST_F(SimpleServerTest, Connect)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    server.start();

    auto client = OpcuaServerHelper::CreateAndConnectTestClient();
    ASSERT_TRUE(client->isConnected());
}

TEST_F(SimpleServerTest, Restart)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);

    server.start();
    auto client1 = OpcuaServerHelper::CreateAndConnectTestClient();
    ASSERT_TRUE(client1->isConnected());
    client1->disconnect();
    server.stop();

    server.start();
    auto client2 = OpcuaServerHelper::CreateAndConnectTestClient();
    ASSERT_TRUE(client2->isConnected());
    client2->disconnect();
    server.stop();
}

TEST_F(SimpleServerTest, CustomPort)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    server.setOpcUaPort(4841);
    server.start();

    OpcUaEndpoint endpoint("opc.tcp://127.0.0.1:4841");
    auto client = std::make_shared<OpcUaClient>(endpoint);
    ASSERT_NO_THROW(client->connect());
    ASSERT_TRUE(client->isConnected());

    server.stop();
}

TEST_F(SimpleServerTest, AnonymousConnectionBlocked)
{
    auto daqInstance = SetupInstance(false);
    GenericServer server(daqInstance);
    server.start();

    OpcUaEndpoint endpoint("opc.tcp://127.0.0.1:4840");
    auto client = std::make_shared<OpcUaClient>(endpoint);
    ASSERT_THROW(client->connect(), std::exception);

    server.stop();
}

TEST_F(SimpleServerTest, AuthenticatedConnectionSucceeds)
{
    auto daqInstance = SetupInstance(false);
    GenericServer server(daqInstance);
    server.start();

    OpcUaEndpoint endpoint("opc.tcp://127.0.0.1:4840", "readerUser", "readerUserPass");
    auto client = std::make_shared<OpcUaClient>(endpoint);
    ASSERT_NO_THROW(client->connect());
    ASSERT_TRUE(client->isConnected());

    server.stop();
}

TEST_F(SimpleServerTest, WrongCredentialsRejected)
{
    auto daqInstance = SetupInstance(false);
    GenericServer server(daqInstance);
    server.start();

    OpcUaEndpoint endpoint("opc.tcp://127.0.0.1:4840", "adminUser", "wrongPassword");
    auto client = std::make_shared<OpcUaClient>(endpoint);
    ASSERT_THROW(client->connect(), std::exception);

    server.stop();
}

TEST_F(SimpleServerTest, DeviceNodeExistsInObjectsFolder)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    server.start();

    OpcuaServerHelper helper;
    helper.Init();

    auto browseName = daqInstance.getRootDevice().getGlobalId().toStdString();
    std::replace(browseName.begin(), browseName.end(), '/', '-');

    auto deviceNodeId = helper.getChildNodeId(OpcUaNodeId(UA_NS0ID_OBJECTSFOLDER), browseName);
    ASSERT_FALSE(deviceNodeId.isNull());

    helper.Clear();
    server.stop();
}

TEST_F(SimpleServerTest, SignalNodesExistUnderDeviceNode)
{
    auto daqInstance = SetupInstance();
    const auto rootDevice = daqInstance.getRootDevice();
    const SizeT expectedCount = rootDevice.getSignalsRecursive(search::Any()).getCount();
    ASSERT_GT(expectedCount, 0u);

    GenericServer server(daqInstance);
    server.start();

    OpcuaServerHelper helper;
    helper.Init();

    auto browseName = rootDevice.getGlobalId().toStdString();
    std::replace(browseName.begin(), browseName.end(), '/', '-');

    auto deviceNodeId = helper.getChildNodeId(OpcUaNodeId(UA_NS0ID_OBJECTSFOLDER), browseName);
    ASSERT_FALSE(deviceNodeId.isNull());

    auto browseResult = helper.browseNode(deviceNodeId);
    ASSERT_EQ(browseResult->results[0].referencesSize, expectedCount + 1);  // +1 because of HasTypeDefinition

    helper.Clear();
    server.stop();
}

TEST_F(SimpleServerTest, DISABLED_Temp)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    ASSERT_NO_THROW(server.start());
    daqInstance.getDevices()[0].setPropertyValue("GeneratePackets", 1000000);
    std::this_thread::sleep_for(std::chrono::seconds(600));
}