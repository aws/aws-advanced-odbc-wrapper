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

#ifndef OKTA_AUTH_PLUGIN_H_
#define OKTA_AUTH_PLUGIN_H_

#include "../../util/auth_provider.h"

#include "saml_util.h"
#include "../base_plugin.h"
#include "../../driver.h"

class OktaSamlUtil : public SamlUtil {
public:
    OktaSamlUtil(const std::map<RDS_STR, RDS_STR> &connection_attributes);
    OktaSamlUtil(const std::map<RDS_STR, RDS_STR> &connection_attributes, const std::shared_ptr<Aws::Http::HttpClient> &http_client, const std::shared_ptr<Aws::STS::STSClient> &sts_client);
    std::string GetSamlAssertion();

private:
    std::string GetSessionToken();

    std::string sign_in_url;
    std::string session_token_url;

    static inline const std::regex SAML_RESPONSE_PATTERN = std::regex("<input name=\"SAMLResponse\".+value=\"(.+)\"/\\>");
};

class OktaAuthPlugin : public BasePlugin {
public:
    OktaAuthPlugin(DBC* dbc);
    OktaAuthPlugin(DBC* dbc, BasePlugin* next_plugin);
    OktaAuthPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<SamlUtil> &saml_util, const std::shared_ptr<AuthProvider> &auth_provider);
    ~OktaAuthPlugin() override;

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

private:
    std::shared_ptr<AuthProvider> auth_provider;
    std::shared_ptr<SamlUtil> saml_util;
};

#endif // OKTA_AUTH_PLUGIN_H_
