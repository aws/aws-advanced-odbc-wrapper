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
    AdfsSamlUtil(std::map<RDS_STR, RDS_STR> connection_attributes);
    std::string GetSamlAssertion();

private:
    std::map<std::string, std::string> GetParameterFromBody(std::string& body);
    std::string GetFormActionBody(std::string& url, std::map<std::string, std::string>& params);
    bool ValidateUrl(const std::string& url);
    std::vector<std::string> GetInputTagsFromBody(const std::string& body);
    std::string GetValueByKey(const std::string& input, const std::string& key);

    std::string sign_in_url;

    static const std::string FORM_ACTION_PATTERN;
    static const std::string INPUT_TAG_PATTERN;
    static const std::string SAML_RESPONSE_PATTERN;
    static const std::string URL_PATTERN;
};

class AdfsAuthPlugin : public BasePlugin {
public:
    AdfsAuthPlugin(DBC* dbc);
    AdfsAuthPlugin(DBC* dbc, BasePlugin* next_plugin);
    ~AdfsAuthPlugin() override;

    SQLRETURN Connect(
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

private:
    std::shared_ptr<AuthProvider> auth_provider;
    std::shared_ptr<AdfsSamlUtil> saml_util;
};

#endif // ADFS_AUTH_PLUGIN_H_
