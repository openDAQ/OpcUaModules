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
#include <opcuageneric_client/status_container.h>
#include <opcuageneric_client/common.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/function_block_impl.h>
#include "opcuaclient/opcuaclient.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

class OpcUaMonitoredItemFbImpl final : public FunctionBlock
{
    friend class GenericOpcuaMonitoredItemTest;

public:
    explicit OpcUaMonitoredItemFbImpl(const ContextPtr& ctx,
                                      const ComponentPtr& parent,
                                      const FunctionBlockTypePtr& type,
                                      daq::opcua::OpcUaClientPtr client,
                                      DomainSource defaultDomainSource,
                                      const PropertyObjectPtr& config = nullptr);
    ~OpcUaMonitoredItemFbImpl();
    DAQ_OPCUA_GENERIC_MODULE_API static FunctionBlockTypePtr CreateType();

    void setDomainSource(DomainSource domainSource);

protected:
    struct DataPackets
    {
        daq::DataPacketPtr dataPacket;
        daq::DataPacketPtr domainDataPacket;
    };

    struct FbConfig
    {
        OpcUaNodeId nodeId;
        uint32_t samplingInterval;
        DomainSource domainSource;
    };

    static std::atomic<int> localIndex;
    static std::unordered_map<OpcUaNodeId, daq::SampleType> supportedDataTypeNodeIds;
    static std::unordered_map<UA_DataTypeKind, daq::SampleType> supportedDataTypeKinds;
    static std::unordered_map<UA_DataTypeKind, OpcUaNodeId> dataTypeKindToDataTypeNodeId;

    DataDescriptorPtr outputSignalDescriptor;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;

    FbConfig config;
    daq::opcua::OpcUaClientPtr client;
    OpcUaNodeId nodeDataType;

    std::thread readerThread;
    std::atomic<bool> running;
    std::recursive_mutex processingMutex;

    std::shared_ptr<utils::StatusContainer> statuses;
    utils::Error configErr;
    utils::Error nodeValidationErr;
    utils::Error responseValidationErr;
    utils::Error valueValidationErr;
    utils::Error exceptionErr;

    void removed() override;
    static std::string generateLocalId();

    void initStatusContainer();
    void adjustSignalDescriptor();
    void createSignal();
    void reconfigureSignal(const FbConfig& prevConfig);
    SignalConfigPtr createDomainSignal();

    void initProperties(const PropertyObjectPtr& config);
    void readProperties();
    void propertyChanged();

    void updateStatuses();

    void validateNode();
    bool validateResponse(const OpcUaDataValue& value);
    bool validateValueDataType(const OpcUaDataValue& value);

    void runReaderThread();
    void readerLoop();

    DataPackets buildDataPacket(const OpcUaDataValue& value);
    daq::DataPacketPtr buildDomainDataPacket(const OpcUaDataValue& value);
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
