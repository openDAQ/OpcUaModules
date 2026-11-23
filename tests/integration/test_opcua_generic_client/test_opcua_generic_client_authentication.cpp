#include <coreobjects/exceptions.h>
#include <opendaq/authentication_method_factory.h>
#include <opcua_generic_client_module/module_dll.h>
#include <opendaq/component_type_private_ptr.h>
#include <opendaq/config_provider_factory.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/instance_factory.h>
#include <opendaq/security_exceptions.h>
#include <testutils/testutils.h>
#include "opcua_test_server.h"
#include <chrono>
#include <thread>

using namespace daq;

// File-private helpers in a namespace unique to the file - a unity build merges the test sources into one translation unit
namespace generic_client_authentication_test
{

constexpr UA_UInt16 ServerPort = 4842;
constexpr const char* ConnectionString = "daq.opcua.generic://127.0.0.1:4842";
constexpr const char* DeviceTypeId = "OPCUAGeneric";
constexpr const char* UserName = "jure";
constexpr const char* Password = "jure123";

// Creates the add-device config the credentials are supplied with:
// `useLegacyConfig == true` - the legacy default add-device config, via the "Username"/"Password" of its device type config,
// `useLegacyConfig == false` - an IAddDeviceConfig, via the supplied credential of its authentication config
PropertyObjectPtr createAddDeviceConfig(const InstancePtr& instance, bool useLegacyConfig)
{
    if (!useLegacyConfig)
        return instance.createAddDeviceConfig(DeviceInfo(ConnectionString)).asPtr<IPropertyObject>();
    // The legacy config mode exercises the still-functional but deprecated legacy add-device config API
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4996)
#endif
    return instance.createDefaultAddDeviceConfig();
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
}

void setCredentials(const PropertyObjectPtr& config, const std::string& userName, const std::string& password, bool useLegacyConfig)
{
    if (useLegacyConfig)
    {
        const PropertyObjectPtr deviceTypeConfig = config.getPropertyValue(std::string("Device.") + DeviceTypeId);
        deviceTypeConfig.setPropertyValue("Username", userName);
        deviceTypeConfig.setPropertyValue("Password", password);
        return;
    }

    const auto authConfig = config.asPtr<IAddDeviceConfig>().getAuthenticationConfig();
    authConfig.setAuthenticationMethod(StandardUserNamePasswordId);
    authConfig.setSuppliedCredential(Dict<IString, IString>({{"UserName", userName}, {"Password", password}}));
}

// An instance with no modules but the generic OPC UA client module - no module folder is scanned, nor devices on add
InstancePtr createInstance()
{
    auto instance = InstanceBuilder()
                        .addConfigProvider(CmdLineArgsConfigProvider(List<IString>("-CModuleManager_ScanOnAdd=false")))
                        .setModulePath("[[none]]")
                        .build();

    ModulePtr module;
    createModule(&module, instance.getContext());
    instance.getModuleManager().addModule(module);
    return instance;
}

std::string configModeName(const testing::TestParamInfo<bool>& info)
{
    return info.param ? "LegacyConfig" : "AddDeviceConfig";
}

}  // namespace generic_client_authentication_test

using namespace generic_client_authentication_test;

class GenericOpcuaClientAuthenticationTest : public testing::Test
{
protected:
    void SetUp() override
    {
        instance = createInstance();
    }

    void TearDown() override
    {
        // remove the devices while the server is still up
        instance.release();
        server.stop();
    }

    void startServer(bool allowAnonymous, const std::string& password = Password)
    {
        server.start(allowAnonymous, UserName, password);
    }

    PropertyObjectPtr createConfig(bool useLegacyConfig)
    {
        return createAddDeviceConfig(instance, useLegacyConfig);
    }

    PropertyObjectPtr createConfig(const std::string& userName, const std::string& password, bool useLegacyConfig)
    {
        auto config = createAddDeviceConfig(instance, useLegacyConfig);
        setCredentials(config, userName, password, useLegacyConfig);
        return config;
    }

    bool waitForConnectionStatus(const DevicePtr& device, const std::string& expected, std::chrono::milliseconds timeout)
    {
        const auto expectedStatus = Enumeration("ConnectionStatusType", expected, instance.getContext().getTypeManager());
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (device.getStatusContainer().getStatus("ConnectionStatus") == expectedStatus)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    InstancePtr instance;
    OpcUaTestServer server{ServerPort};
};

class GenericOpcuaClientAuthenticationConfigTest : public GenericOpcuaClientAuthenticationTest, public testing::WithParamInterface<bool>
{
};

TEST_F(GenericOpcuaClientAuthenticationTest, DeviceTypeAuthenticationMethods)
{
    const auto deviceType = instance.getAvailableDeviceTypes().get(DeviceTypeId);

    const auto methods = deviceType.asPtr<IComponentTypePrivate>().getSupportedAuthenticationMethods();
    ASSERT_EQ(methods.getCount(), 2u);
    ASSERT_TRUE(methods.hasKey(StandardAnonymousId));
    ASSERT_TRUE(methods.hasKey(StandardUserNamePasswordId));

    // "Anonymous" is added first, so it is the default selection
    ASSERT_EQ(deviceType.createAuthenticationConfig().getSelectedAuthenticationMethod().getId(), StandardAnonymousId);
}

TEST_P(GenericOpcuaClientAuthenticationConfigTest, AnonymousProhibited)
{
    const bool useLegacyConfig = GetParam();
    startServer(false);

    // "Anonymous" is the default authentication method
    ASSERT_THROW(instance.addDevice(ConnectionString), AuthenticationFailedException);
    ASSERT_THROW(instance.addDevice(ConnectionString, createConfig(useLegacyConfig)), AuthenticationFailedException);

    ASSERT_THROW(instance.addDevice(ConnectionString, createConfig(UserName, "wrongPass", useLegacyConfig)), AuthenticationFailedException);
    ASSERT_THROW(instance.addDevice(ConnectionString, createConfig("andrej", "andrej123", useLegacyConfig)), AuthenticationFailedException);

    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(ConnectionString, createConfig(UserName, Password, useLegacyConfig)));
    ASSERT_TRUE(device.assigned());
    ASSERT_EQ(device.getInfo().getPropertyValue("userName"), UserName);
}

TEST_P(GenericOpcuaClientAuthenticationConfigTest, AnonymousAllowed)
{
    const bool useLegacyConfig = GetParam();
    startServer(true);

    DevicePtr anonymousDevice;
    ASSERT_NO_THROW(anonymousDevice = instance.addDevice(ConnectionString, createConfig(useLegacyConfig)));
    ASSERT_TRUE(anonymousDevice.assigned());
    ASSERT_EQ(anonymousDevice.getInfo().getPropertyValue("userName"), "");
    instance.removeDevice(anonymousDevice);

    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(ConnectionString, createConfig(UserName, Password, useLegacyConfig)));
    ASSERT_TRUE(device.assigned());
    ASSERT_EQ(device.getInfo().getPropertyValue("userName"), UserName);
}

// A reconnection the server rejects (the user's password changed meanwhile) is not retried
TEST_P(GenericOpcuaClientAuthenticationConfigTest, ReconnectRejectedIsUnrecoverable)
{
    using namespace std::chrono_literals;

    const bool useLegacyConfig = GetParam();
    startServer(false);

    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(ConnectionString, createConfig(UserName, Password, useLegacyConfig)));
    ASSERT_TRUE(waitForConnectionStatus(device, "Connected", 10s));

    server.stop();
    ASSERT_TRUE(waitForConnectionStatus(device, "Reconnecting", 20s));

    startServer(false, "newPass");
    // the reconnect interval of the device is 5 s
    ASSERT_TRUE(waitForConnectionStatus(device, "Unrecoverable", 20s));
}

INSTANTIATE_TEST_SUITE_P(ConfigMode, GenericOpcuaClientAuthenticationConfigTest, testing::Bool(), configModeName);

TEST_F(GenericOpcuaClientAuthenticationTest, UserNamePasswordWithoutCredentials)
{
    startServer(false);

    // no supplied credential and no credential provider registered
    const auto config = instance.createAddDeviceConfig(DeviceInfo(ConnectionString));
    config.getAuthenticationConfig().setAuthenticationMethod(StandardUserNamePasswordId);
    ASSERT_THROW(instance.addDevice(ConnectionString, config.asPtr<IPropertyObject>()), CredentialsRequiredException);
}

// The device type config with the "Username"/"Password" properties, passed to `addDevice` as is
TEST_F(GenericOpcuaClientAuthenticationTest, LegacyTypeConfigCredentials)
{
    startServer(false);

    const auto createTypeConfig = [this](const std::string& userName, const std::string& password)
    {
        auto config = instance.getAvailableDeviceTypes().get(DeviceTypeId).createDefaultConfig();
        config.setPropertyValue("Username", userName);
        config.setPropertyValue("Password", password);
        return config;
    };

    ASSERT_THROW(instance.addDevice(ConnectionString, createTypeConfig("", "")), AuthenticationFailedException);
    // a password without a user name is ignored - still anonymous
    ASSERT_THROW(instance.addDevice(ConnectionString, createTypeConfig("", Password)), AuthenticationFailedException);
    ASSERT_THROW(instance.addDevice(ConnectionString, createTypeConfig(UserName, "wrongPass")), AuthenticationFailedException);

    DevicePtr device;
    ASSERT_NO_THROW(device = instance.addDevice(ConnectionString, createTypeConfig(UserName, Password)));
    ASSERT_TRUE(device.assigned());
    ASSERT_EQ(device.getInfo().getPropertyValue("userName"), UserName);
}
