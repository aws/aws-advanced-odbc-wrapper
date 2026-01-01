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

#ifndef CUSTOM_PLUGIN_H_
#define CUSTOM_PLUGIN_H_

#include "base_plugin.h"
#include "../util/rds_lib_loader.h"

#include <unordered_map>
#include <memory>

/* Function Pointer Headers */
typedef SQLRETURN (*RDS_FP_Connect)(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion);

typedef SQLRETURN (*RDS_FP_Execute)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength);

/* Custom Plugin Loader */
class CustomPlugin : public BasePlugin {
public:
    CustomPlugin(std::string plugin_name, DBC* dbc, BasePlugin* next_plugin) : BasePlugin(dbc, next_plugin)
    {
        this->plugin_name = plugin_name;

        std::string plugin_settings = dbc->conn_attr[plugin_name];
        std::string plugin_path;
        lib_loader = std::make_shared<RdsLibLoader>(plugin_path);
    }

    ~CustomPlugin() override {
        this->lib_loader.reset();
    };

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override
    {
        RdsLibResult res = lib_loader->CallFunction<RDS_FP_Connect>("Connect", ConnectionHandle, WindowHandle,
            OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
        if (!SQL_SUCCEEDED(res.fn_result)) {
            return SQL_ERROR;
        }
        return next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText = 0,
        SQLINTEGER     TextLength = -1) override
    {
        RdsLibResult res = lib_loader->CallFunction<RDS_FP_Connect>("Execute", StatementHandle, StatementText, TextLength);
        if (!SQL_SUCCEEDED(res.fn_result)) {
            return SQL_ERROR;
        }
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }

protected:
    BasePlugin* next_plugin;
    std::string plugin_name;

private:
    std::shared_ptr<RdsLibLoader> lib_loader;
};

#endif // CUSTOM_PLUGIN_H_
