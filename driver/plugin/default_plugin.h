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

#include "base_plugin.h"

class DefaultPlugin : public BasePlugin {
public:
    DefaultPlugin() = default;
    DefaultPlugin(DBC* dbc);
    DefaultPlugin(DBC* dbc, DefaultPlugin* next_plugin);

    virtual SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion);

    virtual SQLRETURN Execute(
        SQLHSTMT       StatementHandle,
        SQLTCHAR *     StatementText = 0,
        SQLINTEGER     TextLength = -1);

protected:
    std::string plugin_name;
private:
};

#endif // DEFAULT_PLUGIN_H_
