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

#include "plugin_manager.h"

#include <utility>

#include "../plugin/base_plugin.h"
#include "plugin_chain_builder.h"

PluginManager::PluginManager(std::shared_ptr<BasePlugin> plugin_chain)
    : plugin_chain_{ std::move(plugin_chain) } {}

void PluginManager::InitPlugins(DBC* dbc)
{
    plugin_chain_ = PluginChainBuilder::Build(dbc);
}

void PluginManager::InitMonitoringPlugins(DBC* dbc)
{
    plugin_chain_ = PluginChainBuilder::MonitoringBuild(dbc);
}

std::shared_ptr<BasePlugin> PluginManager::GetPluginChain() const
{
    return plugin_chain_;
}

SQLRETURN PluginManager::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion) const
{
    return plugin_chain_->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
}

SQLRETURN PluginManager::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength) const
{
    return plugin_chain_->Execute(StatementHandle, StatementText, TextLength);
}

void PluginManager::ReleaseResources() const
{
    if (plugin_chain_) {
        plugin_chain_->ReleaseResources();
    }
}
