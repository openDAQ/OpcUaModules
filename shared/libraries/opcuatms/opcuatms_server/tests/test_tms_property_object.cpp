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
#include <future>
#include "test_helpers.h"
#include "tms_server_test.h"

using namespace daq;
using namespace opcua::tms;
using namespace opcua;
using namespace std::chrono_literals;

class TmsPropertyObjectTest : public TmsServerObjectTest
{
public:
    PropertyObjectPtr createPropertyObject()
    {
        GenericPropertyObjectPtr object = PropertyObject();

        auto intProp = IntProperty("IntProp", 1);
        object.addProperty(intProp);

        return object;
    }
};

TEST_F(TmsPropertyObjectTest, Create)
{
    PropertyObjectPtr propertyObject = createPropertyObject();
    auto tmsPropertyObject = TmsServerPropertyObject(propertyObject, this->getServer(), ctx, tmsCtx);
}

TEST_F(TmsPropertyObjectTest, BaseObjectList)
{
    GenericPropertyObjectPtr object = PropertyObject();
    
    auto list = List<IBaseObject>();
    list.pushBack(1.0);
    list.pushBack(2.0);
    list.pushBack(3.0);
    object.addProperty(ListProperty("ListProp", list));

    auto tmsPropertyObject = TmsServerPropertyObject(object, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();

    OpcUaObject<UA_ReadRequest> request;
    request->nodesToReadSize = 1;
    request->nodesToRead = (UA_ReadValueId*) UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
    request->nodesToRead[0].nodeId = nodeId.getDetachedValue();
    request->nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;

    OpcUaObject<UA_ReadResponse> response = UA_Client_Service_read(client->getLockedUaClient(), *request);
    const auto status = response->responseHeader.serviceResult;
    ASSERT_TRUE(status == UA_STATUSCODE_GOOD);
}

TEST_F(TmsPropertyObjectTest, Register)
{
    PropertyObjectPtr propertyObject = createPropertyObject();

    auto tmsPropertyObject = TmsServerPropertyObject(propertyObject, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));
}

TEST_F(TmsPropertyObjectTest, Permissions)
{
    PropertyObjectPtr propertyObject = createPropertyObject();

    propertyObject.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsObj = TmsServerPropertyObject(propertyObject, this->getServer(), ctx, tmsCtx);
    const auto nodeId = tmsObj.registerOpcUaNode();
    const auto bunID = tmsObj.getBeginUpdateNodeId();
    const auto eunID = tmsObj.getEndUpdateNodeId();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));

    auto lambdaMain = [&](const OpcUaSession& session, bool read, bool write, bool execute)
    {
        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, nodeId.getPtr(), &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, nodeId.getPtr(), &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, nodeId.getPtr(), &session), execute);
    };

    auto lambdaSpecial = [&](const OpcUaSession& session, bool read, bool write, bool execute)
    {
        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, bunID.getPtr(), &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, bunID.getPtr(), &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, bunID.getPtr(), &session), execute);

        EXPECT_EQ(tmsObj.checkPermission(Permission::Read, eunID.getPtr(), &session), read);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Write, eunID.getPtr(), &session), write);
        EXPECT_EQ(tmsObj.checkPermission(Permission::Execute, eunID.getPtr(), &session), execute);
    };

    lambdaMain(test_helpers::createSessionCommon("common"), false, false, false);
    lambdaMain(test_helpers::createSessionReader("reader"), true, false, false);
    lambdaMain(test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaMain(test_helpers::createSessionExecutor("executor"), true, false, true);
    lambdaMain(test_helpers::createSessionAdmin("admin"), true, true, true);

    lambdaSpecial(test_helpers::createSessionCommon("common"), false, false, false);
    lambdaSpecial(test_helpers::createSessionReader("reader"), true, false, false);
    lambdaSpecial(test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaSpecial(test_helpers::createSessionExecutor("executor"), true, false, false);
    lambdaSpecial(test_helpers::createSessionAdmin("admin"), true, true, true);
}

TEST_F(TmsPropertyObjectTest, DISABLED_OnPropertyValueChangeEvent)
{
    PropertyObjectPtr propertyObject = createPropertyObject();

    auto tmsPropertyObject = TmsServerPropertyObject(propertyObject, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();

    OpcUaObject<UA_CreateSubscriptionRequest> request = UA_CreateSubscriptionRequest_default();

    auto subscription = client->createSubscription(request);

    EventMonitoredItemCreateRequest monitoredItem(nodeId);
    EventFilter filter(1);
    filter.setSelectClause(0, SimpleAttributeOperand::CreateMessageValue());

    monitoredItem.setEventFilter(std::move(filter));

    std::promise<void> waitForChangeEvent;
    subscription->monitoredItemsCreateEvent(
        UA_TIMESTAMPSTORETURN_BOTH,
        *monitoredItem,
        [&waitForChangeEvent](
            OpcUaClient* client, Subscription* subContext, MonitoredItem* monContext, size_t nEventFields, UA_Variant* eventFields) {
            waitForChangeEvent.set_value();
        });

    propertyObject.setPropertyValue("IntProp", 2);

    auto future = waitForChangeEvent.get_future();
    ASSERT_NE(future.wait_for(2s), std::future_status::timeout);
}
