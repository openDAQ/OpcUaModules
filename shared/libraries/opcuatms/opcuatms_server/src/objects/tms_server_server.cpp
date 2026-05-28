#include <opcuatms_server/objects/tms_server_server.h>
#include <open62541/daqdevice_nodeids.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

using namespace opcua;

TmsServerServer::TmsServerServer(const ServerPtr& object, const OpcUaServerPtr& server, const ContextPtr& context, const TmsServerContextPtr& tmsContext)
    : Super(object, server, context, tmsContext)
{
}

void TmsServerServer::addChildNodes()
{
    Super::addChildNodes();

    createEnableDiscoveryNode();
    createDisableDiscoveryNode();
}

OpcUaNodeId TmsServerServer::getTmsTypeId()
{
    return OpcUaNodeId(NAMESPACE_DAQDEVICE, UA_DAQDEVICEID_DAQCOMPONENTTYPE);
}

void TmsServerServer::createEnableDiscoveryNode()
{
    OpcUaNodeId nodeIdOut;
    AddMethodNodeParams params(nodeIdOut, nodeId);
    params.referenceTypeId = OpcUaNodeId(UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT));
    params.setBrowseName("EnableDiscovery");
    params.inputArgumentsSize = 0;

    enableDiscoveryNodeId = server->addMethodNode(params);

    auto callback = [this](NodeEventManager::MethodArgs args) -> UA_StatusCode
    {
        try
        {
            onEnableDiscovery(args);
        }
        catch (const OpcUaException& e)
        {
            return e.getStatusCode();
        }
        catch (...)
        {
            return UA_STATUSCODE_BADINTERNALERROR;
        }

        return UA_STATUSCODE_GOOD;
    };

    addEvent(enableDiscoveryNodeId)->onMethodCall(callback);
}

void TmsServerServer::createDisableDiscoveryNode()
{
    OpcUaNodeId nodeIdOut;
    AddMethodNodeParams params(nodeIdOut, nodeId);
    params.referenceTypeId = OpcUaNodeId(UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT));
    params.setBrowseName("DisableDiscovery");
    params.inputArgumentsSize = 0;

    disableDiscoveryNodeId = server->addMethodNode(params);

    auto callback = [this](NodeEventManager::MethodArgs args) -> UA_StatusCode
    {
        try
        {
            onDisableDiscovery(args);
        }
        catch (const OpcUaException& e)
        {
            return e.getStatusCode();
        }
        catch (...)
        {
            return UA_STATUSCODE_BADINTERNALERROR;
        }

        return UA_STATUSCODE_GOOD;
    };

    addEvent(disableDiscoveryNodeId)->onMethodCall(callback);
}

void TmsServerServer::onEnableDiscovery(const NodeEventManager::MethodArgs& args)
{
    assert(args.inputSize == 0);

    object.enableDiscovery();
}

void TmsServerServer::onDisableDiscovery(const NodeEventManager::MethodArgs& args)
{
    assert(args.inputSize == 0);

    object.disableDiscovery();
}

bool TmsServerServer::checkPermission(const Permission permission,
                                                  const UA_NodeId* const nodeId,
                                                  const OpcUaSession* const sessionContext)
{
    bool allow = true;
    if (permission == Permission::Execute)
    {
        if (const auto browseName = TmsServerObject::readBrowseName(*nodeId); browseName == "EnableDiscovery" || browseName == "DisableDiscovery")
            allow = TmsServerComponent<ServerPtr>::checkPermission(Permission::Write, nodeId, sessionContext);
    }
    return (allow && TmsServerComponent<ServerPtr>::checkPermission(permission, nodeId, sessionContext));
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
