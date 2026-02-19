#include <gtest/gtest.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuaserver/opcuaserver.h>
#include <coreobjects/authentication_provider_factory.h>
#include <coreobjects/user_factory.h>

using namespace daq;
using namespace daq::opcua;

using OpcUaPermissionTest = testing::Test;

TEST_F(OpcUaPermissionTest, DefaultConnect)
{
    auto user = User("jure", "jure123");
    auto sessionId = UA_NodeId();
    auto serverLock = OpcUaServerLock();
    auto session = OpcUaSession(sessionId, &serverLock, user);
    ASSERT_TRUE(session.getUser() == user);
}

TEST_F(OpcUaPermissionTest, NoAnonymous)
{
    auto server = OpcUaServer();
    server.setPort(4840);
    server.setAuthenticationProvider(AuthenticationProvider(false));
    server.start();

    auto client = OpcUaClient("opc.tcp://127.0.0.1");
    ASSERT_THROW(client.connect(), OpcUaException);
}

TEST_F(OpcUaPermissionTest, AuthenticationProvider)
{
    auto users = List<IUser>();
    users.pushBack(User("jure", "jure123"));
    users.pushBack(User("tomaz", "tomaz123"));
    auto authenticationProvider = StaticAuthenticationProvider(true, users);

    auto server = OpcUaServer();
    server.setPort(4840);
    server.setAuthenticationProvider(authenticationProvider);
    server.start();

    OpcUaClientPtr client;

    client = std::make_shared<OpcUaClient>("opc.tcp://127.0.0.1");
    ASSERT_NO_THROW(client->connect());

    client = std::make_shared<OpcUaClient>(OpcUaEndpoint("opc.tcp://127.0.0.1", "jure", "wrongPass"));
    ASSERT_THROW(client->connect(), OpcUaException);

    client = std::make_shared<OpcUaClient>(OpcUaEndpoint("opc.tcp://127.0.0.1", "jure", "jure123"));
    ASSERT_NO_THROW(client->connect());

    client = std::make_shared<OpcUaClient>(OpcUaEndpoint("opc.tcp://127.0.0.1", "tomaz", "tomaz123"));
    ASSERT_NO_THROW(client->connect());
}
