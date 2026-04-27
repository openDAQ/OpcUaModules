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
#include <opcuageneric_client/opcuageneric.h>
#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/status_adaptor.h>
#include <opendaq/device_impl.h>
#include <opendaq/streaming_ptr.h>
#include "opcuaclient/opcuaclient.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <opcuageneric_client/opcua_monitored_item_fb_impl.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

class OpcuaGenericClientDeviceImpl : public Device
{
public:
    explicit OpcuaGenericClientDeviceImpl(const ContextPtr& ctx,
                                          const ComponentPtr& parent,
                                          const PropertyObjectPtr& config,
                                          std::shared_ptr<OpcUaClient> client,
                                          const std::string& localId,
                                          const std::string& name,
                                          uint32_t reconnectIntervalMs = DEFAULT_RECONNECT_INTERVAL);
    ~OpcuaGenericClientDeviceImpl();
    DAQ_OPCUA_GENERIC_MODULE_API static PropertyObjectPtr createDefaultConfig();

protected:
    static std::atomic<int> localIndex;
    static std::string generateLocalId();

    void removed() override;
    DeviceInfoPtr onGetInfo() override;

    bool allowAddFunctionBlocksFromModules() override
    {
        return true;
    };
    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onAddFunctionBlock(const StringPtr& typeId, const PropertyObjectPtr& config) override;
    void onRemoveFunctionBlock(const FunctionBlockPtr& functionBlock) override;

    void initNestedFbTypes();
    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void propertyChanged();
    std::string getConnectionString() const;

    DictObjectPtr<IDict, IString, IFunctionBlockType> nestedFbTypes;

    StatusAdaptor connectionStatus;

    std::atomic<bool> connectedDone{false};
    std::unordered_map<std::string, std::string> deviceMap;  // device name -> signal list JSON

    daq::opcua::OpcUaClientPtr client;
    DomainSource domainSource;

    mutable std::recursive_mutex processingMutex;
    std::vector<FunctionBlockPtr> nestedFunctionBlocks;


    // Reconnect monitor
    const uint32_t reconnectIntervalMs;
    std::thread reconnectThread;
    std::atomic<bool> reconnectRunning{false};
    std::condition_variable reconnectCv;
    std::mutex reconnectMutex;

    void startReconnectMonitor();
    void stopReconnectMonitor();
    void reconnectMonitorLoop();
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
