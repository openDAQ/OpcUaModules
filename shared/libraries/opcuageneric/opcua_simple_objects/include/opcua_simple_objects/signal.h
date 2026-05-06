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

#include <opcua_simple_objects/common.h>
#include <opcuaserver/common.h>
#include <opendaq/signal_ptr.h>
#include <unordered_map>
#include "opcuashared/opcuanodeid.h"
#include "opcuashared/opcuavariant.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_OBJECTS
class SignalNode
{
public:
    explicit SignalNode(daq::opcua::OpcUaServerPtr server,
                        const OpcUaNodeId parentNodeId,
                        const SignalPtr& signal,
                        const PropertyObjectPtr& config = nullptr);
    ~SignalNode() = default;

    void process();

protected:
    static std::unordered_map<SampleType, uint32_t> converterMap;

    daq::opcua::OpcUaServerPtr server;
    SignalPtr signal;
    OpcUaNodeId parentNodeId;
    OpcUaNodeId variableNodeId;
    const uint16_t namespaceIndex = 1;

    void addVariableNode();
    OpcUaNodeId convertSampleTypeToDataTypeId(const daq::SampleType sampleType) const;
    OpcUaVariant toVariant(const BaseObjectPtr& lastValue, SampleType sampleType) const;

};

END_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_OBJECTS
