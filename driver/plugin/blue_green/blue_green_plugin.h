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

#ifndef BLUE_GREEN_PLUGIN_H_
#define BLUE_GREEN_PLUGIN_H_

#include "blue_green_status.h"
#include "blue_green_status_provider.h"

#include "../base_plugin.h"

#include "../../util/odbc_helper.h"
#include "../../util/plugin_service.h"

#include <string>

class BlueGreenPlugin : public BasePlugin {
public:
    BlueGreenPlugin() = default;
    BlueGreenPlugin(DBC* dbc);
    BlueGreenPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin);
    ~BlueGreenPlugin();

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText,
        SQLINTEGER     TextLength) override;

    int64_t GetHoldTime();
    void ResetRoutingTiming();

private:
    SQLRETURN InitConnection(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion);
    void InitProvider();
    std::shared_ptr<BlueGreenStatusProvider> GetOrCreateProvider();

    std::string blue_green_id_;
    std::string cluster_id_;

    std::shared_ptr<BlueGreenStatusProvider> status_provider_;
    std::map<std::string, std::string> conn_attr_;
    std::shared_ptr<PluginService> plugin_service_;
    std::shared_ptr<OdbcHelper> odbc_helper_;

    BlueGreenStatus blue_green_status_;

    std::chrono::system_clock::time_point start_time_;
    std::chrono::system_clock::time_point end_time_;

    static std::mutex provider_lock_;
    static std::map<std::string, std::pair<unsigned int, std::shared_ptr<BlueGreenStatusProvider>>> status_providers_map_;
    static std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_map_;
};

#endif // BLUE_GREEN_PLUGIN_H_
