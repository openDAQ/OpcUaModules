#include <gtest/gtest.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuatms_server/objects/tms_server_input_port.h>
#include <opcuatms_server/objects/tms_server_signal.h>
#include <open62541/daqbsp_nodeids.h>
#include <open62541/daqbt_nodeids.h>
#include <open62541/nodeids.h>
#include <opendaq/data_descriptor_factory.h>
#include <opendaq/input_port_factory.h>
#include <opendaq/range_factory.h>
#include <opendaq/signal_factory.h>
#include <opendaq/signal_ptr.h>
#include "test_helpers.h"
#include "tms_server_test.h"

using namespace daq;
using namespace opcua::tms;
using namespace opcua;

class TmsValueTest : public TmsServerObjectTest
{
public:
    SignalConfigPtr createSignal(const ContextPtr& context, const StringPtr& localId = "sig")
    {
        SignalConfigPtr signal = Signal(context, nullptr, localId);
        signal.setActive(false);
        return signal;
    }
};

TEST_F(TmsValueTest, Create)
{
    SignalConfigPtr signal = Signal(ctx, nullptr, "sig");
    auto tmsSignal = TmsServerSignal(signal, this->getServer(), ctx, tmsCtx);
}

TEST_F(TmsValueTest, Register)
{
    SignalConfigPtr signal = Signal(ctx, nullptr, "sig");

    auto value = TmsServerValue(signal, this->getServer(), ctx, tmsCtx);
    auto valueNodeId = value.registerOpcUaNode();

    auto analogValue = TmsServerAnalogValue(signal, this->getServer(), ctx, tmsCtx);
    auto analogValueNodeId = analogValue.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(valueNodeId));
    ASSERT_TRUE(this->getClient()->nodeExists(analogValueNodeId));
}

TEST_F(TmsValueTest, Permissions)
{
    SignalConfigPtr signal = Signal(ctx, nullptr, "sig");
    signal.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto value = TmsServerValue(signal, this->getServer(), ctx, tmsCtx);
    auto valueNodeId = value.registerOpcUaNode();

    auto analogValue = TmsServerAnalogValue(signal, this->getServer(), ctx, tmsCtx);
    auto analogValueNodeId = analogValue.registerOpcUaNode();

    ASSERT_TRUE(this->getClient()->nodeExists(valueNodeId));
    ASSERT_TRUE(this->getClient()->nodeExists(analogValueNodeId));

    auto lambdaMain = [&](const OpcUaSession& session, bool read, bool write, bool execute)
    {
        EXPECT_EQ(value.checkPermission(Permission::Read, valueNodeId.getPtr(), &session), read);
        EXPECT_EQ(value.checkPermission(Permission::Write, valueNodeId.getPtr(), &session), write);
        EXPECT_EQ(value.checkPermission(Permission::Execute, valueNodeId.getPtr(), &session), execute);

        EXPECT_EQ(analogValue.checkPermission(Permission::Read, analogValueNodeId.getPtr(), &session), read);
        EXPECT_EQ(analogValue.checkPermission(Permission::Write, analogValueNodeId.getPtr(), &session), write);
        EXPECT_EQ(analogValue.checkPermission(Permission::Execute, analogValueNodeId.getPtr(), &session), execute);
    };

    lambdaMain(test_helpers::createSessionCommon("common"), false, false, false);
    lambdaMain(test_helpers::createSessionReader("reader"), true, false, false);
    lambdaMain(test_helpers::createSessionWriter("writer"), true, true, false);
    lambdaMain(test_helpers::createSessionExecutor("executor"), true, false, true);
    lambdaMain(test_helpers::createSessionAdmin("admin"), true, true, true);

}
