#include <coreobjects/argument_info_factory.h>
#include <coreobjects/callable_info_factory.h>
#include <coreobjects/coercer_factory.h>
#include <coreobjects/property_object_class_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_object_protected_ptr.h>
#include <coreobjects/unit_factory.h>
#include <coreobjects/validator_factory.h>
#include <coretypes/struct_factory.h>
#include <coretypes/type_manager_factory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <opcuaclient/opcuaclient.h>
#include <opcuatms_client/objects/tms_client_device_factory.h>
#include <opcuatms_client/objects/tms_client_property_object_factory.h>
#include <opcuatms_client/objects/tms_client_property_object_impl.h>
#include <opcuatms_server/objects/tms_server_device.h>
#include <opcuatms_server/objects/tms_server_property_object.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/instance_factory.h>
#include <opendaq/mock/default_mocks_factory.h>
#include <opendaq/mock/mock_device_module.h>
#include <opendaq/mock/mock_fb_module.h>
#include <opendaq/mock/mock_physical_device.h>
#include <opendaq/server_type_factory.h>
#include "test_user_helper.h"
#include "tms_object_integration_test.h"

using namespace daq;
using namespace opcua::tms;
// using namespace opcua;

#define DISABLE_EXPECTED_FAILURE
#ifdef DISABLE_EXPECTED_FAILURE
#define DISABLE_EXPECT_ANY_THROW(statement) EXPECT_NO_THROW(statement)
#else
#define DISABLE_EXPECT_ANY_THROW(statement) EXPECT_ANY_THROW(statement)
#endif
#ifdef DISABLE_EXPECTED_FAILURE
#define DISABLE_EXPECT_EQ(val1, val2) EXPECT_NE(val1, val2)
#else
#define DISABLE_EXPECT_EQ(val1, val2) EXPECT_EQ(val1, val2)
#endif

class TmsUserAccessTest : public TmsObjectIntegrationTest, public testing::Test
{
public:
    void SetUp() override
    {
        TmsObjectIntegrationTest::Init();
        createDevice();
    }

    void TearDown() override
    {
        TmsObjectIntegrationTest::Clear();
    }

    InstancePtr createDevice()
    {
        const auto moduleManager = ModuleManager("[[none]]");
        auto context = Context(nullptr, Logger(), nullptr, moduleManager, nullptr);
        const ModulePtr deviceModule(MockDeviceModule_Create(context));
        moduleManager.addModule(deviceModule);

        const ModulePtr fbModule(MockFunctionBlockModule_Create(context));
        moduleManager.addModule(fbModule);

        instance = InstanceCustom(context, "localInstance");
        instance.addDevice("daq.root://default_client");
        device = instance.addDevice("daqmock://phys_device");

        const auto infoInternal = device.getInfo().asPtr<IDeviceInfoInternal>();
        infoInternal.addServerCapability(ServerCapability("protocol_1", "protocol 1", ProtocolType::Streaming));
        infoInternal.addServerCapability(ServerCapability("protocol_2", "protocol 2", ProtocolType::Configuration));

        fb = instance.addFunctionBlock("mock_fb_uid");

        device.addProperty(IntPropertyBuilder("ReadOnlyProperty", 0).setReadOnly(true).build());
        device.addProperty(IntPropertyBuilder("InvisibleProperty", 1).setReadOnly(false).setVisible(false).build());

        fb.addProperty(IntPropertyBuilder("ReadOnlyProperty", 0).setReadOnly(true).build());
        fb.addProperty(IntPropertyBuilder("InvisibleProperty", 1).setReadOnly(false).setVisible(false).build());
        Print(device, fb);
        return instance;
    }

    void CreateClient(const std::string& login, const std::string& password)
    {
        client = CreateAndConnectTestClient(login, password);

        ctxClient = daq::NullContext(logger);
        clientContext = std::make_shared<TmsClientContext>(client, ctxClient);

        clientContext->addEnumerationTypesToTypeManager();
    }

    void Print(DevicePtr device, FunctionBlockPtr fb)
    {
        auto printProp = [](daq::ListPtr<daq::IProperty> props)
        {
            for (const auto& prop : props)
            {
                std::cout << "PROPERTY: " << prop.getName() << " ValueType: " << prop.getValueType()
                          << " IsReadOnly: " << (prop.getReadOnly() ? "true" : "false")
                          << " IsVisibly: " << (prop.getVisible() ? "true" : "false") << std::endl;
            }
        };
        auto printSignals = [](daq::ListPtr<daq::ISignal> sigs)
        {
            for (const auto& sig : sigs)
            {
                std::cout << "SIGNAL: " << sig.getName() << " IsVisibly: " << (sig.getVisible() ? "true" : "false") << std::endl;
            }
        };

        auto pringInputPorts = [](daq::ListPtr<daq::IInputPort> ports)
        {
            for (const auto& port : ports)
            {
                std::cout << "PORTS: " << port.getName() << std::endl;
            }
        };
        std::cout << "---dev---" << std::endl;
        printProp(device.getAllProperties());
        printSignals(device.getSignals());
        std::cout << "---fb---" << std::endl;
        printProp(fb.getAllProperties());
        printSignals(fb.getSignals());
        pringInputPorts(fb.getInputPorts());
        std::cout << "---end---" << std::endl;
    }

    InstancePtr instance;
    DevicePtr device;
    FunctionBlockPtr fb;
};

TEST_F(TmsUserAccessTest, CreateClientDevice)
{
    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    ASSERT_NO_THROW(TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));
}

TEST_F(TmsUserAccessTest, Anonymous)
{
    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient("", "");
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    EXPECT_EQ(clientDevice.getDevices().getCount(), 1);
    EXPECT_EQ(clientDevice.getFunctionBlocks().getCount(), 0);
}

TEST_F(TmsUserAccessTest, CommonUser)
{
    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient("commonUser", "commonUserPass");
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    EXPECT_EQ(clientDevice.getDevices().getCount(), 1);
    EXPECT_EQ(clientDevice.getFunctionBlocks().getCount(), 0);
}

TEST_F(TmsUserAccessTest, CommonUserForRootDevice)
{
    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient("commonUser", "commonUserPass");
    DevicePtr clientDevice;
    ASSERT_ANY_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));
}

TEST_F(TmsUserAccessTest, ReaderUser)
{
    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient("readerUser", "readerUserPass");
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    // one private signal in MockFunctionBlockImpl. and one in MockPhysicalDeviceImpl
    EXPECT_EQ(device.getAllProperties().getCount() - 1, mockDevice.getAllProperties().getCount());
    EXPECT_EQ(fb.getAllProperties().getCount(), mockFb.getAllProperties().getCount());

    // struct property in a mock device
    EXPECT_EQ(device.getSignals().getCount() - 1, mockDevice.getSignals().getCount());
    EXPECT_EQ(fb.getSignals().getCount() - 1, mockFb.getSignals().getCount());

    // an exception has not been implemented on opc ua client side
    // it catchs opcua-wrapper exeption (via daqTry) and print error message to log but return seccess
    {
        DISABLE_EXPECT_ANY_THROW(mockDevice.getProperty("TestProperty").setValue(String("NewValue")));
        EXPECT_NE(mockDevice.getPropertyValue("TestProperty"), "NewValue");

        DISABLE_EXPECT_ANY_THROW(mockDevice.setName("NewDevName"));
        EXPECT_NE(mockDevice.getName(), "NewDevName");

        DISABLE_EXPECT_ANY_THROW(mockDevice.setDescription("NewDeviceDescription"));
        EXPECT_NE(mockDevice.getDescription(), "NewDeviceDescription");

        ProcedurePtr proc;
        EXPECT_NO_THROW(proc = mockDevice.getPropertyValue("stop"));
        EXPECT_ANY_THROW(proc());

        DISABLE_EXPECT_ANY_THROW(mockFb.getProperty("TestConfigString").setValue(String("NewValue")));
        EXPECT_NE(mockFb.getPropertyValue("TestConfigString"), "NewValue");

        DISABLE_EXPECT_ANY_THROW(mockFb.setName("NewFbName"));
        EXPECT_NE(mockFb.getName(), "NewFbName");

        DISABLE_EXPECT_ANY_THROW(mockFb.setDescription("NewFbDescription"));
        EXPECT_NE(mockFb.getDescription(), "NewFbDescription");
    }

    {
        // due to lack of access rights (the reader does not have write and execute permissions), these calls should fail
        // an exception has not been implemented on opc ua client side
        // it calls execution without checking of user access rights and when even doesn't check a call result
        DISABLE_EXPECT_ANY_THROW(mockDevice->beginUpdate());
        DISABLE_EXPECT_ANY_THROW(mockDevice->endUpdate());

        DISABLE_EXPECT_ANY_THROW(mockFb->beginUpdate());
        DISABLE_EXPECT_ANY_THROW(mockFb->endUpdate());
    }
    {
        EXPECT_ANY_THROW(clientDevice.addFunctionBlock("mock_fb_uid"));
        EXPECT_ANY_THROW(clientDevice.removeFunctionBlock(mockFb));
    }
}

TEST_F(TmsUserAccessTest, WriterUser)
{
    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient("writerUser", "writerUserPass");
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    // one private signal in MockFunctionBlockImpl. and one in MockPhysicalDeviceImpl
    EXPECT_EQ(device.getAllProperties().getCount() - 1, mockDevice.getAllProperties().getCount());
    EXPECT_EQ(fb.getAllProperties().getCount(), mockFb.getAllProperties().getCount());

    // struct property in a mock device
    EXPECT_EQ(device.getSignals().getCount() - 1, mockDevice.getSignals().getCount());
    EXPECT_EQ(fb.getSignals().getCount() - 1, mockFb.getSignals().getCount());

    {
        EXPECT_NO_THROW(mockDevice.getProperty("TestProperty").setValue(String("NewValue")));
        EXPECT_EQ(mockDevice.getPropertyValue("TestProperty"), "NewValue");

        EXPECT_NO_THROW(mockDevice.setName("NewDevName"));
        EXPECT_EQ(mockDevice.getName(), "NewDevName");

        EXPECT_NO_THROW(mockDevice.setDescription("NewDeviceDescription"));
        EXPECT_EQ(mockDevice.getDescription(), "NewDeviceDescription");

        ProcedurePtr proc;
        EXPECT_NO_THROW(proc = mockDevice.getPropertyValue("stop"));
        EXPECT_ANY_THROW(proc());

        EXPECT_NO_THROW(mockFb.getProperty("TestConfigString").setValue(String("NewValue")));
        EXPECT_EQ(mockFb.getPropertyValue("TestConfigString"), "NewValue");

        EXPECT_NO_THROW(mockFb.setName("NewFbName"));
        EXPECT_EQ(mockFb.getName(), "NewFbName");

        EXPECT_NO_THROW(mockFb.setDescription("NewFbDescription"));
        EXPECT_EQ(mockFb.getDescription(), "NewFbDescription");
    }

    {
        // due to lack of access rights (the writer does not have execute permissions), these calls should fail
        // an exception has not been implemented on opc ua client side
        // it calls execution without checking of user access rights and when even doesn't check a call result
        DISABLE_EXPECT_ANY_THROW(mockDevice->beginUpdate());
        DISABLE_EXPECT_ANY_THROW(mockDevice->endUpdate());

        DISABLE_EXPECT_ANY_THROW(mockFb->beginUpdate());
        DISABLE_EXPECT_ANY_THROW(mockFb->endUpdate());
    }
    {
        EXPECT_ANY_THROW(clientDevice.addFunctionBlock("mock_fb_uid"));
        EXPECT_ANY_THROW(clientDevice.removeFunctionBlock(mockFb));
    }
}

TEST_F(TmsUserAccessTest, ExecutorUser)
{
    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient("executorUser", "executorUserPass");
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    // one private signal in MockFunctionBlockImpl. and one in MockPhysicalDeviceImpl
    EXPECT_EQ(device.getAllProperties().getCount() - 1, mockDevice.getAllProperties().getCount());
    EXPECT_EQ(fb.getAllProperties().getCount(), mockFb.getAllProperties().getCount());

    // struct property in a mock device
    EXPECT_EQ(device.getSignals().getCount() - 1, mockDevice.getSignals().getCount());
    EXPECT_EQ(fb.getSignals().getCount() - 1, mockFb.getSignals().getCount());

    // due to lack of access rights (the executor does not have write permissions), ally write calls should fail
    // an exception has not been implemented on opc ua client side
    // it catchs opcua-wrapper exeption (via daqTry) and print error message to log but return seccess
    {
        DISABLE_EXPECT_ANY_THROW(mockDevice.getProperty("TestProperty").setValue(String("NewValue")));
        EXPECT_NE(mockDevice.getPropertyValue("TestProperty"), "NewValue");

        DISABLE_EXPECT_ANY_THROW(mockDevice.setName("NewDevName"));
        EXPECT_NE(mockDevice.getName(), "NewDevName");

        DISABLE_EXPECT_ANY_THROW(mockDevice.setDescription("NewDeviceDescription"));
        EXPECT_NE(mockDevice.getDescription(), "NewDeviceDescription");

        ProcedurePtr proc;
        EXPECT_NO_THROW(proc = mockDevice.getPropertyValue("stop"));
        EXPECT_NO_THROW(proc());

        DISABLE_EXPECT_ANY_THROW(mockFb.getProperty("TestConfigString").setValue(String("NewValue")));
        EXPECT_NE(mockFb.getPropertyValue("TestConfigString"), "NewValue");

        DISABLE_EXPECT_ANY_THROW(mockFb.setName("NewFbName"));
        EXPECT_NE(mockFb.getName(), "NewFbName");

        DISABLE_EXPECT_ANY_THROW(mockFb.setDescription("NewFbDescription"));
        EXPECT_NE(mockFb.getDescription(), "NewFbDescription");
    }

    {
        // due to lack of access rights (the executor does not have write permissions), these calls should fail
        // an exception has not been implemented on opc ua client side
        // it calls execution without checking of user access rights and when even doesn't check a call result
        DISABLE_EXPECT_ANY_THROW(mockDevice->beginUpdate());
        DISABLE_EXPECT_ANY_THROW(mockDevice->endUpdate());

        DISABLE_EXPECT_ANY_THROW(mockFb->beginUpdate());
        DISABLE_EXPECT_ANY_THROW(mockFb->endUpdate());
    }

    {
        EXPECT_ANY_THROW(clientDevice.addFunctionBlock("mock_fb_uid"));
        EXPECT_ANY_THROW(clientDevice.removeFunctionBlock(mockFb));
    }
}

TEST_F(TmsUserAccessTest, AdminUser)
{
    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient("adminUser", "adminUserPass");
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    // one private signal in MockFunctionBlockImpl. and one in MockPhysicalDeviceImpl
    EXPECT_EQ(device.getAllProperties().getCount() - 1, mockDevice.getAllProperties().getCount());
    EXPECT_EQ(fb.getAllProperties().getCount(), mockFb.getAllProperties().getCount());

    // struct property in a mock device
    EXPECT_EQ(device.getSignals().getCount() - 1, mockDevice.getSignals().getCount());
    EXPECT_EQ(fb.getSignals().getCount() - 1, mockFb.getSignals().getCount());

    {
        EXPECT_NO_THROW(mockDevice.getProperty("TestProperty").setValue(String("NewValue")));
        EXPECT_EQ(mockDevice.getPropertyValue("TestProperty"), "NewValue");

        EXPECT_NO_THROW(mockDevice.setName("NewDevName"));
        EXPECT_EQ(mockDevice.getName(), "NewDevName");

        EXPECT_NO_THROW(mockDevice.setDescription("NewDeviceDescription"));
        EXPECT_EQ(mockDevice.getDescription(), "NewDeviceDescription");

        ProcedurePtr proc;
        EXPECT_NO_THROW(proc = mockDevice.getPropertyValue("stop"));
        EXPECT_NO_THROW(proc());

        EXPECT_NO_THROW(mockFb.getProperty("TestConfigString").setValue(String("NewValue")));
        EXPECT_EQ(mockFb.getPropertyValue("TestConfigString"), "NewValue");

        EXPECT_NO_THROW(mockFb.setName("NewFbName"));
        EXPECT_EQ(mockFb.getName(), "NewFbName");

        EXPECT_NO_THROW(mockFb.setDescription("NewFbDescription"));
        EXPECT_EQ(mockFb.getDescription(), "NewFbDescription");
    }

    {
        EXPECT_NO_THROW(mockDevice->beginUpdate());
        EXPECT_NO_THROW(mockDevice->endUpdate());

        EXPECT_NO_THROW(mockFb->beginUpdate());
        EXPECT_NO_THROW(mockFb->endUpdate());
    }

    {
        EXPECT_NO_THROW(clientDevice.addFunctionBlock("mock_fb_uid"));
        EXPECT_NO_THROW(clientDevice.removeFunctionBlock(mockFb));
    }
}
