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

#include "limitless_plugin.h"

#include "../../dialect/dialect.h"
#include "../../dialect/dialect_aurora_postgres.h"
#include "../../util/connection_string_helper.h"
#include "../../util/init_plugin_helper.h"

LimitlessPlugin::LimitlessPlugin(DBC *dbc) : LimitlessPlugin(dbc, nullptr) {}

LimitlessPlugin::LimitlessPlugin(DBC *dbc, BasePlugin *next_plugin) : LimitlessPlugin(dbc, next_plugin, nullptr, nullptr) {}

LimitlessPlugin::LimitlessPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<Dialect>& dialect, const std::shared_ptr<LimitlessRouterService> &limitless_router_service) : BasePlugin(dbc, next_plugin)
{
    const std::map<std::string, std::string> conn_info = dbc->conn_attr;
    this->plugin_name = "LIMITLESS";
    this->dialect_ = dialect ? dialect : InitDialect(conn_info);
    this->limitless_router_service_ = limitless_router_service;
}

LimitlessPlugin::~LimitlessPlugin() {
    this->limitless_router_service_.reset();
}

SQLRETURN LimitlessPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering Connect";
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);
    const std::shared_ptr<DialectLimitless> limitless_dialect = std::dynamic_pointer_cast<DialectLimitless>(this->dialect_);
    if (!limitless_dialect) {
        LOG(ERROR) << "The limitless connection plugin does not support the current dialect or database";
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO("The limitless connection plugin does not support the current dialect or database.", ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return SQL_ERROR;
    }

    const bool limitless_enabled =
        dbc->conn_attr.contains(KEY_ENABLE_LIMITLESS) &&
        dbc->conn_attr.at(KEY_ENABLE_LIMITLESS) == VALUE_BOOL_TRUE;

    if (limitless_enabled) {
        dbc->conn_attr.insert_or_assign(KEY_ENABLE_LIMITLESS, VALUE_BOOL_FALSE);

        if (!this->limitless_router_service_) {
            this->limitless_router_service_ = std::make_shared<LimitlessRouterService>(limitless_dialect, dbc->conn_attr);
        }

        limitless_router_service_->StartMonitoring(dbc, limitless_dialect);
        return limitless_router_service_->EstablishConnection(next_plugin, dbc);
    }

    return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
}
