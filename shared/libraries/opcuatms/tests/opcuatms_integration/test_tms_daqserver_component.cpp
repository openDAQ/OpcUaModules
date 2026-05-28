#include <opendaq/context_factory.h>
#include <gtest/gtest.h>
#include <opcuatms_client/objects/tms_client_server_factory.h>
#include <opcuatms_server/objects/tms_server_server.h>
#include "tms_object_integration_test.h"
#include <opendaq/mock/advanced_components_setup_utils.h>

using namespace daq;
using namespace opcua::tms;
using namespace opcua;

class TmsDaqServerComponentTest : public TmsObjectIntegrationTest, public testing::Test
{
public:
    void SetUp() override
    {
        TmsObjectIntegrationTest::Init();
    }

    void TearDown() override
    {
        TmsObjectIntegrationTest::Clear();
    }

    ServerPtr createDaqServerComponent(const ContextPtr& context, const StringPtr& localId = "srv")
    {
        const auto server = createWithImplementation<IServer, test_utils::MockSrvImpl>(context, nullptr, localId);
        return server;
    }
};

TEST_F(TmsDaqServerComponentTest, Create)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx);
    auto tmsServerServer = TmsServerServer(serverDaqServerComponent, this->getServer(), ctx, serverContext);
}

TEST_F(TmsDaqServerComponentTest, Register)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx, "Id");

    auto tmsServerServer = TmsServerServer(serverDaqServerComponent, this->getServer(), ctx, serverContext);
    auto nodeId = tmsServerServer.registerOpcUaNode();

    ServerPtr clientDaqServerComponent = TmsClientServer(NullContext(), nullptr, "Id", clientContext, nodeId, nullptr);

    ASSERT_EQ(serverDaqServerComponent.getLocalId(), clientDaqServerComponent.getLocalId());
}

TEST_F(TmsDaqServerComponentTest, Properties)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx, "Id");

    auto tmsServerServer = TmsServerServer(serverDaqServerComponent, this->getServer(), ctx, serverContext);
    auto nodeId = tmsServerServer.registerOpcUaNode();

    ServerPtr clientDaqServerComponent = TmsClientServer(NullContext(), nullptr, "Id", clientContext, nodeId, nullptr);

    if (serverDaqServerComponent.hasProperty("StrProp"))
    {
        ASSERT_TRUE(clientDaqServerComponent.hasProperty("StrProp"));
        ASSERT_EQ(serverDaqServerComponent.getPropertyValue("StrProp"), clientDaqServerComponent.getPropertyValue("StrProp"));

        serverDaqServerComponent.setPropertyValue("StrProp", "NewValueSetByServer");
        ASSERT_EQ(clientDaqServerComponent.getPropertyValue("StrProp"), "NewValueSetByServer");

        clientDaqServerComponent.setPropertyValue("StrProp", "NewValueSetByClient");
        ASSERT_EQ(serverDaqServerComponent.getPropertyValue("StrProp"), "NewValueSetByClient");
    }
}

TEST_F(TmsDaqServerComponentTest, Attributes)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx, "Id");

    auto tmsServerServer = TmsServerServer(serverDaqServerComponent, this->getServer(), ctx, serverContext);
    auto nodeId = tmsServerServer.registerOpcUaNode();

    ServerPtr clientDaqServerComponent = TmsClientServer(NullContext(), nullptr, "Id", clientContext, nodeId, nullptr);

    ASSERT_TRUE(serverDaqServerComponent.getActive());
    ASSERT_TRUE(clientDaqServerComponent.getActive());

    serverDaqServerComponent.setActive(false);

    ASSERT_FALSE(serverDaqServerComponent.getActive());
    ASSERT_FALSE(clientDaqServerComponent.getActive());

    clientDaqServerComponent.setActive(true);

    ASSERT_TRUE(serverDaqServerComponent.getActive());
    ASSERT_TRUE(clientDaqServerComponent.getActive());
}

TEST_F(TmsDaqServerComponentTest, ComponentMethods)
{
    ServerPtr serverDaqServerComponent = createDaqServerComponent(ctx, "Id");

    auto tmsServerServer = TmsServerServer(serverDaqServerComponent, this->getServer(), ctx, serverContext);
    auto nodeId = tmsServerServer.registerOpcUaNode();

    ServerPtr clientDaqServerComponent = TmsClientServer(NullContext(), nullptr, "Id", clientContext, nodeId, nullptr);

    ASSERT_EQ(serverDaqServerComponent.getName(), clientDaqServerComponent.getName());

    ASSERT_NO_THROW(clientDaqServerComponent.setName("new_name"));
    ASSERT_EQ(serverDaqServerComponent.getName(), clientDaqServerComponent.getName());

    ASSERT_NO_THROW(clientDaqServerComponent.setDescription("new_description"));
    ASSERT_EQ(serverDaqServerComponent.getDescription(), clientDaqServerComponent.getDescription());
}
