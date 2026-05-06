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
#include <opcua_simple_objects/signal.h>
#include <opcuaserver/opcuaserver.h>
#include <opendaq/device_ptr.h>
#include <opendaq/instance_ptr.h>
#include <condition_variable>
#include <vector>
#include <thread>
#include <mutex>

BEGIN_NAMESPACE_OPENDAQ_OPCUA

class GenericServer
{
public:
    GenericServer(const InstancePtr& instance);
    GenericServer(const DevicePtr& device, const ContextPtr& context);
    ~GenericServer();

    void setOpcUaPort(uint16_t port);
    void setOpcUaPath(const std::string& path);
    void start();
    void stop();

protected:
    const uint16_t namespaceIndex = 1;
    DevicePtr device;
    ContextPtr context;

    daq::opcua::OpcUaServerPtr server;
    uint16_t opcUaPort = 4840;
    std::string opcUaPath = "/";
    OpcUaNodeId rootDeviceNodeId;

    std::vector<simple_objects::SignalNode> signalNodes;
    // std::unordered_map<std::string, SizeT> registeredClientIds;

    uint64_t readingIntervalMs;
    std::thread readingThread;
    std::atomic<bool> readingRunning{false};
    std::condition_variable readingCv;
    std::mutex readingMutex;

    void createDeviceNode();
    void fillDeviceNode();
    void addSignalNodes();

    void startReadingThread();
    void stopReadingThread();
    void readingLoop();
};

END_NAMESPACE_OPENDAQ_OPCUA
