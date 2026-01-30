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

#include "../driver.h"

BasePlugin::BasePlugin(DBC *dbc) : BasePlugin(dbc, nullptr) {}

BasePlugin::BasePlugin(DBC *dbc, BasePlugin *next_plugin) :
    next_plugin(next_plugin),
    plugin_name("BasePlugin") {}

BasePlugin::~BasePlugin() {
    if (next_plugin != this) {
        delete next_plugin;
        next_plugin = nullptr;
    }
}

// codechecker_suppress [misc-no-recursion]
SQLRETURN BasePlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    if (next_plugin && next_plugin != this) {
        return next_plugin->Connect(
            ConnectionHandle,
            WindowHandle,
            OutConnectionString,
            BufferLength,
            StringLengthPtr,
            DriverCompletion
        );
    }
    return SQL_ERROR;
}

// codechecker_suppress [misc-no-recursion]
SQLRETURN BasePlugin::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    if (next_plugin && next_plugin != this) {
        return next_plugin->Execute(StatementHandle, StatementText, TextLength);
    }
    return SQL_ERROR;
}
