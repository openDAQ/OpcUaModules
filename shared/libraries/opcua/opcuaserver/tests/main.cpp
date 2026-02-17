#include <gtest/gtest.h>
#include <testutils/memcheck_listener.h>    
#include <opendaq/module_manager_factory.h>

int main(int argc, char** args)
{
    {
        daq::ModuleManager(".");
    }
    ::testing::InitGoogleTest(&argc, args);

    testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new MemCheckListener());

    int res = RUN_ALL_TESTS();

    return res;
}
