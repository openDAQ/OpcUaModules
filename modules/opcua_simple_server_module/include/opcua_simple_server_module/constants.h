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

#include <opcua_simple_server_module/common.h>

BEGIN_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_SERVER_MODULE

static const char* DAQ_OPCUA_SIMPLE_SERVER_ID = "OpenDAQSimpleOPCUA";
static const char* DAQ_OPCUA_SIMPLE_SERVER_MODULE_NAME = "OpenDAQOPCUASimpleServerModule";
static const char* DAQ_OPCUA_SIMPLE_SERVER_MODULE_ID = "OpenDAQOPCUASimpleServerModule";

static const uint16_t DAQ_OPCUA_SIMPLE_SERVER_DEFAULT_PORT = 4840;
static const char* DAQ_OPCUA_SIMPLE_SERVER_DEFAULT_PATH = "/";


END_NAMESPACE_OPENDAQ_OPCUA_SIMPLE_SERVER_MODULE
