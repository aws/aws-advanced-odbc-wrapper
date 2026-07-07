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

#include "base_saml_auth_plugin.h"

#include "../../util/logger_wrapper.h"

BaseSamlAuthPlugin::BaseSamlAuthPlugin(
    DBC* dbc,
    std::shared_ptr<BasePlugin> next_plugin,
    const std::shared_ptr<SamlUtil>& saml_util,
    const std::shared_ptr<AuthProvider>& auth_provider,
    std::shared_ptr<Dialect> dialect,
    std::shared_ptr<OdbcHelper> odbc_helper)
    : BaseTokenAuthPlugin(dbc, next_plugin, auth_provider, dialect, odbc_helper)
{
    this->saml_util = saml_util;
}

BaseSamlAuthPlugin::~BaseSamlAuthPlugin()
{
    if (saml_util) {
        saml_util.reset();
    }
}

bool BaseSamlAuthPlugin::RefreshCredentials(DBC* dbc, const std::string& region)
{
    // Refresh SAML credentials before generating a new token
    const std::string saml_assertion = saml_util->GetSamlAssertion();
    const Aws::Auth::AWSCredentials credentials = saml_util->GetAwsCredentials(saml_assertion);
    auth_provider->UpdateAwsCredential(credentials);
    return true;
}
