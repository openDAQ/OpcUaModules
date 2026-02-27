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
    FunctionBlockPtr fb = instance.addFunctionBlock("mock_fb_uid");
    // Set first input port to private
    auto inputPort = fb.getInputPorts(search::LocalId("TestInputPort1"))[0];
    inputPort.setPublic(false);

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
    auto serverDevice = CreateServerInstance();
    auto client = CreateClientInstance();
    auto clientDevice = client.getDevices(search::LocalId("local"))[0];
    ASSERT_TRUE(clientDevice.assigned());

    auto childDevicesServer = serverDevice.getDevices(search::LocalId("mockdev"));
    ASSERT_EQ(childDevicesServer.getCount(), 1);
    auto childDevicesClient = clientDevice.getDevices(search::LocalId("mockdev"));
    ASSERT_EQ(childDevicesClient.getCount(), 1);

    auto signalsServer = childDevicesServer[0].getSignals(search::LocalId("devicetimesigprivate"));
    ASSERT_EQ(signalsServer.getCount(), 1);
    ASSERT_EQ(signalsServer[0].getPublic(), False);

    // Not available on client
    auto signalsClient = childDevicesClient[0].getSignals(search::LocalId("devicetimesigprivate"));
    ASSERT_EQ(signalsClient.getCount(), 0);

    auto fbsServer = serverDevice.getFunctionBlocks();
    ASSERT_EQ(fbsServer.getCount(), 1);
    auto fbsClient = clientDevice.getFunctionBlocks();
    ASSERT_EQ(fbsClient.getCount(), 1);

    auto portsServer = fbsServer[0].getInputPorts(search::LocalId("TestInputPort1"));
    ASSERT_EQ(portsServer.getCount(), 1);
    ASSERT_FALSE(portsServer[0].getPublic());

    // Not available on client
    auto portsClient = fbsClient[0].getInputPorts(search::LocalId("TestInputPort1"));
    ASSERT_EQ(portsClient.getCount(), 0);
}
