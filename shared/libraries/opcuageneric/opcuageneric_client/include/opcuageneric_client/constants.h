#pragma once

#include "opcuageneric.h"

BEGIN_NAMESPACE_OPENDAQ_OPCUA_GENERIC

static const char* GENERIC_OPCUA_CLIENT_DEVICE_NAME = "GenericOPCUAClientPseudoDevice";
static const char* DaqOpcUaGenericDevicePrefix = "daq.opcua.generic";
static const char* OpcUaGenericScheme = "opc.tcp";

// Property names
static constexpr const char* PROPERTY_NAME_OPCUA_HOST = "Host";
static constexpr const char* PROPERTY_NAME_OPCUA_PORT = "Port";
static constexpr const char* PROPERTY_NAME_OPCUA_PATH = "Path";
static constexpr const char* PROPERTY_NAME_OPCUA_USERNAME = "Username";
static constexpr const char* PROPERTY_NAME_OPCUA_PASSWORD = "Password";

// Defaults
static constexpr const char* DEFAULT_OPCUA_HOST = "127.0.0.1";
static constexpr uint16_t DEFAULT_OPCUA_PORT = 4840;
static constexpr const char* DEFAULT_OPCUA_USERNAME = "";
static constexpr const char* DEFAULT_OPCUA_PASSWORD = "";
static constexpr const char* DEFAULT_OPCUA_PATH = "";

static constexpr const char* GENERIC_OPCUA_MONITORED_ITEM_FB_NAME = "MonitoredItem";

END_NAMESPACE_OPENDAQ_OPCUA_GENERIC
