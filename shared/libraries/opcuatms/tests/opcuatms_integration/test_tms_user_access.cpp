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

class TmsUserAccess : public TmsObjectIntegrationTest
{
public:
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

    bool ListContains(daq::ListPtr<daq::IString> list, const std::string& value)
    {
        for (const auto& item : list)
        {
            if (item == value)
                return true;
        }
        return false;
    };

    InstancePtr instance;
    DevicePtr device;
    FunctionBlockPtr fb;
};

class TmsUserAccessTest : public TmsUserAccess, public testing::Test
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
};

class UserPermission
{
public:
    enum PermissionType : uint32_t
    {
        Read = 0b001,
        Write = 0b010,
        Execute = 0b100
    };

    UserPermission(PermissionType permissionType) : permissionType(permissionType) {};

    bool hasPermission(PermissionType permissionType) const
    {
        return ((static_cast<uint32_t>(this->permissionType) & static_cast<uint32_t>(permissionType)) == permissionType);
    }
protected:
    PermissionType permissionType;
};

namespace {
    std::string ParamNameGenerator(const testing::TestParamInfo<UserPermission::PermissionType>& info)
    {
        using UP = UserPermission::PermissionType;
        UserPermission perms(info.param);
        if (perms.hasPermission(UP::Write) && perms.hasPermission(UP::Execute))
        {
            return "adminRights";
        }
        else if (perms.hasPermission(UP::Write))
        {
            return "writerRights";
        }
        else if (perms.hasPermission(UP::Execute))
        {
            return "executorRights";
        }
        else if (perms.hasPermission(UP::Read))
        {
            return "readerRights";
        }
        else
        {
            throw std::runtime_error("Invalid permissions for test user");
        }
        return "";
    }
}

class TmsUserAccessPTest : public TmsUserAccess, public ::testing::TestWithParam<UserPermission::PermissionType>
{
public:
    void SetUp() override
    {
        TmsObjectIntegrationTest::Init();
        createDevice();
        serverContext = std::make_shared<TmsServerContext>(ctx, instance.getRootDevice());
    }

    void TearDown() override
    {
        TmsObjectIntegrationTest::Clear();
    }

    void CreateClient(UserPermission perms)
    {
        using UP = UserPermission::PermissionType;
        if (perms.hasPermission(UP::Write) && perms.hasPermission(UP::Execute))
        {
            client = CreateAndConnectTestClient("adminUser", "adminUserPass");
        }
        else if (perms.hasPermission(UP::Write))
        {
            client = CreateAndConnectTestClient("writerUser", "writerUserPass");
        }
        else if (perms.hasPermission(UP::Execute))
        {
            client = CreateAndConnectTestClient("executorUser", "executorUserPass");
        }
        else if (perms.hasPermission(UP::Read))
        {
            client = CreateAndConnectTestClient("readerUser", "readerUserPass");
        }
        else
        {
            throw std::runtime_error("Invalid permissions for test user");
        }

        ctxClient = daq::NullContext(logger);
        clientContext = std::make_shared<TmsClientContext>(client, ctxClient);

        clientContext->addEnumerationTypesToTypeManager();
    }

    const std::vector<std::string>& attributesToCheck()
    {
        return attributes;
    }

protected:
    std::vector<std::string> attributes{"Name", "Description", "Active"};
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

TEST_P(TmsUserAccessPTest, CommonChecks)
{
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    // one private signal in MockFunctionBlockImpl. and one in MockPhysicalDeviceImpl
    EXPECT_EQ(device.getAllProperties().getCount() - 1, mockDevice.getAllProperties().getCount());
    EXPECT_EQ(fb.getAllProperties().getCount(), mockFb.getAllProperties().getCount());

    EXPECT_EQ(device.getSignals().getCount() - 1, mockDevice.getSignals().getCount());
    EXPECT_EQ(fb.getSignals().getCount() - 1, mockFb.getSignals().getCount());
}

TEST_P(TmsUserAccessPTest, Properties)
{
    using UP = UserPermission::PermissionType;
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    EXPECT_EQ(mockDevice.getProperty("TestProperty").getReadOnly(), userPerm.hasPermission(UP::Write) ? False : True);
    if (userPerm.hasPermission(UP::Write))
    {
        EXPECT_NO_THROW(mockDevice.getProperty("TestProperty").setValue(String("NewValue")));
        EXPECT_EQ(mockDevice.getPropertyValue("TestProperty"), "NewValue");
    }
    else
    {
        EXPECT_ANY_THROW(mockDevice.getProperty("TestProperty").setValue(String("NewValue")));
        EXPECT_NE(mockDevice.getPropertyValue("TestProperty"), "NewValue");
    }

    ProcedurePtr proc;
    EXPECT_EQ(mockDevice.getProperty("stop").getVisible(), userPerm.hasPermission(UP::Execute) ? True : False);
    EXPECT_NO_THROW(proc = mockDevice.getPropertyValue("stop"));
    if (userPerm.hasPermission(UP::Execute))
    {
        EXPECT_NO_THROW(proc());
    }
    else
    {
        EXPECT_ANY_THROW(proc());
    }


    EXPECT_EQ(mockFb.getProperty("TestConfigString").getReadOnly(), userPerm.hasPermission(UP::Write) ? False : True);
    if (userPerm.hasPermission(UP::Write))
    {
        EXPECT_NO_THROW(mockFb.getProperty("TestConfigString").setValue(String("NewValue")));
        EXPECT_EQ(mockFb.getPropertyValue("TestConfigString"), "NewValue");
    }
    else
    {
        EXPECT_ANY_THROW(mockFb.getProperty("TestConfigString").setValue(String("NewValue")));
        EXPECT_NE(mockFb.getPropertyValue("TestConfigString"), "NewValue");
    }
}

TEST_P(TmsUserAccessPTest, Attributes)
{
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    daq::ListPtr<daq::IComponent> list = List<daq::IComponent>();
    list.pushBack(mockDevice);
    list.pushBack(mockFb);

    for (const auto& sig : mockDevice.getSignals())
        list.pushBack(sig);

    for (const auto& channel : mockDevice.getChannels())
        list.pushBack(channel);

    for (const auto& sig : mockFb.getSignals())
        list.pushBack(sig);

    for (const auto& ip : mockFb.getInputPorts())
        list.pushBack(ip);

    {
        for (const auto& comp : list)
        {
            const auto listLockedAttr = comp.getLockedAttributes();
            for (const auto& attr : attributesToCheck())
            {
                EXPECT_EQ(ListContains(listLockedAttr, attr), !userPerm.hasPermission(UserPermission::PermissionType::Write));
            }

            EXPECT_NO_THROW(comp.setName("NewCompName"));
            EXPECT_NO_THROW(comp.setDescription("NewCompDescription"));

            if (userPerm.hasPermission(UserPermission::PermissionType::Write))
            {

                EXPECT_EQ(comp.getName(), "NewCompName");
                EXPECT_EQ(comp.getDescription(), "NewCompDescription");
            }
            else
            {
                EXPECT_NE(comp.getName(), "NewCompName");
                EXPECT_NE(comp.getDescription(), "NewCompDescription");
            }

        }
    }
}

TEST_P(TmsUserAccessPTest, BeginEndUpdate)
{
    using UP = UserPermission::PermissionType;
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    const ErrCode expectedCode = (userPerm.hasPermission(UP::Write | UP::Execute)) ? OPENDAQ_SUCCESS : OPENDAQ_ERR_ACCESSDENIED;

    ErrCode code = OPENDAQ_SUCCESS;
    ASSERT_NO_THROW(code = mockDevice->beginUpdate());
    EXPECT_EQ(code, expectedCode);
    ASSERT_NO_THROW(code = mockDevice->endUpdate());
    EXPECT_EQ(code, expectedCode);

    ASSERT_NO_THROW(code = mockFb->beginUpdate());
    EXPECT_EQ(code, expectedCode);
    ASSERT_NO_THROW(code = mockFb->endUpdate());
    EXPECT_EQ(code, expectedCode);
}

TEST_P(TmsUserAccessPTest, AvailableComponents)
{
    using UP = UserPermission::PermissionType;
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    if (userPerm.hasPermission(UP::Write))
    {
        EXPECT_EQ(clientDevice.getAvailableFunctionBlockTypes().getCount(), instance.getRootDevice().getAvailableFunctionBlockTypes().getCount());
        EXPECT_EQ(mockDevice.getAvailableFunctionBlockTypes().getCount(), device.getAvailableFunctionBlockTypes().getCount());
    }
    else
    {
        EXPECT_EQ(clientDevice.getAvailableFunctionBlockTypes().getCount(), 0);
        EXPECT_EQ(mockDevice.getAvailableFunctionBlockTypes().getCount(), 0);
    }

    {
        // getAvailableFunctionBlockTypes() has not been implemented for a function block (client and server sides),
        // so it will return 0 regardless of permissions.
        EXPECT_EQ(mockFb.getAvailableFunctionBlockTypes().getCount(), 0);

        // getAvailableDeviceTypes() has not been implemented for a device (client and server sides),
        // so it will return 0 regardless of permissions.
        EXPECT_EQ(clientDevice.getAvailableDeviceTypes().getCount(), 0);
        EXPECT_EQ(mockDevice.getAvailableDeviceTypes().getCount(), 0);

        // getAvailableDevices() has not been implemented for a device (client and server sides),
        // so it will return 0 regardless of permissions.
        EXPECT_EQ(clientDevice.getAvailableDevices().getCount(), 0);
        EXPECT_EQ(mockDevice.getAvailableDevices().getCount(), 0);
    }
}

TEST_P(TmsUserAccessPTest, AddRemoveFb)
{
    using UP = UserPermission::PermissionType;
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    if (userPerm.hasPermission(UP::Write | UP::Execute))
    {
        EXPECT_NO_THROW(clientDevice.addFunctionBlock("mock_fb_uid"));
        EXPECT_NO_THROW(clientDevice.removeFunctionBlock(mockFb));
    }
    else
    {
        EXPECT_ANY_THROW(clientDevice.addFunctionBlock("mock_fb_uid"));
        EXPECT_ANY_THROW(clientDevice.removeFunctionBlock(mockFb));
    }
}

TEST_P(TmsUserAccessPTest, DeviceOperationMode)
{
    using UP = UserPermission::PermissionType;
    using OMT = daq::OperationModeType;
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    if (userPerm.hasPermission(UP::Write))
    {
        OMT op;
        ASSERT_NO_THROW(clientDevice.setOperationMode(OMT::Idle));
        ASSERT_NO_THROW(op = clientDevice.getOperationMode());
        EXPECT_EQ(op, OMT::Idle);

        ASSERT_NO_THROW(clientDevice.setOperationMode(OMT::SafeOperation));
        ASSERT_NO_THROW(op = clientDevice.getOperationMode());
        EXPECT_EQ(op, OMT::SafeOperation);

        ASSERT_NO_THROW(clientDevice.setOperationMode(OMT::Operation));
        ASSERT_NO_THROW(op = clientDevice.getOperationMode());
        EXPECT_EQ(op, OMT::Operation);
    }
    else
    {
        OMT op = OMT::Idle;
        ASSERT_NO_THROW(op = clientDevice.getOperationMode());

        OMT opToSet = (op == OMT::Idle) ? OMT::SafeOperation : OMT::Idle;
        EXPECT_ANY_THROW(clientDevice.setOperationMode(opToSet));
        ASSERT_NO_THROW(op = clientDevice.getOperationMode());
        EXPECT_NE(op, opToSet);
    }
}

TEST_P(TmsUserAccessPTest, Connect)
{
    using UP = UserPermission::PermissionType;
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    daq::ListPtr<daq::IInputPort> ip;
    daq::ListPtr<daq::ISignal> sig;
    ASSERT_NO_THROW(ip = mockFb.getInputPorts());
    ASSERT_NE(ip.getCount(), 0);
    ASSERT_NO_THROW(sig = mockDevice.getSignals());
    ASSERT_NE(sig.getCount(), 0);

    if (userPerm.hasPermission(UP::Write | UP::Execute))
    {
        ASSERT_NO_THROW(ip[0].connect(sig[0]));
    }
    else
    {
        ASSERT_ANY_THROW(ip[0].connect(sig[0]));
    }
}

TEST_P(TmsUserAccessPTest, Disconnect)
{
    using UP = UserPermission::PermissionType;
    const UserPermission userPerm(GetParam());

    fb.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    device.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());
    instance.getPermissionManager().setPermissions(test_helpers::CreatePermissionsBuilder().build());

    {
        daq::ListPtr<daq::IInputPort> ip;
        daq::ListPtr<daq::ISignal> sig;
        ASSERT_NO_THROW(ip = fb.getInputPorts());
        ASSERT_NE(ip.getCount(), 0);
        ASSERT_NO_THROW(sig = device.getSignals());
        ASSERT_NE(sig.getCount(), 0);
        ASSERT_NO_THROW(ip[0].connect(sig[0]));
    }

    auto tmsPropertyObject = TmsServerDevice(instance, this->getServer(), ctx, serverContext);
    auto nodeId = tmsPropertyObject.registerOpcUaNode();
    auto ctx = NullContext();
    CreateClient(userPerm);
    DevicePtr clientDevice;
    ASSERT_NO_THROW(clientDevice = TmsClientRootDevice(ctx, nullptr, "dev", clientContext, nodeId));

    ASSERT_EQ(clientDevice.getDevices().getCount(), 2);
    ASSERT_EQ(clientDevice.getFunctionBlocks().getCount(), 1);

    DevicePtr mockDevice = clientDevice.getDevices()[1];
    FunctionBlockPtr mockFb = clientDevice.getFunctionBlocks()[0];

    daq::ListPtr<daq::IInputPort> ip;
    ASSERT_NO_THROW(ip = mockFb.getInputPorts());
    ASSERT_NE(ip.getCount(), 0);

    if (userPerm.hasPermission(UP::Write | UP::Execute))
    {
        ASSERT_NO_THROW(ip[0].disconnect());
    }
    else
    {
        ASSERT_ANY_THROW(ip[0].disconnect());
    }
}

INSTANTIATE_TEST_SUITE_P(UserAccess,
                         TmsUserAccessPTest,
                         ::testing::Values(UserPermission::PermissionType::Read,
                                           UserPermission::PermissionType::Read | UserPermission::PermissionType::Write,
                                           UserPermission::PermissionType::Read | UserPermission::PermissionType::Execute,
                                           UserPermission::PermissionType::Read | UserPermission::PermissionType::Write | UserPermission::PermissionType::Execute), ParamNameGenerator);
