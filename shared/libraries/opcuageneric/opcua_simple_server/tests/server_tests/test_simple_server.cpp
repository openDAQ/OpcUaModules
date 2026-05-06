#include <gtest/gtest.h>
#include <opcua_simple_server/simple_server.h>
#include <generic_opcua_test_helper.h>
#include <test_helpers.h>

using SimpleServerTest = testing::Test;

using namespace daq;
using namespace daq::opcua;
using namespace test_helpers;


TEST_F(SimpleServerTest, Create)
{
    auto daqInstance = SetupInstance();
    ASSERT_NO_THROW(GenericServer server(daqInstance));
}

TEST_F(SimpleServerTest, StartStop)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    ASSERT_NO_THROW(server.start());
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

TEST_F(SimpleServerTest, Temp)
{
    auto daqInstance = SetupInstance();
    GenericServer server(daqInstance);
    ASSERT_NO_THROW(server.start());
    std::this_thread::sleep_for(std::chrono::seconds(600));
}