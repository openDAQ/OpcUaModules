#include <coretypes/version_info_factory.h>
#include <opcua_simple_server_module/opcua_simple_server_impl.h>
#include <opcua_simple_server_module/opcua_simple_server_module_impl.h>
#include <opcua_simple_server_module/version.h>
#include "opcua_simple_server_module/constants.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_SERVER_MODULE

OpcUaSimpleServerModule::OpcUaSimpleServerModule(ContextPtr context)
    : Module(DAQ_OPCUA_SIMPLE_SERVER_MODULE_NAME,
             daq::VersionInfo(OPCUA_SIMPLE_SERVER_MODULE_MAJOR_VERSION, OPCUA_SIMPLE_SERVER_MODULE_MINOR_VERSION, OPCUA_SIMPLE_SERVER_MODULE_PATCH_VERSION),
             std::move(context),
             DAQ_OPCUA_SIMPLE_SERVER_MODULE_ID)
{
}

DictPtr<IString, IServerType> OpcUaSimpleServerModule::onGetAvailableServerTypes()
{
    auto result = Dict<IString, IServerType>();

    auto serverType = OpcUaSimpleServerImpl::createType(context);
    result.set(serverType.getId(), serverType);

    return result;
}

ServerPtr OpcUaSimpleServerModule::onCreateServer(const StringPtr& serverType,
                                            const PropertyObjectPtr& serverConfig,
                                            const DevicePtr& rootDevice)
{
    if (!context.assigned())
        DAQ_THROW_EXCEPTION(InvalidParameterException, "Context parameter cannot be null.");

    PropertyObjectPtr config = serverConfig;
    if (!config.assigned())
        config = OpcUaSimpleServerImpl::createDefaultConfig(context);
    else
        config = OpcUaSimpleServerImpl::populateDefaultConfig(config, context);

    ServerPtr server(OpcUaSimpleServer_Create(rootDevice, config, context));
    return server;
}

END_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_SERVER_MODULE
