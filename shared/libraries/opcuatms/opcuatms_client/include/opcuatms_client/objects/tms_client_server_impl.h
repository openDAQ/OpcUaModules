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
#include <opcuatms_client/objects/tms_client_component_impl.h>
#include <opendaq/server_impl.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_TMS

class TmsClientServerImpl : public TmsClientComponentBaseImpl<ServerImpl<IServer, ITmsClientComponent>>
{
public:
    using Impl = ServerImpl<IServer, ITmsClientComponent>;
    using Super = TmsClientComponentBaseImpl<Impl>;

    TmsClientServerImpl(const ContextPtr& ctx,
                                    const ComponentPtr& parent,
                                    const StringPtr& localId,
                                    const TmsClientContextPtr& clientContext,
                                    const opcua::OpcUaNodeId& nodeId,
                                    const DevicePtr& parentDevice);

    // IServer
    ErrCode INTERFACE_FUNC enableDiscovery() override;
    ErrCode INTERFACE_FUNC disableDiscovery() override;
    ErrCode INTERFACE_FUNC stop() override;
};

END_NAMESPACE_OPENDAQ_OPCUA_TMS
