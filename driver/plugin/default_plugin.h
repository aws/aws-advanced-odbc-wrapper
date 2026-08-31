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

#ifndef DEFAULT_PLUGIN_H_
#define DEFAULT_PLUGIN_H_

#include <memory>

#include "../util/odbc_helper.h"
#include "base_plugin.h"

class DefaultPlugin : public BasePlugin {
public:
    DefaultPlugin() = default;
    explicit DefaultPlugin(DBC* dbc);
    DefaultPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin);

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

    SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText = nullptr,
        SQLINTEGER     TextLength = -1) override;

private:
    std::shared_ptr<OdbcHelper>odbc_helper_;
};

#endif // DEFAULT_PLUGIN_H_
