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

#ifndef IAM_AUTH_PLUGIN_H_
#define IAM_AUTH_PLUGIN_H_

#include "../../util/auth_provider.h"

#include "../base_plugin.h"
#include "../../driver.h"

class IamAuthPlugin : public BasePlugin {
public:
    IamAuthPlugin(DBC* dbc);
    IamAuthPlugin(DBC* dbc, BasePlugin* next_plugin);
    IamAuthPlugin(DBC* dbc, BasePlugin* next_plugin, const std::shared_ptr<AuthProvider>& auth_provider);
    ~IamAuthPlugin() override;

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

private:
    std::shared_ptr<AuthProvider> auth_provider;
};

#endif // IAM_AUTH_PLUGIN_H_
