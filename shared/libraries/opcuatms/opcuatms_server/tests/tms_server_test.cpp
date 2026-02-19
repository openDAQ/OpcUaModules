#include "tms_server_test.h"

void TmsServerObjectTest::Init()
{
    TmsObjectTest::Init();

    ctx = daq::NullContext();
    tmsCtx = std::make_shared<daq::opcua::tms::TmsServerContext>(ctx, nullptr);
}

void TmsServerObjectTest::Clear()
{
    ctx = nullptr;
    tmsCtx = nullptr;

    TmsObjectTest::Clear();
}
