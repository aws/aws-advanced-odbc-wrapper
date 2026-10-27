// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLUGIN_MANAGER_H_
#define PLUGIN_MANAGER_H_

#include "windows_headers.h"

#include <sql.h>

#include <memory>

class BasePlugin;
struct DBC;

// Owns the plugin chain of a connection and is the entry point for calls into it.
// Held by the connection's FullServiceContainer, and only published there once the chain is built.
class PluginManager {
public:
    PluginManager() = default;
    explicit PluginManager(std::shared_ptr<BasePlugin> plugin_chain);

    // Builds the chain from the DBC's connection attributes. The plugins read the DBC's PluginService while
    // being constructed, so it must already be set. Throws if the configuration is invalid
    void InitPlugins(DBC* dbc);
    // Builds the reduced chain that monitors open their own connections with
    void InitMonitoringPlugins(DBC* dbc);

    [[nodiscard]] std::shared_ptr<BasePlugin> GetPluginChain() const;

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) const;

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText = nullptr,
        SQLINTEGER     TextLength = -1) const;

    void ReleaseResources() const;

private:
    std::shared_ptr<BasePlugin> plugin_chain_;
};

#endif // PLUGIN_MANAGER_H_
