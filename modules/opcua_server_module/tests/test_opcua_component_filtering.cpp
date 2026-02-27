#include <coreobjects/authentication_provider_factory.h>
#include <coretypes/common.h>
#include <opcua_server_module/module_dll.h>
#include <opcua_server_module/version.h>
#include <opcuaclient/opcuaclient.h>
#include <opendaq/context_factory.h>
#include <opendaq/instance_factory.h>
#include <opendaq/instance_ptr.h>
#include <opendaq/mock/mock_device_module.h>
#include <opendaq/mock/mock_fb_module.h>
#include <opendaq/module_manager_factory.h>
#include <opendaq/module_ptr.h>
#include <opendaq/opendaq.h>

#include <testutils/testutils.h>

using OpcUaComponentFilteringTest = testing::Test;

using namespace daq;

static ModulePtr CreateModule(ContextPtr context = NullContext())
{
    ModulePtr module;
    createOpcUaServerModule(&module, context);
    return module;
}

static InstancePtr CreateServerInstance()
{
    const auto logger = Logger();
    const auto moduleManager = ModuleManager("[[none]]");
    const auto authenticationProvider = AuthenticationProvider();
    const auto context = Context(Scheduler(logger), logger, TypeManager(), moduleManager, authenticationProvider);

    const ModulePtr deviceModule(MockDeviceModule_Create(context));
    moduleManager.addModule(deviceModule);

    const ModulePtr fbModule(MockFunctionBlockModule_Create(context));
    moduleManager.addModule(fbModule);

    const ModulePtr opcuaServerModule = CreateModule(context);
    moduleManager.addModule(opcuaServerModule);

    auto instance = InstanceBuilder().setContext(context).setDefaultRootDeviceLocalId("local").build();

    instance.addDevice("daqmock://phys_device");
    instance.addFunctionBlock("mock_fb_uid");
    instance.addServer("OpenDAQOPCUA", nullptr);
    return instance;
}

static InstancePtr CreateClientInstance()
{
    auto instance = InstanceBuilder().build();

    auto config = instance.createDefaultAddDeviceConfig();
    PropertyObjectPtr general = config.getPropertyValue("General");
    general.setPropertyValue("StreamingConnectionHeuristic", 2);

    instance.addDevice("daq.opcua://127.0.0.1", config);
    return instance;
}

TEST_F(OpcUaComponentFilteringTest, FilterComponents)
{
    auto server = CreateServerInstance();
    auto client = CreateClientInstance();

    auto childDevices = server.getDevices(search::LocalId("mockdev"));
    ASSERT_EQ(childDevices.getCount(), 1);
    auto signals = childDevices[0].getSignals(search::LocalId("devicetimesigprivate"));
    ASSERT_EQ(signals.getCount(), 1);
    ASSERT_EQ(signals[0].getPublic(), False);

    auto rootDeviceClient = client.getDevices(search::LocalId("local"))[0];
    ASSERT_TRUE(rootDeviceClient.assigned());
    auto childDevicesClient = rootDeviceClient.getDevices(search::LocalId("mockdev"));
    ASSERT_EQ(childDevicesClient.getCount(), 1);
    auto signalsClient = childDevicesClient[0].getSignals(search::LocalId("devicetimesigprivate"));
    ASSERT_EQ(signalsClient.getCount(), 0);
}
