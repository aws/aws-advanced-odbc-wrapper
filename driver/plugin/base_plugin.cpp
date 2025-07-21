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

#include "base_plugin.h"

#include "../odbcapi.h"
#include "../util/connection_string_helper.h"
#include "../util/logger_wrapper.h"
#include "../util/rds_lib_loader.h"

BasePlugin::BasePlugin(DBC *dbc) : BasePlugin(dbc, nullptr) {}

BasePlugin::BasePlugin(DBC *dbc, BasePlugin *next_plugin) :
    dbc(dbc),
    next_plugin(next_plugin),
    plugin_name("BasePlugin")
{
    if (!dbc) {
        throw std::runtime_error("DBC cannot be null.");
    }
}

BasePlugin::~BasePlugin()
{
}

SQLRETURN BasePlugin::Connect(
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    SQLRETURN ret = SQL_ERROR;
    bool has_conn_attr_errors = false;
    ENV* env = dbc->env;

    // TODO - Should a new connect use a new underlying DBC?
    // Create Wrapped DBC if not already allocated
    RdsLibResult res;
    if (!dbc->wrapped_dbc) {
        res = env->driver_lib_loader->CallFunction<RDS_FP_SQLAllocHandle>(RDS_STR_SQLAllocHandle,
            SQL_HANDLE_DBC, env->wrapped_env, &dbc->wrapped_dbc
        );
    }

    // DSN should be read from the original input
    // and a new connection string should be built without DSN & Driver
    RDS_STR conn_in = ConnectionStringHelper::BuildFullConnectionString(dbc->conn_attr);
    res = env->driver_lib_loader->CallFunction<RDS_FP_SQLDriverConnect>(RDS_STR_SQLDriverConnect,
        dbc->wrapped_dbc, WindowHandle, AS_SQLTCHAR(conn_in.c_str()), SQL_NTS, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion
    );

    if (res.fn_load_success) {
        ret = res.fn_result;
    }

    // Apply Tracked Connection Attributes
    for (auto const& [key, val] : dbc->attr_map) {
        res = env->driver_lib_loader->CallFunction<RDS_FP_SQLSetConnectAttr>(RDS_STR_SQLSetConnectAttr,
            dbc->wrapped_dbc, key, val.first, val.second
        );
        if (!res.fn_result) {
            LOG(WARNING) << "Error setting connection attribute";
        }
        has_conn_attr_errors != res.fn_result;
    }

    // TODO - Error Handling for ConnAttr, IsConnected
    // Successful Connection, but bad environment and/or connection attribute setting
    if (SQL_SUCCEEDED(ret) && has_conn_attr_errors) {
        // TODO - Set Error
        ret = SQL_SUCCESS_WITH_INFO;
    }
    return ret;
}
