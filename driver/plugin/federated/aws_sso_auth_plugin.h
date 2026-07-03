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

#ifndef AWS_SSO_AUTH_PLUGIN_H_
#define AWS_SSO_AUTH_PLUGIN_H_

#include "../../util/auth_provider.h"

#include "../base_plugin.h"
#include "../../driver.h"
#include "../../dialect/dialect.h"
#include "../../util/odbc_helper.h"
#include "sso_browser_login_util.h"

#include <memory>

class AwsSsoAuthPlugin : public BasePlugin {
public:
    explicit AwsSsoAuthPlugin(DBC* dbc);
    AwsSsoAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin);
    AwsSsoAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin,
                     const std::shared_ptr<SsoBrowserLoginUtil>& login_util,
                     const std::shared_ptr<AuthProvider>& auth_provider,
                     std::shared_ptr<Dialect> dialect = nullptr,
                     std::shared_ptr<OdbcHelper> odbc_helper = nullptr);
    ~AwsSsoAuthPlugin() override;

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

private:
    static std::string ResolveRegion(DBC* dbc);

    std::shared_ptr<SsoBrowserLoginUtil> login_util_;
    std::shared_ptr<AuthProvider> auth_provider_;
    std::shared_ptr<Dialect> dialect_;
    std::shared_ptr<OdbcHelper> odbc_helper_;
};

#endif // AWS_SSO_AUTH_PLUGIN_H_
