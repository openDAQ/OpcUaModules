#include <opcua_generic_client_module/opcua_generic_client_module_impl.h>
#include <opcua_generic_client_module/version.h>
#include <opcua_generic_client_module/constants.h>
#include <coretypes/version_info_factory.h>
#include <opcuageneric_client/constants.h>
#include <opendaq/custom_log.h>
#include <coreobjects/property_object_factory.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/mirrored_signal_config_ptr.h>
#include <opendaq/search_filter_factory.h>
#include <regex>
#include <opendaq/address_info_factory.h>
#include <coreobjects/property_factory.h>
#include <opcuageneric_client/generic_client_device_impl.h>

#include <opendaq/mirrored_device_config_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC_CLIENT_MODULE

static const std::regex RegexIpv6Hostname(R"(^(.+://)?(\[[a-fA-F0-9:]+(?:\%[a-zA-Z0-9_\.-~]+)?\])(?::(\d+))?(/.*)?$)");
static const std::regex RegexIpv4Hostname(R"(^(.+://)?([^:/\s]+)(?::(\d+))?(/.*)?$)");

using namespace discovery;
using namespace daq::opcua;
using namespace daq::opcua::generic;

OpcUaGenericClientModule::OpcUaGenericClientModule(ContextPtr context)
    : Module(MODULE_NAME,
             daq::VersionInfo(OPCUA_GENERIC_CLIENT_MODULE_MAJOR_VERSION,
                              OPCUA_GENERIC_CLIENT_MODULE_MINOR_VERSION,
                              OPCUA_GENERIC_CLIENT_MODULE_PATCH_VERSION),
             std::move(context),
             MODULE_ID)
    , discoveryClient()
{
    loggerComponent = this->context.getLogger().getOrAddComponent(DaqOpcUaGenericComponentName);
    discoveryClient.initMdnsClient(List<IString>("_opcua-tcp._tcp.local."));
}

ListPtr<IDeviceInfo> OpcUaGenericClientModule::onGetAvailableDevices()
{
    auto availableDevices = List<IDeviceInfo>();
    for (const auto& device : discoveryClient.discoverMdnsDevices())
        availableDevices.pushBack(populateDiscoveredDevice(device));
    return availableDevices;
}

DictPtr<IString, IDeviceType> OpcUaGenericClientModule::onGetAvailableDeviceTypes()
{
    auto result = Dict<IString, IDeviceType>();

    auto deviceType = createDeviceType();
    result.set(deviceType.getId(), deviceType);
    return result;
}

DevicePtr OpcUaGenericClientModule::onCreateDevice(const StringPtr& connectionString,
                                            const ComponentPtr& parent,
                                            const PropertyObjectPtr& config)
{
    if (!connectionString.assigned())
        DAQ_THROW_EXCEPTION(ArgumentNullException);

    PropertyObjectPtr configPtr = config;
    if (!configPtr.assigned())
        configPtr = createDefaultConfig();
    else
        configPtr = populateDefaultConfig(configPtr);

    if (!acceptsConnectionParameters(connectionString, configPtr))
        DAQ_THROW_EXCEPTION(InvalidParameterException);

    if (!context.assigned())
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Context is not available.");

    std::string host;
    std::string hostType;
    int port;
    const auto opcuaConnStr = formConnectionString(connectionString, configPtr, host, port, hostType);

    std::scoped_lock lock(sync);

    std::shared_ptr<OpcUaClient> client;
    std::string deviceName;
    std::string deviceLocalId;

    try
    {
        client = std::make_shared<OpcUaClient>(OpcUaEndpoint(opcuaConnStr,
                                                             configPtr.getPropertyValue(PROPERTY_NAME_OPCUA_USERNAME),
                                                             configPtr.getPropertyValue(PROPERTY_NAME_OPCUA_PASSWORD)));
        const auto desc = client->readApplicationDescription();

        deviceName = desc.name.empty() ? GENERIC_OPCUA_CLIENT_DEVICE_NAME : desc.name;
        deviceLocalId = desc.uri.empty() ? "" : desc.uri;
        std::replace(deviceLocalId.begin(), deviceLocalId.end(), '/', '-');
    }

    catch (const OpcUaException& e)
    {
        switch (e.getStatusCode())
        {
            case UA_STATUSCODE_BADUSERACCESSDENIED:
            case UA_STATUSCODE_BADIDENTITYTOKENINVALID:
                DAQ_THROW_EXCEPTION(AuthenticationFailedException, e.what());
            default:
                DAQ_THROW_EXCEPTION(NotFoundException, e.what());
        }
    }

    DevicePtr device(createWithImplementation<IDevice, OpcuaGenericClientDeviceImpl>(context, parent, configPtr, client, deviceLocalId, deviceName));

    // Set the connection info for the device
    DeviceInfoPtr deviceInfo = device.getInfo();
    deviceInfo.asPtr<IPropertyObjectProtected>().setProtectedPropertyValue("connectionString", connectionString);
    ServerCapabilityConfigPtr connectionInfo = deviceInfo.getConfigurationConnectionInfo();
    
    const auto addressInfo = AddressInfoBuilder().setAddress(host)
                                                 .setReachabilityStatus(AddressReachabilityStatus::Reachable)
                                                 .setType(hostType)
                                                 .setConnectionString(connectionString)
                                                 .build();

    connectionInfo.setProtocolId(DaqOpcUaGenericProtocolId)
                  .setProtocolName(DaqOpcUaGenericProtocolName)
                  .setProtocolType(ProtocolType::Unknown)
                  .setConnectionType("TCP/IP")
                  .addAddress(host)
                  .setPort(port)
                  .setPrefix(DaqOpcUaGenericDevicePrefix)
                  .setConnectionString(connectionString)
                  .addAddressInfo(addressInfo)
                  .freeze();

    return device;
}

PropertyObjectPtr OpcUaGenericClientModule::populateDefaultConfig(const PropertyObjectPtr& config)
{
    const auto defConfig = createDefaultConfig();
    for (const auto& prop : defConfig.getAllProperties())
    {
        const auto name = prop.getName();
        if (config.hasProperty(name))
            defConfig.setPropertyValue(name, config.getPropertyValue(name));
    }

    return defConfig;
}

DeviceInfoPtr OpcUaGenericClientModule::populateDiscoveredDevice(const MdnsDiscoveredDevice& discoveredDevice)
{
    auto cap = ServerCapability(DaqOpcUaGenericProtocolId, DaqOpcUaGenericProtocolName, ProtocolType::Unknown);

    for (const auto& ipAddress : discoveredDevice.ipv4Addresses)
    {
        auto connectionStringIpv4 = fmt::format("{}://{}:{}{}",
                                                DaqOpcUaGenericDevicePrefix,
                                                ipAddress,
                                                discoveredDevice.servicePort,
                                                discoveredDevice.getPropertyOrDefault("path", "/"));
        cap.addConnectionString(connectionStringIpv4);
        cap.addAddress(ipAddress);

        const auto addressInfo = AddressInfoBuilder().setAddress(ipAddress)
                                     .setReachabilityStatus(AddressReachabilityStatus::Unknown)
                                     .setType("IPv4")
                                     .setConnectionString(connectionStringIpv4)
                                     .build();
        cap.addAddressInfo(addressInfo);
    }

    for (const auto& ipAddress : discoveredDevice.ipv6Addresses)
    {
        auto connectionStringIpv6 = fmt::format("{}://{}:{}{}",
                                                DaqOpcUaGenericDevicePrefix,
                                                ipAddress,
                                                discoveredDevice.servicePort,
                                                discoveredDevice.getPropertyOrDefault("path", "/"));
        cap.addConnectionString(connectionStringIpv6);
        cap.addAddress(ipAddress);

        const auto addressInfo = AddressInfoBuilder().setAddress(ipAddress)
                                     .setReachabilityStatus(AddressReachabilityStatus::Unknown)
                                     .setType("IPv6")
                                     .setConnectionString(connectionStringIpv6)
                                     .build();
        cap.addAddressInfo(addressInfo);
    }

    cap.setConnectionType("TCP/IP");
    cap.setPrefix(DaqOpcUaGenericDevicePrefix);
    cap.setProtocolVersion("");
    if (discoveredDevice.servicePort > 0)
        cap.setPort(discoveredDevice.servicePort);

    auto devInfo = populateDiscoveredDeviceInfo(DiscoveryClient::populateDiscoveredInfoProperties, discoveredDevice, cap, createDeviceType());
    devInfo.asPtr<IDeviceInfoConfig>().setName(discoveredDevice.serviceInstance);
    return devInfo;
}

StringPtr OpcUaGenericClientModule::formConnectionString(const StringPtr& connectionString, const PropertyObjectPtr& config, std::string& host, int& port, std::string& hostType)
{
    std::string urlString = connectionString.toStdString();
    std::smatch match;

    std::string prefix = "";
    std::string path = "/";

    if (config.assigned())
    {
        if (config.hasProperty(PROPERTY_NAME_OPCUA_PORT))
            port = config.getPropertyValue(PROPERTY_NAME_OPCUA_PORT);

        if (config.hasProperty(PROPERTY_NAME_OPCUA_PATH))
            path = String(config.getPropertyValue(PROPERTY_NAME_OPCUA_PATH)).toStdString();
    }

    bool parsed = false;
    parsed = std::regex_search(urlString, match, RegexIpv6Hostname);
    hostType = "IPv6";

    if (!parsed)
    {
        parsed = std::regex_search(urlString, match, RegexIpv4Hostname);
        hostType = "IPv4";
    }

    if (parsed)
    {
        prefix = match[1];
        host = match[2];

        if (match[3].matched)
            port = std::stoi(match[3]);

        if (match[4].matched)
            path = match[4];
    }
    else
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Host name not found in url: {}", connectionString);

    if (prefix != std::string(DaqOpcUaGenericDevicePrefix) + "://")
        DAQ_THROW_EXCEPTION(InvalidParameterException, "OpcUa does not support connection string with prefix {}", prefix);


    if (config.assigned())
    {
        if (config.hasProperty(PROPERTY_NAME_OPCUA_PORT))
            config.setPropertyValue(PROPERTY_NAME_OPCUA_PORT, port);

        if (config.hasProperty(PROPERTY_NAME_OPCUA_PATH))
            config.setPropertyValue(PROPERTY_NAME_OPCUA_PATH, path);

        if (config.hasProperty(PROPERTY_NAME_OPCUA_HOST))
            config.setPropertyValue(PROPERTY_NAME_OPCUA_HOST, host);
    }

    return std::string(OpcUaGenericScheme) + "://" + host + ":" + std::to_string(port) + path;
}

bool OpcUaGenericClientModule::acceptsConnectionParameters(const StringPtr& connectionString, const PropertyObjectPtr& config)
{
    std::string connStr = connectionString;
    auto found = connStr.find(std::string(DaqOpcUaGenericDevicePrefix) + "://");
    return found == 0;
}

Bool OpcUaGenericClientModule::onCompleteServerCapability(const ServerCapabilityPtr& source, const ServerCapabilityConfigPtr& target)
{
    if (target.getProtocolId() != DaqOpcUaGenericProtocolId)
        return false;

    if (source.getConnectionType() != "TCP/IP")
        return false;

    if (!source.getAddresses().assigned() || !source.getAddresses().getCount())
    {
        LOG_W("Source server capability address is not available when filling in missing OPC UA capability information.")
        return false;
    }

    const auto addrInfos = source.getAddressInfo();
    if (!addrInfos.assigned() || !addrInfos.getCount())
    {
        LOG_W("Source server capability addressInfo is not available when filling in missing OPC UA capability information.")
        return false;
    }

    auto port = target.getPort();
    if (port == -1)
    {
        port = DEFAULT_OPCUA_PORT;
        target.setPort(port);
        LOG_W("OPC UA server capability is missing port. Defaulting to {}.", DEFAULT_OPCUA_PORT)
    }

    const auto path = target.hasProperty(PROPERTY_NAME_OPCUA_PATH) ? target.getPropertyValue(PROPERTY_NAME_OPCUA_PATH) : DEFAULT_OPCUA_PATH;
    const auto targetAddress = target.getAddresses();
    for (const auto& addrInfo : addrInfos)
    {
        const auto address = addrInfo.getAddress();
        if (auto it = std::find(targetAddress.begin(), targetAddress.end(), address); it != targetAddress.end())
            continue;

        const auto prefix = target.getPrefix();
        StringPtr connectionString;
        if (source.getPrefix() == prefix)
            connectionString = addrInfo.getConnectionString();
        else
            connectionString = fmt::format("{}://{}:{}{}", prefix, address, port, path);

        const auto targetAddrInfo = AddressInfoBuilder()
                                        .setAddress(address)
                                        .setReachabilityStatus(addrInfo.getReachabilityStatus())
                                        .setType(addrInfo.getType())
                                        .setConnectionString(connectionString)
                                        .build();

        target.addAddressInfo(targetAddrInfo)
              .setConnectionString(connectionString)
              .addAddress(address);
    }

    return true;
}

DeviceTypePtr OpcUaGenericClientModule::createDeviceType()
{
    return DeviceTypeBuilder()
        .setId(DaqOpcUaGenericProtocolId)
        .setName("OpcUa enabled device")
        .setDescription("Network device connected over OpcUa protocol")
        .setConnectionStringPrefix(DaqOpcUaGenericDevicePrefix)
        .setDefaultConfig(createDefaultConfig())
        .build();
}

PropertyObjectPtr OpcUaGenericClientModule::createDefaultConfig()
{
    return OpcuaGenericClientDeviceImpl::createDefaultConfig();
}

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC_CLIENT_MODULE
