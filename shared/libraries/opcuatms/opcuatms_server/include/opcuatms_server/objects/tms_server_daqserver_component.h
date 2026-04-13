/*
 * Copyright 2022-2025 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <opendaq/server_ptr.h>
#include <opcuatms_server/objects/tms_server_component.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

class TmsServerDaqServerComponent;
using TmsServerDaqServerComponentPtr = std::shared_ptr<TmsServerDaqServerComponent>;

class TmsServerDaqServerComponent : public TmsServerComponent<ServerPtr>
{
public:
    using Super = TmsServerComponent<ServerPtr>;

    TmsServerDaqServerComponent(const ServerPtr& object, const opcua::OpcUaServerPtr& server, const ContextPtr& context, const TmsServerContextPtr& tmsContext);
    void addChildNodes() override;
    bool checkPermission(const Permission permission,
                         const UA_NodeId* const nodeId,
                         const OpcUaSession* const sessionContext) override;
    OpcUaNodeId getEnableDiscoveryNodeId() const { return enableDiscoveryNodeId; }
    OpcUaNodeId getDisableDiscoveryNodeId() const { return disableDiscoveryNodeId; }

protected:
    opcua::OpcUaNodeId getTmsTypeId() override;
    void createEnableDiscoveryNode();
    void createDisableDiscoveryNode();
    void onEnableDiscovery(const NodeEventManager::MethodArgs& args);
    void onDisableDiscovery(const NodeEventManager::MethodArgs& args);

    OpcUaNodeId enableDiscoveryNodeId;
    OpcUaNodeId disableDiscoveryNodeId;
};

END_NAMESPACE_OPENDAQ_OPCUA_TMS
