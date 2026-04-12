#include <opcuatms_server/objects/tms_server_channel.h>
#include <opcuatms_server/objects/tms_server_folder.h>
#include <opcuatms/converters/variant_converter.h>
#include <open62541/daqdevice_nodeids.h>
#include <opendaq/io_folder_config.h>
#include <opendaq/search_filter_factory.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

using namespace opcua;

TmsServerFolder::TmsServerFolder(const FolderPtr& object, const OpcUaServerPtr& server, const ContextPtr& context, const TmsServerContextPtr& tmsContext)
    : Super(object, server, context, tmsContext)
{
}

void TmsServerFolder::addChildNodes()
{
    uint32_t numberInList = 0;
    for (auto item : object.getItems(search::Any()))
    {
        auto folder = item.asPtrOrNull<IFolder>();
        auto channel = item.asPtrOrNull<IChannel>();
        auto server = item.asPtrOrNull<IServer>();
        auto component = item.asPtrOrNull<IComponent>();

        if (channel.assigned())
        {
            auto tmsChannel = registerTmsObjectOrAddReference<TmsServerChannel>(this->nodeId, channel, numberInList++);
            channels.push_back(std::move(tmsChannel));
        }
        else if (server.assigned())
        {
            auto tmsServerComponent = registerTmsObjectOrAddReference<TmsServerDaqServerComponent>(this->nodeId, server, numberInList++);
            daqServerComponents.push_back(std::move(tmsServerComponent));
        }
        else if (folder.assigned()) // It is important to test for folder last as a channel and server also are folders!
        {
            auto tmsFolder = registerTmsObjectOrAddReference<TmsServerFolder>(this->nodeId, folder, numberInList++);
            folders.push_back(std::move(tmsFolder));
        }
        else if (component.assigned())  // It is important to test for component after folder!
        {
            auto tmsComponent = registerTmsObjectOrAddReference<TmsServerComponent<>>(this->nodeId, component, numberInList++);
            components.push_back(std::move(tmsComponent));
        }
        else
        {
            DAQ_THROW_EXCEPTION(daq::NotImplementedException, "Unhandled item: " + item.getGlobalId());
        }
    }

    Super::addChildNodes();
}

void TmsServerFolder::onCoreEvent(const CoreEventArgsPtr& args)
{
    Super::onCoreEvent(args);

    if (this->object.getLocalId() == "Srv" && args.getEventId() == static_cast<int>(CoreEventId::ComponentAdded))
    {
        auto server = args.getParameters().get("Component").asPtrOrNull<IServer>();
        if (server.assigned())
        {
            auto tmsServerComponent = registerTmsObjectOrAddReference<TmsServerDaqServerComponent>(this->nodeId, server, std::numeric_limits<uint32_t>::max());
            daqServerComponents.push_back(std::move(tmsServerComponent));
        }

    }
}

OpcUaNodeId TmsServerFolder::getTmsTypeId()
{
    if (object.supportsInterface<IIoFolderConfig>())
        return OpcUaNodeId(NAMESPACE_DAQDEVICE, UA_DAQDEVICEID_IOCOMPONENTTYPE);
    return OpcUaNodeId(NAMESPACE_DAQDEVICE, UA_DAQDEVICEID_DAQCOMPONENTTYPE);
}

void TmsServerFolder::createNonhierarchicalReferences()
{
    createChildNonhierarchicalReferences(channels);
    createChildNonhierarchicalReferences(daqServerComponents);
    createChildNonhierarchicalReferences(folders);
    // createChildNonhierarchicalReferences(components); // ?
}

END_NAMESPACE_OPENDAQ_OPCUA_TMS
