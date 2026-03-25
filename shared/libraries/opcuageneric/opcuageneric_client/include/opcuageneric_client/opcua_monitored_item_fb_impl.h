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
#include <opendaq/function_block_impl.h>
#include <opendaq/data_packet_ptr.h>
#include "opcuaclient/opcuaclient.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

class OpcUaMonitoredItemFbImpl final : public FunctionBlock
{
    friend class GenericOpcuaMonitoredItemTest;
public:

    // only string NodeIDs are supported at the moment
    enum class NodeIDType : int
    {
        //Numeric = 0,
        String = 0,
        //Guid,
        //Opaque,
        _count
    };

    explicit OpcUaMonitoredItemFbImpl(const ContextPtr& ctx,
                                      const ComponentPtr& parent,
                                      const FunctionBlockTypePtr& type,
                                      daq::opcua::OpcUaClientPtr client,
                                      const PropertyObjectPtr& config = nullptr);
    ~OpcUaMonitoredItemFbImpl();
    /*DAQ_OPCUA_MODULE_API*/ static FunctionBlockTypePtr CreateType();
protected:

    struct FbConfig {
        NodeIDType nodeIdType;
        std::string nodeId;
        uint32_t namespaceIndex;
        uint32_t samplingInterval;
    };

    static std::atomic<int> localIndex;
    static std::unordered_map<OpcUaNodeId, daq::SampleType> supportedDataTypes;

    DataDescriptorPtr outputSignalDescriptor;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;
    std::atomic<bool> configValid;
    std::string configMsg;
    std::atomic<bool> nodeValidationError;
    std::string nodeValidationErrorMsg;
    std::atomic<bool> valueValidationError;

    FbConfig config;
    daq::opcua::OpcUaClientPtr client;
    OpcUaNodeId nodeId;
    OpcUaNodeId nodeDataType;

    std::thread readerThread;
    std::atomic<bool> running;

    void removed() override;
    static std::string generateLocalId();

    void adjustSignalDescriptor();
    void createSignal();
    void reconfigureSignal(const FbConfig& prevConfig);
    SignalConfigPtr createDomainSignal();

    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void propertyChanged();

    void updateStatuses();

    void validateNode();
    bool validateValueDataType(const OpcUaVariant& value);

    void runReaderThread();
    void readerLoop();

    daq::DataPacketPtr buildDataPacket(const OpcUaVariant& value);
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
