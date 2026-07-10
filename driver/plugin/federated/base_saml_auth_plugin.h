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

#ifndef BASE_SAML_AUTH_PLUGIN_H_
#define BASE_SAML_AUTH_PLUGIN_H_

#include "../../util/auth_provider.h"
#include "../../dialect/dialect.h"
#include "../../util/odbc_helper.h"

#include "saml_util.h"
#include "../base_token_auth_plugin.h"
#include "../../driver.h"

#include <memory>

class BaseSamlAuthPlugin : public BaseTokenAuthPlugin {
public:
    BaseSamlAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin,
                       const std::shared_ptr<SamlUtil>& saml_util,
                       const std::shared_ptr<AuthProvider>& auth_provider,
                       std::shared_ptr<Dialect> dialect = nullptr,
                       std::shared_ptr<OdbcHelper> odbc_helper = nullptr);
    ~BaseSamlAuthPlugin() override;

protected:
    bool RefreshCredentials(DBC* dbc, const std::string& region) override;

    std::shared_ptr<SamlUtil> saml_util;
};

#endif // BASE_SAML_AUTH_PLUGIN_H_
