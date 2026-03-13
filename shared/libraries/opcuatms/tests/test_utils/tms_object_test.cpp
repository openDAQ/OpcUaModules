#include <opcuaclient/opcuaclient.h>
#include <opcuatms_server/objects/tms_server_object.h>
#include <open62541/types_daqbsp_generated.h>
#include <open62541/types_daqdevice_generated.h>
#include <open62541/types_daqesp_generated.h>
#include <open62541/types_daqhbk_generated.h>
#include <open62541/types_di_generated.h>
#include <tms_object_test.h>

using namespace daq::opcua;

TmsObjectTest::TmsObjectTest()
{
}

void TmsObjectTest::Init()
{
    server = CreateAndStartTestServer();
    client = CreateAndConnectTestClient();
}

void TmsObjectTest::Clear()
{
    client.reset();
    server.reset();
}

OpcUaServerPtr TmsObjectTest::getServer()
{
    return server;
}

OpcUaClientPtr TmsObjectTest::getClient()
{
    return client;
}

void TmsObjectTest::writeChildNode(const OpcUaNodeId& parent, const std::string& browseName, const OpcUaVariant& variant)
{
    getClient()->writeValue(getChildNodeId(parent, browseName), variant);
}

OpcUaVariant TmsObjectTest::readChildNode(const OpcUaNodeId& parent, const std::string& browseName)
{
    return getClient()->readValue(getChildNodeId(parent, browseName));
}

OpcUaNodeId TmsObjectTest::getChildNodeId(const OpcUaNodeId& parent, const std::string& browseName)
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

OpcUaObject<UA_BrowseResponse> TmsObjectTest::browseNode(const daq::opcua::OpcUaNodeId& nodeId)
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

void TmsObjectTest::waitForInput()
{
    std::cout << "Type q to quit..." << std::endl;
    std::string input;
    std::cin >> input;
}

daq::opcua::OpcUaServerPtr TmsObjectTest::CreateAndStartTestServer()
{
    auto server = std::make_shared<daq::opcua::OpcUaServer>();
    server->setPort(4840);
    server->start();
    return server;
}

daq::opcua::OpcUaClientPtr TmsObjectTest::CreateAndConnectTestClient(const std::string& username, const std::string& password)
{
    OpcUaEndpoint endpoint("opc.tcp://127.0.0.1:4840", username, password);
    endpoint.registerCustomTypes(UA_TYPES_DI_COUNT, UA_TYPES_DI);
    endpoint.registerCustomTypes(UA_TYPES_DAQBT_COUNT, UA_TYPES_DAQBT);
    endpoint.registerCustomTypes(UA_TYPES_DAQBSP_COUNT, UA_TYPES_DAQBSP);
    endpoint.registerCustomTypes(UA_TYPES_DAQDEVICE_COUNT, UA_TYPES_DAQDEVICE);
    endpoint.registerCustomTypes(UA_TYPES_DAQESP_COUNT, UA_TYPES_DAQESP);
    endpoint.registerCustomTypes(UA_TYPES_DAQHBK_COUNT, UA_TYPES_DAQHBK);

    auto client = std::make_shared<daq::opcua::OpcUaClient>(endpoint);
    client->connect();
    client->runIterate();
    return client;
}
