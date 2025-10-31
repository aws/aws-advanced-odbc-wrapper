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

#include "okta_auth_plugin.h"

#include <unordered_set>

#include "html_util.h"

#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/rds_utils.h"
#include "saml_util.h"

OktaAuthPlugin::OktaAuthPlugin(DBC *dbc) : OktaAuthPlugin(dbc, nullptr) {}

OktaAuthPlugin::OktaAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : OktaAuthPlugin(dbc, next_plugin, nullptr, nullptr) {}

OktaAuthPlugin::OktaAuthPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<SamlUtil> &saml_util, const std::shared_ptr<AuthProvider> &auth_provider) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "OKTA";

    if (saml_util) {
        this->saml_util = saml_util;
    } else {
        this->saml_util = std::make_shared<OktaSamlUtil>(dbc->conn_attr);
    }

    if (auth_provider) {
        this->auth_provider = auth_provider;
    } else {
        std::string region = dbc->conn_attr.contains(KEY_REGION) ?
            ToStr(dbc->conn_attr.at(KEY_REGION)) : "";
        if (region.empty()) {
            region = dbc->conn_attr.contains(KEY_SERVER) ?
                RdsUtils::GetRdsRegion(ToStr(dbc->conn_attr.at(KEY_SERVER)))
                : Aws::Region::US_EAST_1;
        }
        const std::string saml_assertion = this->saml_util->GetSamlAssertion();
        this->auth_provider = std::make_shared<AuthProvider>(region, this->saml_util->GetAwsCredentials(saml_assertion));
    }
}

OktaAuthPlugin::~OktaAuthPlugin()
{
    if (auth_provider) {
        auth_provider.reset();
    }
    if (saml_util) {
        saml_util.reset();
    }
}

SQLRETURN OktaAuthPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::string server = dbc->conn_attr.contains(KEY_SERVER) ?
        ToStr(dbc->conn_attr.at(KEY_SERVER)) : "";
    const std::string iam_host = dbc->conn_attr.contains(KEY_IAM_HOST) ?
        ToStr(dbc->conn_attr.at(KEY_IAM_HOST)) : server;
    std::string region = dbc->conn_attr.contains(KEY_REGION) ?
        ToStr(dbc->conn_attr.at(KEY_REGION)) : "";
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(ToStr(dbc->conn_attr.at(KEY_SERVER)))
            : Aws::Region::US_EAST_1;
    }
    const std::string port = dbc->conn_attr.contains(KEY_PORT) ?
        ToStr(dbc->conn_attr.at(KEY_PORT)) : "";
    const std::string username = dbc->conn_attr.contains(KEY_DB_USERNAME) ?
        ToStr(dbc->conn_attr.at(KEY_DB_USERNAME)) : "";
    const std::chrono::milliseconds token_expiration = dbc->conn_attr.contains(KEY_TOKEN_EXPIRATION) ?
        std::chrono::milliseconds(std::strtol(ToStr(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION)).c_str(), nullptr, 0))
        : AuthProvider::DEFAULT_EXPIRATION_MS;
    const bool extra_url_encode = dbc->conn_attr.contains(KEY_EXTRA_URL_ENCODE) ?
        dbc->conn_attr.at(KEY_EXTRA_URL_ENCODE) == VALUE_BOOL_TRUE : false;

    // TODO - Proper error handling for missing parameters
    std::pair<std::string, bool> token = auth_provider->GetToken(iam_host, region, port, username, true, extra_url_encode, token_expiration);

    SQLRETURN ret = SQL_ERROR;

    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, ToRdsStr(token.first));
    ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    // Unsuccessful connection using cached token
    //  Skip cache and generate a new token to retry
    if (!SQL_SUCCEEDED(ret) && token.second) {
        // Update AWS Credentials
        const std::string saml_assertion = saml_util->GetSamlAssertion();
        const Aws::Auth::AWSCredentials credentials = saml_util->GetAwsCredentials(saml_assertion);
        auth_provider->UpdateAwsCredential(credentials);
        //  and retry without cache
        token = auth_provider->GetToken(iam_host, region, port, username, false, extra_url_encode, token_expiration);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, ToRdsStr(token.first));
        ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    return ret;
}

OktaSamlUtil::OktaSamlUtil(const std::map<RDS_STR, RDS_STR> &connection_attributes)
    : OktaSamlUtil(connection_attributes, nullptr, nullptr) {}

OktaSamlUtil::OktaSamlUtil(
    const std::map<RDS_STR, RDS_STR> &connection_attributes,
    const std::shared_ptr<Aws::Http::HttpClient> &http_client,
    const std::shared_ptr<Aws::STS::STSClient> &sts_client)
    : SamlUtil(connection_attributes, http_client, sts_client)
{
    const std::string app_id = connection_attributes.contains(KEY_APP_ID) ?
        ToStr(connection_attributes.at(KEY_APP_ID)) : "";
    sign_in_url = "https://" + idp_endpoint + ":" + idp_port + "/app/amazon_aws/" + app_id + "/sso/saml" + "?onetimetoken=";
    session_token_url = "https://" + idp_endpoint + ":" + idp_port + "/api/v1/authn";
}

std::string OktaSamlUtil::GetSamlAssertion()
{
    LOG(INFO) << "OKTA Sign In URL w/o Session Token: " << sign_in_url;
    const std::string session_token = GetSessionToken();
    if (session_token.empty()) {
        LOG(WARNING) << "No session token generated for SAML request";
        return "";
    }

    std::string url(sign_in_url);
    url += session_token;

    const std::shared_ptr<Aws::Http::HttpRequest> req = Aws::Http::CreateHttpRequest(
        url, Aws::Http::HttpMethod::HTTP_GET, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
    const std::shared_ptr<Aws::Http::HttpResponse> response = http_client->MakeRequest(req);

    std::string retval;
    // Check response code
    if (response->GetResponseCode() != Aws::Http::HttpResponseCode::OK) {
        LOG(WARNING) << "OKTA request returned bad HTTP response code: " << response->GetResponseCode();
        if (response->HasClientError()) {
            LOG(WARNING) << "Client error: " << response->GetClientErrorMessage();
        }
        return retval;
    }

    const std::istreambuf_iterator<char> eos;
    const std::string body(std::istreambuf_iterator<char>(response->GetResponseBody().rdbuf()), eos);

    if (std::smatch matches; std::regex_search(body, matches, std::regex(SAML_RESPONSE_PATTERN))) {
        return HtmlUtil::EscapeHtmlEntity(matches.str(1));
    }
    LOG(WARNING) << "No SAML response found in response";
    return "";
}

std::string OktaSamlUtil::GetSessionToken()
{
    // Send request for session token
    LOG(INFO) << "Got OKTA Session Token URL: " << session_token_url;
    const std::shared_ptr<Aws::Http::HttpRequest> req = Aws::Http::CreateHttpRequest(
        session_token_url, Aws::Http::HttpMethod::HTTP_POST, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
    Aws::Utils::Json::JsonValue json_body;
    json_body.WithString("username", idp_username)
        .WithString("password", idp_password);
    const Aws::String json_str = json_body.View().WriteReadable();
    const Aws::String json_len = Aws::Utils::StringUtils::to_string(json_str.size());
    req->SetContentType("application/json");
    req->AddContentBody(Aws::MakeShared<Aws::StringStream>("", json_str));
    req->SetContentLength(json_len);
    const std::shared_ptr<Aws::Http::HttpResponse> response = http_client->MakeRequest(req);

    // Check resp status
    if (response->GetResponseCode() != Aws::Http::HttpResponseCode::OK) {
        LOG(WARNING) << "OKTA request returned bad HTTP response code: " << response->GetResponseCode();
        if (response->HasClientError()) {
            LOG(WARNING) << "HTTP Client Error: " << response->GetClientErrorMessage();
        }
        return "";
    }

    // Get response session token
    const Aws::Utils::Json::JsonValue json_val(response->GetResponseBody());
    if (!json_val.WasParseSuccessful()) {
        LOG(WARNING) << "Unable to parse JSON from response";
        return "";
    }
    const Aws::Utils::Json::JsonView json_view = json_val.View();
    if (!json_view.KeyExists("sessionToken")) {
        LOG(WARNING) << "Could not find session token in JSON";
        return "";
    }
    return json_view.GetString("sessionToken");
}
