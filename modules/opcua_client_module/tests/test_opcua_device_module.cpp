#include <opcua_client_module/module_dll.h>
#include <opendaq/opendaq.h>
#include <opendaq/module_manager_ptr.h>
#include <opendaq/mock/mock_device_module.h>
#include <opendaq/mock/mock_fb_module.h>
#include <testutils/testutils.h>

using OpcuaDeviceModulesTest = testing::Test;

using namespace daq;

static ModulePtr CreateClientOpcUaModule(const ContextPtr& context)
{
    ModulePtr module;
    createOpcUaClientModule(&module, context);
    return module;
}

static InstancePtr CreateServerInstance()
{
    auto instance = InstanceBuilder().setDefaultRootDeviceLocalId("local").build();
    ContextPtr context = instance.getContext();
    ModuleManagerPtr manager = instance.getModuleManager();

    manager.addModule(ModulePtr(MockDeviceModule_Create(context)));
    manager.addModule(ModulePtr(MockFunctionBlockModule_Create(context)));

    instance.addDevice("daqmock://phys_device");
    instance.addFunctionBlock("mock_fb_uid");
    instance.addServer("OpenDAQOPCUA", nullptr);
    return instance;
}

static InstancePtr CreateClientInstance()
{
    auto instance = InstanceBuilder().setModulePath("[[none]]").build();
    ContextPtr context = instance.getContext();
    ModuleManagerPtr manager = instance.getModuleManager();

    manager.addModule(CreateClientOpcUaModule(context));

    auto config = instance.createDefaultAddDeviceConfig();
    PropertyObjectPtr general = config.getPropertyValue("General");
    general.setPropertyValue("StreamingConnectionHeuristic", 2);

    instance.addDevice("daq.opcua://127.0.0.1", config);
    return instance;
}

TEST_F(OpcuaDeviceModulesTest, ComponentActiveChangedRecursive)
{
    auto server = CreateServerInstance();
    auto client = CreateClientInstance();

    // Get the client's mirror of server device (this is a root device)
    auto clientDevice = client.getDevices()[0];

    // Get all components in clientDevice subtree
    auto clientDeviceComponents = clientDevice.getItems(search::Recursive(search::Any()));

    // Set client (Instance) active to false
    client.setActive(false);

    // client itself should be inactive
    ASSERT_FALSE(client.getActive()) << "client should be inactive";

    // clientDevice should still be active (it's a root device, doesn't receive parentActive)
    ASSERT_TRUE(clientDevice.getActive()) << "clientDevice should remain active as it's a root device";

    // All clientDevice subtree components should still be active
    for (const auto& comp : clientDeviceComponents)
        ASSERT_TRUE(comp.getActive()) << "Component should be active: " << comp.getGlobalId();
}

TEST_F(OpcuaDeviceModulesTest, ComponentActiveChangedRecursiveGateway)
{
    // Create leaf server
    auto leaf = InstanceBuilder().setDefaultRootDeviceLocalId("leaf").build();
    leaf.addServer("OpenDAQOPCUA", nullptr);

    // Create gateway that connects to leaf
    auto gateway = Instance();
    auto gatewayServerConfig = gateway.getAvailableServerTypes().get("OpenDAQOPCUA").createDefaultConfig();
    gatewayServerConfig.setPropertyValue("Port", 4841);
    gateway.addDevice("daq.opcua://127.0.0.1");
    gateway.addServer("OpenDAQOPCUA", gatewayServerConfig);

    // Create client that connects to gateway
    auto client = Instance();
    auto clientGatewayDevice = client.addDevice("daq.opcua://127.0.0.1:4841");

    // Get the leaf device through gateway
    auto clientLeafDevice = clientGatewayDevice.getDevices()[0];

    // Set clientGatewayDevice active to false (from client side)
    clientGatewayDevice.setActive(false);

    // clientGatewayDevice itself should be inactive
    ASSERT_FALSE(clientGatewayDevice.getActive()) << "clientGatewayDevice should be inactive";

    // clientLeafDevice should still be active (it's a root device)
    ASSERT_TRUE(clientLeafDevice.getActive()) << "clientLeafDevice should remain active as it's a root device";
}
