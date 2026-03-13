#include <coreobjects/property_object_factory.h>
#include <coreobjects/unit_factory.h>
#include <gtest/gtest.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuashared/opcuacommon.h>
#include <opcuatms/type_mappings.h>
#include <opcuatms_server/objects/tms_server_channel.h>
#include <open62541/daqbsp_nodeids.h>
#include <opendaq/instance_factory.h>
#include <opendaq/mock/mock_channel_factory.h>
#include "test_helpers.h"
#include "tms_server_test.h"

using namespace daq;
using namespace opcua::tms;
using namespace opcua;
using namespace daq::opcua::utils;

class TmsChannelTest : public TmsServerObjectTest, public testing::Test
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

    ChannelPtr createChannel()
    {
        return MockChannel(ctx, nullptr, "mockch");
    }
};

TEST_F(TmsChannelTest, Create)
{
    ChannelPtr channel = createChannel();
    auto tmsChannel = TmsServerChannel(channel, this->getServer(), ctx, tmsCtx);
}

TEST_F(TmsChannelTest, Register)
{
    ChannelPtr channel = createChannel();
    auto serverChannel = TmsServerChannel(channel, this->getServer(), ctx, tmsCtx);
    auto nodeId = serverChannel.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(nodeId));
}

TEST_F(TmsChannelTest, AttrFunctionBlockType)
{
    ChannelPtr channel = createChannel();
    auto serverChannel = TmsServerChannel(channel, this->getServer(), ctx, tmsCtx);

    auto nodeId = serverChannel.registerOpcUaNode();

    auto variant = readChildNode(nodeId, "FunctionBlockInfo");
    ASSERT_TRUE(variant.isType<UA_FunctionBlockInfoStructure>());

    UA_FunctionBlockInfoStructure type = variant.readScalar<UA_FunctionBlockInfoStructure>();

    ASSERT_EQ(ToStdString(type.id), "mock_ch");
    ASSERT_EQ(ToStdString(type.name), "mock_ch");
    ASSERT_EQ(ToStdString(type.description), "");
}

TEST_F(TmsChannelTest, BrowseSignals)
{
    ChannelPtr channel = createChannel();
    auto serverChannel = TmsServerChannel(channel, this->getServer(), ctx, tmsCtx);
    auto nodeId = serverChannel.registerOpcUaNode();

    OpcUaServerNode serverNodeFB(*this->getServer(), nodeId);
    auto signalServerNode = serverNodeFB.getChildNode(UA_QUALIFIEDNAME_ALLOC(NAMESPACE_DAQBSP, "Sig"));
    auto signalReferences = signalServerNode->browse(OpcUaNodeId(NAMESPACE_DAQBSP, UA_DAQBSPID_HASVALUESIGNAL));
    ASSERT_EQ(signalReferences.size(), 10u);
}

TEST_F(TmsChannelTest, Property)
{
    // Build server channel
    auto serverChannel = createChannel();

    const auto sampleRateProp =
        FloatPropertyBuilder("SampleRate", 100.0).setUnit(Unit("Hz")).setMinValue(1.0).setMaxValue(1000000.0).build();

    serverChannel.addProperty(sampleRateProp);

    auto tmsServerChannel = TmsServerChannel(serverChannel, this->getServer(), ctx, tmsCtx);
    auto nodeId = tmsServerChannel.registerOpcUaNode();

    auto sampleRateNodeId = this->getChildNodeId(nodeId, "SampleRate");
    ASSERT_FALSE(sampleRateNodeId.isNull());

    auto srValue = this->getServer()->readValue(sampleRateNodeId);
    ASSERT_TRUE(srValue.hasScalarType<UA_Double>());
    ASSERT_DOUBLE_EQ(srValue.readScalar<UA_Double>(), 100.0);

    serverChannel.setPropertyValue("SampleRate", 14.0);

    srValue = this->getServer()->readValue(sampleRateNodeId);
    ASSERT_TRUE(srValue.hasScalarType<UA_Double>());
    ASSERT_DOUBLE_EQ(srValue.readScalar<UA_Double>(), 14.0);

    this->getServer()->writeValue(sampleRateNodeId, OpcUaVariant(22.2));

    srValue = this->getServer()->readValue(sampleRateNodeId);
    ASSERT_TRUE(srValue.hasScalarType<UA_Double>());
    ASSERT_DOUBLE_EQ(srValue.readScalar<UA_Double>(), 22.2);
}

TEST_F(TmsChannelTest, Permissions)
{
    ChannelPtr channel = createChannel();
    channel.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    auto tmsObj = TmsServerChannel(channel, this->getServer(), ctx, tmsCtx);

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
