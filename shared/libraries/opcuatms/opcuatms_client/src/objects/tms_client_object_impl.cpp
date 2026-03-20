#include <opcuatms_client/objects/tms_client_object_impl.h>
#include <opcuaclient/browse_request.h>
#include <opcuaclient/browser/opcuabrowser.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

using namespace opcua;
using namespace opcua::utils;

TmsClientObjectImpl::TmsClientObjectImpl(const ContextPtr& daqContext, const TmsClientContextPtr& clientContext, const OpcUaNodeId& nodeId)
    : clientContext(clientContext)
    , client(clientContext->getClient())
    , nodeId(nodeId)
    , daqContext(daqContext)
{
}

TmsClientObjectImpl::~TmsClientObjectImpl()
{
    clientContext->unregisterObject(nodeId);
}

void TmsClientObjectImpl::registerObject(const BaseObjectPtr& obj)
{
    clientContext->registerObject(nodeId, obj);
}

SignalPtr TmsClientObjectImpl::findSignal(const opcua::OpcUaNodeId& nodeId) const
{
    return clientContext->getObject<ISignal>(nodeId);
}

bool TmsClientObjectImpl::hasReference(const std::string& name)
{
    return clientContext->getReferenceBrowser()->hasReference(nodeId, name);
}

OpcUaNodeId TmsClientObjectImpl::getNodeId(const std::string& nodeName)
{
    return clientContext->getReferenceBrowser()->getChildNodeId(nodeId, nodeName);
}

void TmsClientObjectImpl::writeValue(const std::string& nodeName, const OpcUaVariant& value)
{
    const auto nodeId = getNodeId(nodeName);
    client->writeValue(nodeId, value);
}

OpcUaVariant TmsClientObjectImpl::readValue(const std::string& nodeName)
{
    const auto nodeId = getNodeId(nodeName);
    return client->readValue(nodeId);
}

MonitoredItem* TmsClientObjectImpl::monitoredItemsCreateEvent(const EventMonitoredItemCreateRequest& item,
                                                              const EventNotificationCallbackType& eventNotificationCallback)
{
    return getSubscription()->monitoredItemsCreateEvent(UA_TIMESTAMPSTORETURN_BOTH, *item, eventNotificationCallback);
}

MonitoredItem* TmsClientObjectImpl::monitoredItemsCreateDataChange(const UA_MonitoredItemCreateRequest& item,
                                                                   const DataChangeNotificationCallbackType& dataChangeNotificationCallback)
{
    return getSubscription()->monitoredItemsCreateDataChange(UA_TIMESTAMPSTORETURN_BOTH, item, dataChangeNotificationCallback);
}

Subscription* TmsClientObjectImpl::getSubscription()
{
    if (!subscription)
        subscription = client->createSubscription(UA_CreateSubscriptionRequest_default(), std::bind(&TmsClientObjectImpl::subscriptionStatusChangeCallback, this, std::placeholders::_3));

    return subscription;
}

void TmsClientObjectImpl::subscriptionStatusChangeCallback(UA_StatusChangeNotification* notification)
{
    //TODO report on disconnect
}

uint32_t TmsClientObjectImpl::tryReadChildNumberInList(const std::string& nodeName)
{
    const auto childId = this->getNodeId(nodeName);
    return tryReadChildNumberInList(childId);
}

uint32_t TmsClientObjectImpl::tryReadChildNumberInList(const opcua::OpcUaNodeId& nodeId)
{
    try
    {
        const auto numberInListId = clientContext->getReferenceBrowser()->getChildNodeId(nodeId, "NumberInList");
        const auto variant = clientContext->getAttributeReader()->getValue(numberInListId, UA_ATTRIBUTEID_VALUE);
        return VariantConverter<IInteger>::ToDaqObject(variant);
    }
    catch (...)
    {
    }

    return std::numeric_limits<uint32_t>::max();
}

CachedReferences TmsClientObjectImpl::getChildReferencesOfType(const opcua::OpcUaNodeId& nodeId, const opcua::OpcUaNodeId& typeId)
{
    BrowseFilter filter;
    filter.typeDefinition = typeId;
    return clientContext->getReferenceBrowser()->browseFiltered(nodeId, filter);
}

bool TmsClientObjectImpl::getAttributeWritePermission(const opcua::OpcUaNodeId& nodeId)
{
    // Note: This method is used to determine if a node attributes is changeable.
    // For variable nodes you should check UA_ATTRIBUTEID_ACCESSLEVEL and UA_ATTRIBUTEID_USERACCESSLEVEL!
    // In common case a user can have write permission to a node but not to its attributes.
    bool commonWritePerm = true;
    try
    {
        const auto reader = clientContext->getAttributeReader();
        const int64_t userWriteMask = reader->getValue(nodeId, UA_ATTRIBUTEID_USERWRITEMASK).toInteger();
        const int64_t writeMask = reader->getValue(nodeId, UA_ATTRIBUTEID_WRITEMASK).toInteger();
        commonWritePerm = ((userWriteMask & writeMask) != 0);
    }
    catch (...)
    {
        DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_NOTIMPLEMENTED, "Cannot read write mask attributes for OpcUA node");
    }
    return commonWritePerm;
}

bool TmsClientObjectImpl::getExecutePermission(const opcua::OpcUaNodeId& nodeId)
{
    // Note: This method is used to determine if a method node is executable.
    // Other nodes don't have executable attributes and this method will return true for them.
    bool commonExecutable = true;
    try
    {
        const auto reader = clientContext->getAttributeReader();
        const bool userExecutable = reader->getValue(nodeId, UA_ATTRIBUTEID_USEREXECUTABLE).toBool();
        const bool executable = reader->getValue(nodeId, UA_ATTRIBUTEID_EXECUTABLE).toBool();
        commonExecutable = userExecutable & executable;
    }
    catch (...)
    {
        DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_NOTIMPLEMENTED, "Cannot read executable mask attributes for OpcUA node");
    }
    return commonExecutable;
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
