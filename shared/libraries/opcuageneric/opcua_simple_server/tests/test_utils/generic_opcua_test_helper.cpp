#include <opcuaclient/opcuaclient.h>
#include <generic_opcua_test_helper.h>


using namespace daq::opcua;

namespace test_helpers
{
void OpcuaServerHelper::Init()
{
    client = CreateAndConnectTestClient();
}

void OpcuaServerHelper::Clear()
{
    client.reset();
}

OpcUaClientPtr OpcuaServerHelper::getClient()
{
    return client;
}

void OpcuaServerHelper::writeChildNode(const OpcUaNodeId& parent, const std::string& browseName, const OpcUaVariant& variant)
{
    getClient()->writeValue(getChildNodeId(parent, browseName), variant);
}

OpcUaVariant OpcuaServerHelper::readChildNode(const OpcUaNodeId& parent, const std::string& browseName)
{
    return getClient()->readValue(getChildNodeId(parent, browseName));
}

OpcUaNodeId OpcuaServerHelper::getChildNodeId(const OpcUaNodeId& parent, const std::string& browseName)
{
    OpcUaObject<UA_BrowseRequest> br;
    br->requestedMaxReferencesPerNode = 0;
    br->nodesToBrowse = UA_BrowseDescription_new();
    br->nodesToBrowseSize = 1;
    br->nodesToBrowse[0].nodeId = parent.copyAndGetDetachedValue();
    br->nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

    OpcUaObject<UA_BrowseResponse> result = UA_Client_Service_browse(client->getUaClient(), *br);

    if (result->resultsSize == 0)
        return OpcUaNodeId(UA_NODEID_NULL);

    auto references = result->results[0].references;
    auto referenceCount = result->results[0].referencesSize;

    for (size_t i = 0; i < referenceCount; i++)
    {
        auto reference = references[i];
        std::string refBrowseName = utils::ToStdString(reference.browseName.name);
        if (refBrowseName == browseName)
            return OpcUaNodeId(reference.nodeId.nodeId);
    }

    return OpcUaNodeId(UA_NODEID_NULL);
}

OpcUaObject<UA_BrowseResponse> OpcuaServerHelper::browseNode(const daq::opcua::OpcUaNodeId& nodeId)
{
    OpcUaObject<UA_BrowseRequest> br;
    br->requestedMaxReferencesPerNode = 0;
    br->nodesToBrowse = UA_BrowseDescription_new();
    br->nodesToBrowseSize = 1;
    br->nodesToBrowse[0].nodeId = nodeId.copyAndGetDetachedValue();
    br->nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

    OpcUaObject<UA_BrowseResponse> result = UA_Client_Service_browse(client->getUaClient(), *br);
    if (result->resultsSize == 0)
        throw OpcUaException(UA_STATUSCODE_BADUNEXPECTEDERROR, "");
    CheckStatusCodeException(result->results[0].statusCode);
    return result;
}

daq::opcua::OpcUaClientPtr OpcuaServerHelper::CreateAndConnectTestClient(const std::string& username, const std::string& password)
{
    OpcUaEndpoint endpoint("opc.tcp://127.0.0.1:4840", username, password);

    auto client = std::make_shared<daq::opcua::OpcUaClient>(endpoint);
    client->connect();
    client->runIterate();
    return client;
}
}