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
#include <opcuageneric_client/constants.h>
#include <opcuageneric_client/sampling_scheduler.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/function_block_impl.h>
#include <optional>
#include "opcuaclient/opcuaclient.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

class OpcUaMonitoredItemFbImpl final : public FunctionBlock, public ISampledItem
{
    friend class GenericOpcuaMonitoredItemTest;

public:
    explicit OpcUaMonitoredItemFbImpl(const ContextPtr& ctx,
                                      const ComponentPtr& parent,
                                      const FunctionBlockTypePtr& type,
                                      daq::opcua::OpcUaClientPtr client,
                                      const std::string& localId,
                                      DomainSource initialDomainSource = DomainSource::SourceTimestamp,
                                      uint32_t initialSamplingIntervalMs = DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL,
                                      SamplingScheduler* scheduler = nullptr,
                                      const PropertyObjectPtr& config = nullptr);
    ~OpcUaMonitoredItemFbImpl();

    DAQ_OPCUA_GENERIC_MODULE_API static FunctionBlockTypePtr CreateType();

    uint32_t getSamplingInterval() const override;
    void processSample() override;
    void onConnectionRestored() override;
    void onSchedulerDestroyed() override;

protected:
    struct DataPackets
    {
        daq::DataPacketPtr dataPacket;
        daq::DataPacketPtr domainDataPacket;
    };

    struct FbConfig
    {
        OpcUaNodeId nodeId;
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

    std::atomic<uint32_t> samplingIntervalMs{DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL};

    // Domain value (us since the Unix epoch) of the last published sample
    // A sample resolving to the same value is not published again
    std::optional<uint64_t> lastPublishedDomainTs;

    // Not owned. The device owns the scheduler and destroys it before the component tree releases this
    // block, so the scheduler clears this pointer from its destructor. Atomic because removed() and that
    // teardown can reach it from different threads.
    std::atomic<SamplingScheduler*> scheduler;
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
    static DataDescriptorPtr buildTimeDescriptor(daq::SampleType sampleType);
    void adjustSignalDescriptor();
    void createSignal();
    void reconfigureSignal(const FbConfig& prevConfig);
    SignalConfigPtr createDomainSignal();

    void initProperties(const PropertyObjectPtr& config, DomainSource initialDomainSource, uint32_t initialSamplingIntervalMs);
    void readProperties();
    void propertyChanged();

    void updateStatuses();

    void validateNode();
    bool validateResponse(const OpcUaDataValue& value);
    bool validateValueDataType(const OpcUaDataValue& value);

    void detachFromScheduler();

    std::optional<uint64_t> resolveDomainTimestamp(const OpcUaDataValue& value) const;
    bool isNewDomainTimestamp(uint64_t ts) const;

    DataPackets buildDataPacket(const OpcUaDataValue& value, const std::optional<uint64_t>& domainTs);
    daq::DataPacketPtr buildDomainDataPacket(uint64_t ts);
};

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
