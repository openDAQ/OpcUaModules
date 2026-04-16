#include <coretypes/validation.h>
#include <opendaq/custom_log.h>
#include <opcuatms_client/objects/tms_client_daqserver_component_impl.h>
#include <opcuatms/exceptions.h>

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
    const ErrCode errCode = daqTry([&]()
    {
        const auto methodNodeId = getNodeId("EnableDiscovery");

        auto request = OpcUaCallMethodRequest();
        request->objectId = nodeId.copyAndGetDetachedValue();
        request->methodId = methodNodeId.copyAndGetDetachedValue();
        request->inputArgumentsSize = 0;

        auto response = client->callMethod(request);

        if (response->statusCode != UA_STATUSCODE_GOOD)
            throw OpcUaGeneralException();
    });
    OPENDAQ_RETURN_IF_FAILED(errCode);
    return errCode;
}

ErrCode TmsClientDaqServerComponentImpl::disableDiscovery()
{
    const ErrCode errCode = daqTry([&]()
    {
        const auto methodNodeId = getNodeId("DisableDiscovery");

        auto request = OpcUaCallMethodRequest();
        request->objectId = nodeId.copyAndGetDetachedValue();
        request->methodId = methodNodeId.copyAndGetDetachedValue();
        request->inputArgumentsSize = 0;

        auto response = client->callMethod(request);

        if (response->statusCode != UA_STATUSCODE_GOOD)
            throw OpcUaGeneralException();
    });
    OPENDAQ_RETURN_IF_FAILED(errCode);
    return errCode;
}

ErrCode TmsClientDaqServerComponentImpl::stop()
{
    return OPENDAQ_IGNORED;
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
