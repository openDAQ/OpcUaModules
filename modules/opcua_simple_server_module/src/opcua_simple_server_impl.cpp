#include <opcua_simple_server_module/opcua_simple_server_impl.h>
#include <opcua_simple_server_module/constants.h>
#include <coretypes/impl.h>
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_factory.h>
#include <opendaq/server_type_factory.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/device_info_internal_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_SERVER_MODULE
using namespace daq;
using namespace daq::opcua;

OpcUaSimpleServerImpl::OpcUaSimpleServerImpl(const DevicePtr& rootDevice,
                                 const PropertyObjectPtr& config,
                                 const ContextPtr& context)
    : Server(DAQ_OPCUA_SIMPLE_SERVER_ID, config, rootDevice, context)
    , server(rootDevice, context)
    , context(context)
{
    const uint16_t port = config.getPropertyValue("Port");

    server.setOpcUaPort(port);
    server.setOpcUaPath(config.getPropertyValue("Path"));
    server.start();
}

OpcUaSimpleServerImpl::~OpcUaSimpleServerImpl()
{
}

void OpcUaSimpleServerImpl::populateDefaultConfigFromProvider(const ContextPtr& context, const PropertyObjectPtr& config)
{
    if (!context.assigned())
        return;
    if (!config.assigned())
        return;

    auto options = context.getModuleOptions(DAQ_OPCUA_SIMPLE_SERVER_MODULE_ID);
    for (const auto& [key, value] : options)
    {
        if (config.hasProperty(key))
        {
            config->setPropertyValue(key, value);
        }
    }
}

PropertyObjectPtr OpcUaSimpleServerImpl::createDefaultConfig(const ContextPtr& context)
{
    constexpr Int minPortValue = 0;
    constexpr Int maxPortValue = 65535;

    auto defaultConfig = PropertyObject();

    const auto portProp = IntPropertyBuilder("Port", DAQ_OPCUA_SIMPLE_SERVER_DEFAULT_PORT)
        .setMinValue(minPortValue)
        .setMaxValue(maxPortValue)
        .build();
    defaultConfig.addProperty(portProp);

    defaultConfig.addProperty(StringProperty("Path", DAQ_OPCUA_SIMPLE_SERVER_DEFAULT_PATH));

    populateDefaultConfigFromProvider(context, defaultConfig);
    return defaultConfig;
}

PropertyObjectPtr OpcUaSimpleServerImpl::populateDefaultConfig(const PropertyObjectPtr& config, const ContextPtr& context)
{
    const auto defConfig = createDefaultConfig(context);
    for (const auto& prop : defConfig.getAllProperties())
    {
        const auto name = prop.getName();
        if (config.hasProperty(name))
            defConfig.setPropertyValue(name, config.getPropertyValue(name));
    }

    return defConfig;
}

PropertyObjectPtr OpcUaSimpleServerImpl::getDiscoveryConfig()
{
    auto discoveryConfig = PropertyObject();
    discoveryConfig.addProperty(StringProperty("ServiceName", "_opcua-tcp._tcp.local."));
    discoveryConfig.addProperty(StringProperty("ServiceCap", "OPENDAQ"));
    discoveryConfig.addProperty(StringProperty("Path", config.getPropertyValue("Path")));
    discoveryConfig.addProperty(IntProperty("Port", config.getPropertyValue("Port")));
    discoveryConfig.addProperty(StringProperty("ProtocolVersion", ""));
    return discoveryConfig;
}

ServerTypePtr OpcUaSimpleServerImpl::createType(const ContextPtr& context)
{
    return ServerType(DAQ_OPCUA_SIMPLE_SERVER_ID,
                      "openDAQ simple OPC UA server",
                      "Publishes signal nodes over OPC UA protocol",
                      OpcUaSimpleServerImpl::createDefaultConfig(context));
}

void OpcUaSimpleServerImpl::onStopServer()
{
    server.stop();
}

OPENDAQ_DEFINE_CLASS_FACTORY_WITH_INTERFACE(
    INTERNAL_FACTORY, OpcUaSimpleServer, daq::IServer,
    daq::DevicePtr, rootDevice,
    PropertyObjectPtr, config,
    const ContextPtr&, context
)

END_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_SERVER_MODULE
