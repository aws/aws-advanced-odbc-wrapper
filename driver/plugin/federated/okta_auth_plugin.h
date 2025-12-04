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

typedef enum {
    NONE,
    TOTP,
    PUSH
} MfaType;

static std::map<std::string, MfaType> const mfa_type_table = {
    {"", MfaType::NONE},
    {VALUE_MFA_TOTP, MfaType::TOTP},
    {VALUE_MFA_PUSH, MfaType::PUSH}
};

class OktaSamlUtil : public SamlUtil {
public:
    OktaSamlUtil(const std::map<std::string, std::string> &connection_attributes);
    OktaSamlUtil(const std::map<std::string, std::string> &connection_attributes, const std::shared_ptr<Aws::Http::HttpClient> &http_client, const std::shared_ptr<Aws::STS::STSClient> &sts_client);
    std::string GetSamlAssertion();

private:
    std::string GetSessionToken();

    std::string VerifyTOTPChallenge(const std::string &verify_url, const std::string &state_token);

    std::string VerifyPushChallenge(const std::string &verify_url, const std::string &state_token);

    static inline const std::string DEFAULT_MFA_TIMEOUT = "60";
    static inline const int VERIFY_PUSH_INTERVAL = 5;
    static inline const std::string DEFAULT_PORT = "8080";
    static inline const std::string WEBSERVER_HOST = "http://127.0.0.1";
    std::string sign_in_url;
    std::string session_token_url;
    MfaType mfa_type;
    std::string mfa_port;
    std::string mfa_timeout;

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
