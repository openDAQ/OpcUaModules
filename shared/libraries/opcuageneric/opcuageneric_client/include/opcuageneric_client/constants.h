#pragma once

#include "opcuageneric.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

// Constants
static const char* GENERIC_OPCUA_CLIENT_DEVICE_NAME = "GenericOPCUAClientPseudoDevice";
static const char* DaqOpcUaGenericDevicePrefix = "daq.opcua.generic";
static const char* OpcUaGenericScheme = "opc.tcp";

static const char* OPCUA_LOCAL_MONITORED_ITEM_FB_ID_PREFIX = "MonitoredItemFb";
static constexpr const char* OPCUA_VALUE_SIGNAL_LOCAL_ID = "OpcUaValueSignal";
static constexpr const char* OPCUA_TS_SIGNAL_LOCAL_ID = "OpcUaDomainSignal";



// FB names
static constexpr const char* GENERIC_OPCUA_MONITORED_ITEM_FB_NAME = "MonitoredItem";

// Property names
// ----------
// Device and module
static constexpr const char* PROPERTY_NAME_OPCUA_USERNAME = "Username";
static constexpr const char* PROPERTY_NAME_OPCUA_PASSWORD = "Password";
static constexpr const char* PROPERTY_NAME_OPCUA_MI_LOCAL_ID = "LocalId";
static constexpr const char* PROPERTY_NAME_OPCUA_TS_MODE = "TimestampMode";

// MonitoredItem FB
static constexpr const char* PROPERTY_NAME_OPCUA_NODE_ID_TYPE = "NodeIDType";
static constexpr const char* PROPERTY_NAME_OPCUA_NODE_ID_STRING = "NodeIDString";
static constexpr const char* PROPERTY_NAME_OPCUA_NODE_ID_NUMERIC = "NodeIDNumeric";
static constexpr const char* PROPERTY_NAME_OPCUA_NAMESPACE_INDEX = "NamespaceIndex";
static constexpr const char* PROPERTY_NAME_OPCUA_SAMPLING_INTERVAL = "SamplingInterval";
// ----------

// Defaults
// ----------
// Device and module
static constexpr const char* DEFAULT_OPCUA_HOST = "127.0.0.1";
static constexpr uint16_t DEFAULT_OPCUA_PORT = 4840;
static constexpr const char* DEFAULT_OPCUA_USERNAME = "";
static constexpr const char* DEFAULT_OPCUA_PASSWORD = "";
static constexpr const char* DEFAULT_OPCUA_PATH = "";

static constexpr uint32_t DEFAULT_OPCUA_MIFB_SAMPLING_INTERVAL = 100;     // in milliseconds
static constexpr uint32_t DEFAULT_RECONNECT_INTERVAL = 5000;           // in milliseconds
// ----------

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
