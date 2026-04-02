#include <opcuageneric_client/generic_client_device_impl.h>

#include <opendaq/device_info_factory.h>
#include <opendaq/function_block_type_factory.h>
#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>
#include "opcuashared/opcuaendpoint.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

std::atomic<int> OpcuaGenericClientDeviceImpl::localIndex = 0;

namespace
{
    PropertyObjectPtr populateDefaultConfig(const PropertyObjectPtr& defaultConfig, const PropertyObjectPtr& config)
    {
        auto newConfig = PropertyObject();
        for (const auto& prop : defaultConfig.getAllProperties())
        {
            if (prop.getName() == PROPERTY_NAME_OPCUA_MI_LOCAL_ID)
                continue;
            newConfig.addProperty(prop.asPtr<IPropertyInternal>(true).clone());
            const auto propName = prop.getName();
            newConfig.setPropertyValue(propName, config.hasProperty(propName) ? config.getPropertyValue(propName) : prop.getValue());
        }
        return newConfig;
    }
}

OpcuaGenericClientDeviceImpl::OpcuaGenericClientDeviceImpl(const ContextPtr& ctx,
                                                           const ComponentPtr& parent,
                                                           const PropertyObjectPtr& config,
                                                           std::shared_ptr<OpcUaClient> client,
                                                           const std::string& localId,
                                                           const std::string& name)
    : Device(ctx, parent, localId.empty() ? generateLocalId() : localId)
    , connectionStatus(Enumeration("ConnectionStatusType", "Connected", this->context.getTypeManager()))
    , client(client)
{
    if (this->client == nullptr)
        DAQ_THROW_EXCEPTION(UninitializedException, "OpcUaClient is not initialized");

    this->name = name.empty() ? GENERIC_OPCUA_CLIENT_DEVICE_NAME : name;

    if (config.assigned())
        initProperties(populateDefaultConfig(createDefaultConfig(), config));
    else
        initProperties(createDefaultConfig());

    try
    {
        this->client->connect();
        this->client->runIterate();
    }
    catch (const OpcUaException& e)
    {
        switch (e.getStatusCode())
        {
            case UA_STATUSCODE_BADUSERACCESSDENIED:
            case UA_STATUSCODE_BADIDENTITYTOKENINVALID:
                DAQ_THROW_EXCEPTION(AuthenticationFailedException, e.what());
            default:
                DAQ_THROW_EXCEPTION(GeneralErrorException, e.what());
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
    defaultConfig.addProperty(StringProperty(PROPERTY_NAME_OPCUA_MI_LOCAL_ID, ""));


    return defaultConfig;
}

void OpcuaGenericClientDeviceImpl::initProperties(const PropertyObjectPtr& config)
{
    for (const auto& prop : config.getAllProperties())
    {
        const auto propName = prop.getName();
        if (propName == PROPERTY_NAME_OPCUA_MI_LOCAL_ID)
            continue;
        if (!objPtr.hasProperty(propName))
        {
            auto propClone = PropertyBuilder(prop.getName())
            .setValueType(prop.getValueType())
                .setDescription(prop.getDescription())
                .setDefaultValue(prop.getValue())
                .setVisible(prop.getVisible())
                .setReadOnly(true)
                .build();
            objPtr.addProperty(propClone);
        }
    }
}

std::string OpcuaGenericClientDeviceImpl::getConnectionString() const
{
    return client->getEndpoint().getUrl();
}

void OpcuaGenericClientDeviceImpl::removed()
{
    Device::removed();
}

DeviceInfoPtr OpcuaGenericClientDeviceImpl::onGetInfo()
{
    return DeviceInfo(getConnectionString(), GENERIC_OPCUA_CLIENT_DEVICE_NAME);
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
