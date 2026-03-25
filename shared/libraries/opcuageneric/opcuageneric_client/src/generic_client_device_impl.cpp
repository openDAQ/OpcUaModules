#include <opcuageneric_client/generic_client_device_impl.h>

#include <opendaq/device_info_factory.h>
#include <opendaq/function_block_type_factory.h>
#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include "opcuashared/opcuaendpoint.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

std::atomic<int> OpcuaGenericClientDeviceImpl::localIndex = 0;

OpcuaGenericClientDeviceImpl::OpcuaGenericClientDeviceImpl(const ContextPtr& ctx, const ComponentPtr& parent, const PropertyObjectPtr& config)
    : Device(ctx, parent, generateLocalId()),
    connectionStatus(Enumeration("ConnectionStatusType", "Connected", this->context.getTypeManager()))
{
    this->name = GENERIC_OPCUA_CLIENT_DEVICE_NAME;

    const auto host = config.getPropertyValue(PROPERTY_NAME_OPCUA_HOST).asPtr<IString>().toStdString();
    const auto port = config.getPropertyValue(PROPERTY_NAME_OPCUA_PORT).asPtr<IInteger>().getValue(DEFAULT_OPCUA_PORT);
    const auto path = config.getPropertyValue(PROPERTY_NAME_OPCUA_PATH).asPtr<IString>().toStdString();

    connectionString = std::string(OpcUaGenericScheme) + "://" + host + ":" + std::to_string(port) + path;

    auto endpoint = OpcUaEndpoint(connectionString);

    endpoint.setUsername(config.getPropertyValue(PROPERTY_NAME_OPCUA_USERNAME));
    endpoint.setPassword(config.getPropertyValue(PROPERTY_NAME_OPCUA_PASSWORD));

    try
    {
        client = std::make_shared<OpcUaClient>(endpoint);
        client->connect();
        client->runIterate();
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


    initComponentStatus();
    initNestedFbTypes();
}

PropertyObjectPtr OpcuaGenericClientDeviceImpl::createDefaultConfig()
{
    auto defaultConfig = PropertyObject();

    defaultConfig.addProperty(StringProperty(PROPERTY_NAME_OPCUA_HOST, DEFAULT_OPCUA_HOST));
    defaultConfig.addProperty(IntProperty(PROPERTY_NAME_OPCUA_PORT, DEFAULT_OPCUA_PORT));
    defaultConfig.addProperty(StringProperty(PROPERTY_NAME_OPCUA_PATH, DEFAULT_OPCUA_PATH));
    defaultConfig.addProperty(StringProperty(PROPERTY_NAME_OPCUA_USERNAME, DEFAULT_OPCUA_USERNAME));
    defaultConfig.addProperty(StringProperty(PROPERTY_NAME_OPCUA_PASSWORD, DEFAULT_OPCUA_PASSWORD));

    return defaultConfig;
}

void OpcuaGenericClientDeviceImpl::removed()
{
    Device::removed();
}

DeviceInfoPtr OpcuaGenericClientDeviceImpl::onGetInfo()
{
    return DeviceInfo(connectionString, GENERIC_OPCUA_CLIENT_DEVICE_NAME);
}


void OpcuaGenericClientDeviceImpl::initNestedFbTypes()
{
    nestedFbTypes = Dict<IString, IFunctionBlockType>();
    // Add a function block type for monitoring an OPCUA node
    {
        const auto fbType = OpcUaMonitoredItemFbImpl::CreateType();
        nestedFbTypes.set(fbType.getId(), fbType);
    }
}


DictPtr<IString, IFunctionBlockType> OpcuaGenericClientDeviceImpl::onGetAvailableFunctionBlockTypes()
{
    return nestedFbTypes;
}

FunctionBlockPtr OpcuaGenericClientDeviceImpl::onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config)
{
    FunctionBlockPtr nestedFunctionBlock;
    {
        if (nestedFbTypes.hasKey(typeId))
        {
            auto fbTypePtr = nestedFbTypes.getOrDefault(typeId);
            if (fbTypePtr.getName() == GENERIC_OPCUA_MONITORED_ITEM_FB_NAME)
            {
                nestedFunctionBlock = createWithImplementation<IFunctionBlock, OpcUaMonitoredItemFbImpl>(context, functionBlocks, fbTypePtr, client, config);
            }
            else
            {
                setComponentStatusWithMessage(ComponentStatus::Error, "Function block type is not available: " + typeId.toStdString());
                return nestedFunctionBlock;
            }
        }
        if (nestedFunctionBlock.assigned())
        {
            addNestedFunctionBlock(nestedFunctionBlock);
            setComponentStatus(ComponentStatus::Ok);
        }
        else
        {
            DAQ_THROW_EXCEPTION(NotFoundException, "Function block type is not available: " + typeId.toStdString());
        }
    }
    return nestedFunctionBlock;
}

std::string OpcuaGenericClientDeviceImpl::generateLocalId()
{
    return std::string(GENERIC_OPCUA_CLIENT_DEVICE_NAME + std::to_string(localIndex++));
}
END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
