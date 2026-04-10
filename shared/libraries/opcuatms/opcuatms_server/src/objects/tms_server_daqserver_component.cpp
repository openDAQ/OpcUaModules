#include <opcuatms_server/objects/tms_server_daqserver_component.h>
#include <open62541/daqdevice_nodeids.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

using namespace opcua;

TmsServerDaqServerComponent::TmsServerDaqServerComponent(const ServerPtr& object, const OpcUaServerPtr& server, const ContextPtr& context, const TmsServerContextPtr& tmsContext)
    : Super(object, server, context, tmsContext)
{
}

OpcUaNodeId TmsServerDaqServerComponent::getTmsTypeId()
{
    return OpcUaNodeId(NAMESPACE_DAQDEVICE, UA_DAQDEVICEID_DAQCOMPONENTTYPE);
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
