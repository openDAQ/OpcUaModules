#include <coretypes/validation.h>
#include <opendaq/custom_log.h>
#include <opcuatms_client/objects/tms_client_daqserver_component_impl.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS
using namespace opcua;

TmsClientDaqServerComponentImpl::TmsClientDaqServerComponentImpl(const ContextPtr& ctx,
                                                                 const ComponentPtr& parent,
                                                                 const StringPtr& localId,
                                                                 const TmsClientContextPtr& clientContext,
                                                                 const opcua::OpcUaNodeId& nodeId,
                                                                 const DevicePtr& parentDevice)
    : Super(ctx, parent, localId, clientContext, nodeId, parentDevice)
{
}

ErrCode TmsClientDaqServerComponentImpl::enableDiscovery()
{
    return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_NOTIMPLEMENTED);
}

ErrCode TmsClientDaqServerComponentImpl::stop()
{
    return OPENDAQ_IGNORED;
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
