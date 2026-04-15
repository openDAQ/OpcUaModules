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
#include <opcua_generic_client_module/common.h>
#include <opendaq/module_impl.h>
#include <daq_discovery/daq_discovery_client.h>
#include <opendaq/device_ptr.h>
#include "opcuaclient/opcuaclient.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC_CLIENT_MODULE

class OpcUaGenericClientModule final : public Module
{
public:
    OpcUaGenericClientModule(ContextPtr context);

    ListPtr<IDeviceInfo> onGetAvailableDevices() override;
    DictPtr<IString, IDeviceType> onGetAvailableDeviceTypes() override;
    DevicePtr onCreateDevice(const StringPtr& connectionString,
                             const ComponentPtr& parent,
                             const PropertyObjectPtr& config) override;
    bool acceptsConnectionParameters(const StringPtr& connectionString);
    Bool onCompleteServerCapability(const ServerCapabilityPtr& source, const ServerCapabilityConfigPtr& target) override;

private:
    struct ParsedConnectionInfo
    {
        std::string host;
        int port;
        std::string hostType;
        std::string path;
    };

    static ParsedConnectionInfo formConnectionString(const StringPtr& connectionString);
    static std::shared_ptr<opcua::OpcUaClient> initOpcuaClient(const std::string& url,
                                                               const std::string& username,
                                                               const std::string& password);
    static opcua::OpcUaClient::ApplicationDescription readApplicationDescription(std::shared_ptr<opcua::OpcUaClient> client);
    static std::string buildDeviceName(const opcua::OpcUaClient::ApplicationDescription& appDesc);
    static std::string buildDeviceLocalId(const opcua::OpcUaClient::ApplicationDescription& appDesc);
    static std::string buildOpcuaUrl(const ParsedConnectionInfo& connectionInfo);
    static void populateDeviceInfo(DeviceInfoPtr deviceInfo,
                                   const StringPtr& connectionString,
                                   const ParsedConnectionInfo& connectionInfo,
                                   const std::string& username,
                                   const std::string& productUri);

    static DeviceTypePtr createDeviceType();
    static PropertyObjectPtr createDefaultConfig();
    static PropertyObjectPtr populateDefaultConfig(const PropertyObjectPtr& config);
    static DeviceInfoPtr populateDiscoveredDevice(const discovery::MdnsDiscoveredDevice& discoveredDevice);
    discovery::DiscoveryClient discoveryClient;

    std::mutex sync;
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC_CLIENT_MODULE
