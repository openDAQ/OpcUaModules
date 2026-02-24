#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_class_ptr.h>
#include <coreobjects/property_object_factory.h>
#include <gtest/gtest.h>
#include <opcuaclient/event_filter.h>
#include <opcuaclient/monitored_item_create_request.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuaclient/subscriptions.h>
#include <opcuatms_server/objects/tms_server_property_object.h>
#include <opendaq/context_factory.h>
#include "test_helpers.h"
#include "tms_server_test.h"

using namespace daq;
using namespace opcua::tms;
using namespace opcua;
using namespace std::chrono_literals;

class TmsPropertyTest : public TmsServerObjectTest, public testing::Test
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

    PropertyObjectPtr createPropertyObject()
    {
        GenericPropertyObjectPtr object = PropertyObject();

        auto intProp = IntProperty("IntProp", 1);
        object.addProperty(intProp);

        return object;
    }
};

TEST_F(TmsPropertyTest, Create)
{
    PropertyObjectPtr propertyObject = createPropertyObject();
    auto prop = propertyObject.getAllProperties()[0];
    ASSERT_NO_THROW(auto tmsProperty = TmsServerProperty(prop, this->getServer(), ctx, tmsCtx));
}

TEST_F(TmsPropertyTest, BaseObjectList)
{
    PropertyObjectPtr propertyObject = createPropertyObject();
    auto prop = propertyObject.getAllProperties()[0];
    auto tmsProperty = TmsServerProperty(prop, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsProperty.registerOpcUaNode();

    OpcUaObject<UA_ReadRequest> request;
    request->nodesToReadSize = 1;
    request->nodesToRead = (UA_ReadValueId*) UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
    request->nodesToRead[0].nodeId = nodeId.getDetachedValue();
    request->nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;

    OpcUaObject<UA_ReadResponse> response = UA_Client_Service_read(client->getLockedUaClient(), *request);
    const auto status = response->responseHeader.serviceResult;
    ASSERT_TRUE(status == UA_STATUSCODE_GOOD);
}

TEST_F(TmsPropertyTest, Register)
{
    PropertyObjectPtr propertyObject = createPropertyObject();
    auto prop = propertyObject.getAllProperties()[0];

    auto tmsProperty = TmsServerProperty(prop, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsProperty.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));
}

TEST_F(TmsPropertyTest, Permissions)
{
    PropertyObjectPtr propertyObject = createPropertyObject();

    propertyObject.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    auto prop = propertyObject.getAllProperties()[0];
    auto tmsObj = TmsServerProperty(prop, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();

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
