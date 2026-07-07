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

#include "../base_token_auth_plugin.h"
#include "../../driver.h"
#include "../../dialect/dialect.h"
#include "../../util/odbc_helper.h"

#include <memory>

class IamAuthPlugin : public BaseTokenAuthPlugin {
public:
    IamAuthPlugin(DBC* dbc);
    IamAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin);
    IamAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin, const std::shared_ptr<AuthProvider>& auth_provider);
    IamAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin,
                  const std::shared_ptr<AuthProvider>& auth_provider,
                  std::shared_ptr<Dialect> dialect,
                  std::shared_ptr<OdbcHelper> odbc_helper);

protected:
    std::string ResolveRegion(DBC* dbc) override;
    bool EnsureCredentials(DBC* dbc, const std::string& region, std::string& out_error) override;
};

#endif // IAM_AUTH_PLUGIN_H_
