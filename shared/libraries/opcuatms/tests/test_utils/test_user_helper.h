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

#include <coreobjects/authentication_provider_factory.h>
#include "coreobjects/permission_mask_builder_factory.h"
#include "coreobjects/permissions_builder_factory.h"
#include "coreobjects/user_factory.h"
#include "opcuaserver/opcuasession.h"

namespace test_helpers
{
    inline daq::PermissionsBuilderPtr CreatePermissionsBuilder()
    {
        using namespace daq;
        return PermissionsBuilder()
            .inherit(false)
            .assign("everyone", PermissionMaskBuilder())
            .assign("reader", PermissionMaskBuilder().read())
            .assign("writer", PermissionMaskBuilder().read().write())
            .assign("executor", PermissionMaskBuilder().read().execute())
            .assign("admin", PermissionMaskBuilder().read().write().execute());
    }

    inline auto CreateUsers()
    {
        using namespace daq;
        auto users = List<IUser>();
        const std::vector<std::pair<std::string, std::string>> templateForUser = {
            {"common", ""}, {"reader", "reader"}, {"writer", "writer"}, {"executor", "executor"}, {"admin", "admin"}};
        for (const auto& [user, group] : templateForUser)
        {
            if (group.empty())
                users.pushBack(User(user + "User", user + "UserPass"));
            else
                users.pushBack(User(user + "User", user + "UserPass", {group}));
        }
        return users;
    }

    inline daq::opcua::OpcUaSession createSession(uint32_t id, const std::string& username, std::string password = "", std::vector<std::string> roles = {})
    {
        if (password == "")
            password = username + "Pass";
        return daq::opcua::OpcUaSession(daq::opcua::OpcUaNodeId(id), nullptr, daq::User(username, password, roles));
    }

    inline daq::opcua::OpcUaSession createSessionCommon(const std::string& username, const std::string& password = "")
    {
        return createSession(0, username, password);
    }

    inline daq::opcua::OpcUaSession createSessionReader(const std::string& username, const std::string& password = "")
    {
        return createSession(1, username, password, {"reader"});
    }

    inline daq::opcua::OpcUaSession createSessionWriter(const std::string& username, const std::string& password = "")
    {
        return createSession(2, username, password, {"writer"});
    }

    inline daq::opcua::OpcUaSession createSessionExecutor(const std::string& username, const std::string& password = "")
    {
        return createSession(3, username, password, {"executor"});
    }

    inline daq::opcua::OpcUaSession createSessionAdmin(const std::string& username, const std::string& password = "")
    {
        return createSession(4, username, password, {"admin"});
    }
}

