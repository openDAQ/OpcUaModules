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

#include <gtest/gtest.h>
#include <open62541/server_config_default.h>
#include <chrono>
#include <future>
#include <optional>
#include <thread>
#include "opcuaclient/opcuaclient.h"
#include "opcuashared/opcua.h"
#include "opcuashared/opcuacommon.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA

#define ASSERT_EQ_STATUS(status, expectedStatus) ASSERT_EQ(status, (UA_StatusCode) expectedStatus)

namespace helper::constants
{
    constexpr uint16_t TEST_NS = 1;
    // an object with device info that is not a part of the Device Integration model
    constexpr const char* PLAIN_DEVICE_STRING_ID = "TestPlainDevice";
    // objects that carry only a part of the device info
    constexpr const char* SERIAL_ONLY_DEVICE_STRING_ID = "TestSerialOnlyDevice";
    constexpr const char* MANUFACTURER_ONLY_DEVICE_STRING_ID = "TestManufacturerOnlyDevice";
    constexpr const char* DI_DEVICE_STRING_ID = "TestDiDevice";
    constexpr const char* DI_FOREIGN_OBJECT_STRING_ID = "TestDiForeignObject";
    constexpr const char* DI_STANDALONE_DEVICE_STRING_ID = "TestDiStandaloneDevice";
    constexpr uint32_t DI_STANDALONE_DEVICE_NUMERIC_ID = 2001;
    constexpr const char* DI_DEVICE_TYPE_STRING_ID = "TestDiDeviceType";

    // OPC UA Device Integration model (OPC 10000-100)
    constexpr const char* DI_NAMESPACE_URI = "http://opcfoundation.org/UA/DI/";
    constexpr uint32_t DI_TOPOLOGY_ELEMENT_TYPE_ID = 1001;
    constexpr uint32_t DI_DEVICE_TYPE_ID = 1002;
    constexpr uint32_t DI_DEVICE_SET_ID = 5001;
    constexpr uint32_t DI_COMPONENT_TYPE_ID = 15063;

    constexpr const char* EXPECTED_MANUFACTURER = "openDAQ test manufacturer";
    constexpr const char* EXPECTED_MANUFACTURER_URI = "https://www.opendaq.com/opcua-test-manufacturer";
    constexpr const char* EXPECTED_MODEL = "TEST MODEL";
    constexpr const char* EXPECTED_HW_REVISION = "HW-1.0";
    constexpr const char* EXPECTED_SW_REVISION = "SW-2.3.4";
    constexpr const char* EXPECTED_DEV_REVISION = "DEV-5.6.7";
    constexpr const char* EXPECTED_SERIAL = "SN-1234567";
    constexpr const char* EXPECTED_STANDALONE_SERIAL = "SN-STANDALONE";
    constexpr const char* EXPECTED_PLAIN_SERIAL = "SN-PLAIN";
    constexpr const char* EXPECTED_SERIAL_ONLY_SERIAL = "SN-ONLY";
    constexpr const char* EXPECTED_PRODUCT_CODE = "TEST PRODUCT CODE";
    constexpr const char* EXPECTED_DEVICE_MANUAL = "TEST DEVICE MANUAL";
    constexpr const char* EXPECTED_DEVICE_CLASS = "TEST DEVICE CLASS";
    constexpr const char* EXPECTED_PRODUCT_INSTANCE_URI = "https://www.opendaq.com/test-product-instance";
    constexpr const char* EXPECTED_ASSET_ID = "testASSET_ID-1234567";
    constexpr const char* EXPECTED_COMPONENT_NAME = "TEST COMPONENT NAME";
    constexpr const UA_Int32 EXPECTED_REVISION_CNT = 33;
}

class OpcUaServerTestHelper final
{
public:
    using OnConfigureCallback = std::function<void(UA_ServerConfig* config)>;

    // What the server publishes of the OPC UA Device Integration model
    struct DiModel
    {
        size_t deviceCount = 0;            // devices referenced from the DeviceSet
        bool foreignObject = false;        // a TopologyElementType object in the DeviceSet, which is not a device
        bool legacyTypeHierarchy = false;  // DeviceType derived from TopologyElementType directly, as before DI 1.2
        bool standaloneDevices = false;    // devices outside the DeviceSet, one with a string and one with a numeric node ID
    };

    OpcUaServerTestHelper();
    ~OpcUaServerTestHelper();

    void setSessionTimeout(double sessionTimeoutMs);
    // Takes effect on the next startServer(). Without it the server has no Device Integration model at all.
    void setDiModel(const DiModel& model);

    void onConfigure(const OnConfigureCallback& callback);
    void onTweakConfig(const OnConfigureCallback& callback);
    void startServer();
    void stop();

    std::string getServerUrl() const;

    void publishVariable(std::string identifier,
                         const void* value,
                         const UA_DataType* type,
                         UA_NodeId* parentNodeId,
                         const char* locale = "en_US",
                         uint16_t nodeIndex = 1,
                         size_t dimension = 1,
                         UA_Byte accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE);

    void publishVariable(uint32_t numericId,
                         const void* value,
                         const UA_DataType* type,
                         UA_NodeId* parentNodeId,
                         const char* locale = "en_US",
                         uint16_t nodeIndex = 1,
                         size_t dimension = 1,
                         UA_Byte accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE);

    void writeValueNode(const OpcUaNodeId& nodeId, const OpcUaVariant& value);
    void writeDataValueNode(const OpcUaNodeId& nodeId, const OpcUaDataValue& value);

private:
    void runServer();
    void createModel();
    void createDiModel(const DiModel& model);
    void publishObjectType(const OpcUaNodeId& nodeId, const char* name, const OpcUaNodeId& superTypeId);
    void publishObject(const OpcUaNodeId& nodeId,
                       const char* name,
                       const OpcUaNodeId& parentNodeId,
                       const OpcUaNodeId& referenceTypeId,
                       const OpcUaNodeId& typeDefinitionId);
    void publishDeviceInfoProperties(OpcUaNodeId deviceNodeId, const char* serialNumber);
    void publishFolder(const char* identifier, UA_NodeId* parentNodeId, const char* locale = "en_US", int nodeIndex = 1);
    void publishMethod(std::string identifier, UA_NodeId* parentNodeId, const char* locale = "en_US", int nodeIndex = 1);

    void publishVariableImpl(OpcUaNodeId nodeId,
                             const std::string& name,
                             const void* value,
                             const UA_DataType* type,
                             UA_NodeId* parentNodeId,
                             const char* locale,
                             uint16_t nodeIndex,
                             size_t dimension,
                             UA_Byte accessLevel);

    void addPropertyImpl(const std::string& name,
                         const void* value,
                         const UA_DataType* type,
                         UA_NodeId* parentNodeId,
                         const char* locale = "en_US",
                         uint16_t nodeIndex = 1,
                         UA_Byte accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE);

    static UA_StatusCode helloMethodCallback(UA_Server* server,
                                             const UA_NodeId* sessionId,
                                             void* sessionHandle,
                                             const UA_NodeId* methodId,
                                             void* methodContext,
                                             const UA_NodeId* objectId,
                                             void* objectContext,
                                             size_t inputSize,
                                             const UA_Variant* input,
                                             size_t outputSize,
                                             UA_Variant* output);

    double sessionTimeoutMs;
    std::optional<DiModel> diModel;
    UA_Server* server{};
    std::unique_ptr<std::thread> serverThreadPtr;
    std::atomic<UA_Boolean> serverRunning = false;

    UA_UInt16 port = 4842u;
    OnConfigureCallback onConfigureCallback;
    OnConfigureCallback onTweakConfigCallback;
};

class BaseClientTest : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;
    OpcUaServerTestHelper testHelper;

    std::string getServerUrl() const;

    static void IterateAndWaitForPromise(OpcUaClient& client, const std::future<void>& future)
    {
        using namespace std::chrono;
        while (client.iterate(milliseconds(10)) == UA_STATUSCODE_GOOD && future.wait_for(milliseconds(1)) != std::future_status::ready)
        {
        };
    }

    OpcUaClientPtr prepareAndConnectClient(int timeout = -1);
};

END_NAMESPACE_OPENDAQ_OPCUA
