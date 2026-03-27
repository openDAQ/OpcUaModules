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

namespace utils
{
    class Error
    {
    public:
        Error(std::string name)
            : name(std::move(name))
            , present(false)
            , updated(true)
        {
        }

        void set(std::string msg)
        {
            updated = true;
            present = true;
            message = std::move(msg);
        }

        void add(const std::string& msg)
        {
            if (message.empty())
            {
                set(msg);
            }
            else
            {
                updated = true;
                present = true;
                message.reserve(msg.size() + 2);
                message.append("; ");
                message.append(msg);
            }
        }

        void reset()
        {
            if (present)
                updated = true;
            present = false;
            message.clear();
        }
        bool ok() const
        {
            return !present;
        }

        const std::string& getMessage() const
        {
            return message;
        }

        bool isUpdated() const
        {
            bool tmp = updated;
            updated = false;
            return tmp;
        }

    protected:
        const std::string name;
        std::string message;
        bool present;
        mutable bool updated;
    };

    class StatusContainer
    {
    public:
        StatusContainer() = default;

        bool addStatus(const std::string& name)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            if (map.count(name))
                return false;
            map.emplace(std::pair<std::string, Error>(name, Error(name)));
            return true;
        }

        bool isUpdated()
        {
            std::scoped_lock<std::mutex> lock(mtx);
            bool result = false;
            for (const auto& e : map)
            {
                bool isUpdated = e.second.isUpdated();
                result |= isUpdated;
            }
            return result;
        }
        void resetAll()
        {
            std::scoped_lock<std::mutex> lock(mtx);
            for (auto& e : map)
                e.second.reset();
        }

        void set(const std::string& name, std::string msg)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            getStatus(name).set(std::move(msg));
        }

        void addToStatus(const std::string& name, const std::string& msg)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            getStatus(name).add(msg);
        }

        void reset(const std::string& name)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            getStatus(name).reset();
        }

        bool ok(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            return getStatus(name).ok();
        }

        std::string getMessage(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            return getStatus(name).getMessage();
        }

    protected:
        mutable std::mutex mtx;
        std::unordered_map<std::string, Error> map;

        Error& getStatus(const std::string& name)
        {
            return map.at(name);
        }

        const Error& getStatus(const std::string& name) const
        {
            return map.at(name);
        }
    };
}

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

    enum class DomainSource : int
    {
        None = 0,
        ServerTimestamp,
        SourceTimestamp,
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

    struct DataPackets
    {
        daq::DataPacketPtr dataPacket;
        daq::DataPacketPtr domainDataPacket;
    };

    struct FbConfig {
        NodeIDType nodeIdType;
        std::string nodeId;
        uint32_t namespaceIndex;
        uint32_t samplingInterval;
        DomainSource domainSource;
    };

    static std::atomic<int> localIndex;
    static std::unordered_map<OpcUaNodeId, daq::SampleType> supportedDataTypes;

    DataDescriptorPtr outputSignalDescriptor;
    SignalConfigPtr outputSignal;
    SignalConfigPtr outputDomainSignal;

    FbConfig config;
    daq::opcua::OpcUaClientPtr client;
    OpcUaNodeId nodeId;
    OpcUaNodeId nodeDataType;

    std::thread readerThread;
    std::atomic<bool> running;

    utils::StatusContainer statuses;

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
