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
#include <opcuaclient/opcuaclient.h>

namespace test_helpers
{

class OpcuaServerHelper
{
public:
    OpcuaServerHelper() = default;
    virtual void Init();
    virtual void Clear();

    daq::opcua::OpcUaClientPtr getClient();
    void writeChildNode(const daq::opcua::OpcUaNodeId& parent,
                        const std::string& browseName,
                        const daq::opcua::OpcUaVariant& variant);
    daq::opcua::OpcUaVariant readChildNode(const daq::opcua::OpcUaNodeId& parent, const std::string& browseName);
    daq::opcua::OpcUaNodeId getChildNodeId(const daq::opcua::OpcUaNodeId& parent, const std::string& browseName);
    daq::opcua::OpcUaObject<UA_BrowseResponse> browseNode(const daq::opcua::OpcUaNodeId& nodeId);

    static daq::opcua::OpcUaClientPtr CreateAndConnectTestClient(const std::string& username = "", const std::string& password = "");

protected:
    daq::opcua::OpcUaClientPtr client;
};
}