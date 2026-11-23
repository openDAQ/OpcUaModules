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
#include <open62541/plugin/accesscontrol_default.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>

// A bare open62541 server with the default access control - one user, anonymous login allowed or not.
// The endpoints keep the anonymous and username token policies set up by `UA_ServerConfig_setMinimal`, so
// a client always gets to the session activation, where the access control rejects the identity token.
class OpcUaTestServer
{
public:
    explicit OpcUaTestServer(UA_UInt16 port)
        : port(port)
    {
    }

    ~OpcUaTestServer()
    {
        stop();
    }

    OpcUaTestServer(const OpcUaTestServer&) = delete;
    OpcUaTestServer& operator=(const OpcUaTestServer&) = delete;

    void start(bool allowAnonymous, const std::string& userName, const std::string& password)
    {
        stop();

        server = UA_Server_new();
        UA_ServerConfig* config = UA_Server_getConfig(server);
        UA_ServerConfig_setMinimal(config, port, nullptr);

        UA_UsernamePasswordLogin login;
        login.username = UA_STRING(const_cast<char*>(userName.c_str()));
        login.password = UA_STRING(const_cast<char*>(password.c_str()));
        // the logins are copied into the access control context
        UA_StatusCode status = UA_AccessControl_default(
            config, allowAnonymous, nullptr, &config->securityPolicies[config->securityPoliciesSize - 1].policyUri, 1, &login);
        if (status == UA_STATUSCODE_GOOD)
            status = UA_Server_run_startup(server);
        if (status != UA_STATUSCODE_GOOD)
        {
            UA_Server_delete(server);
            server = nullptr;
            throw std::runtime_error(std::string("Failed to start the OPC UA test server: ") + UA_StatusCode_name(status));
        }

        running = true;
        serverThread = std::thread(
            [this]
            {
                while (running)
                    UA_Server_run_iterate(server, true);
            });
    }

    void stop()
    {
        if (!server)
            return;

        running = false;
        serverThread.join();
        UA_Server_run_shutdown(server);
        UA_Server_delete(server);
        server = nullptr;
    }

private:
    const UA_UInt16 port;
    UA_Server* server = nullptr;
    std::atomic<bool> running{false};
    std::thread serverThread;
};
