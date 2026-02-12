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

#ifndef AURORA_INITIAL_CONNECTION_STRATEGY_PLUGIN_H
#define AURORA_INITIAL_CONNECTION_STRATEGY_PLUGIN_H

#include "../base_plugin.h"
#include "../../host_info.h"
#include "../../driver.h"
#include "../../host_selector/host_selector.h"
#include "../../util/plugin_service.h"

class AuroraInitialConnectionStrategyPlugin : public BasePlugin {
public:
    AuroraInitialConnectionStrategyPlugin(DBC* dbc);
    AuroraInitialConnectionStrategyPlugin(DBC* dbc, BasePlugin* next_plugin);

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

protected:
    std::shared_ptr<PluginService> plugin_service_;
    std::shared_ptr<HostSelector> host_selector_;
    std::shared_ptr<Dialect> dialect_;
    std::shared_ptr<OdbcHelper> odbc_helper_;
    std::shared_ptr<TopologyUtil> topology_util_;
    std::chrono::milliseconds retry_delay_ms_;
    std::chrono::milliseconds retry_timeout_ms_;
    std::string verify_initial_connection_type_;

    SQLRETURN GetVerifiedWriter(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion);
    SQLRETURN GetVerifiedReader(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion);
    HostInfo GetReader(const std::string region);
    bool HasNoReaders();

    static constexpr int MAX_STATE_LENGTH = 32;
    static constexpr int MAX_MSG_LENGTH = 1024;
    static inline const std::chrono::milliseconds DEFAULT_INITIAL_CONNECTION_RETRY_INTERVAL_MS = std::chrono::milliseconds(1000);
    static inline const std::chrono::milliseconds DEFAULT_INITIAL_CONNECTION_RETRY_TIMEOUT_MS = std::chrono::milliseconds(30000);
};

#endif  // AURORA_INITIAL_CONNECTION_STRATEGY_PLUGIN_H
