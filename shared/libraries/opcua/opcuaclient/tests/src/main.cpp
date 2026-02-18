#include <gtest/gtest.h>
#include <opendaq/module_manager_factory.h>
#include <testutils/memcheck_listener.h>

int main(int argc, char** args)
{
    {
        daq::ModuleManager(".");
    }
    testing::InitGoogleTest(&argc, args);

    testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new MemCheckListener());

    return RUN_ALL_TESTS();
}
