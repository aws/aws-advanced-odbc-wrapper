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

#ifndef ADFS_AUTH_PLUGIN_H_
#define ADFS_AUTH_PLUGIN_H_

#include "../../util/auth_provider.h"

#include "../base_plugin.h"

#include "saml_util.h"

class AdfsSamlUtil : public SamlUtil {
public:
    AdfsSamlUtil(const std::map<RDS_STR, RDS_STR> &connection_attributes);
    AdfsSamlUtil(const std::map<RDS_STR, RDS_STR> &connection_attributes, const std::shared_ptr<Aws::Http::HttpClient> &http_client, const std::shared_ptr<Aws::STS::STSClient> &sts_client);
    std::string GetSamlAssertion();

private:
    std::map<std::string, std::string> GetParameterFromBody(std::string& body);
    std::string GetFormActionBody(const std::string& url, const std::map<std::string, std::string>& params);
    static bool ValidateUrl(const std::string& url);
    static std::vector<std::string> GetInputTagsFromBody(const std::string& body);
    static std::string GetValueByKey(const std::string& input, const std::string& key);

    std::string sign_in_url;

    static inline const std::string DEFAULT_RELAY_ID = "urn:amazon:webservices";
    static inline const std::regex FORM_ACTION_PATTERN = std::regex("<form.*?action=\"([^\"]+)\"");
    static inline const std::regex INPUT_TAG_PATTERN = std::regex("<input id=(.*)");
    static inline const std::regex SAML_RESPONSE_PATTERN = std::regex("name=\"SAMLResponse\"\\s+value=\"([^\"]+)\"");
    static inline const std::regex URL_PATTERN = std::regex("^(https)://[-a-zA-Z0-9+&@#/%?=~_!:,.']*[-a-zA-Z0-9+&@#/%=~_']");
};

class AdfsAuthPlugin : public BasePlugin {
public:
    AdfsAuthPlugin(DBC* dbc);
    AdfsAuthPlugin(DBC* dbc, BasePlugin* next_plugin);
    AdfsAuthPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<SamlUtil> &saml_util, const std::shared_ptr<AuthProvider> &auth_provider);
    ~AdfsAuthPlugin() override;

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

#endif // ADFS_AUTH_PLUGIN_H_
