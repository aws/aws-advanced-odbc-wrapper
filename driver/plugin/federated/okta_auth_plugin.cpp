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

#if (defined(_WIN32) || defined(_WIN64))
    #include <windows.h>
    #include <shellapi.h>
    #undef GetObject
#endif

#include "okta_auth_plugin.h"

#include "html_util.h"
#include "http/WEBServer.h"
#include "http/WEBServer_utils.h"
#include "saml_util.h"

#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/map_utils.h"
#include "../../util/rds_strings.h"
#include "../../util/rds_utils.h"

OktaAuthPlugin::OktaAuthPlugin(DBC *dbc) : OktaAuthPlugin(dbc, nullptr) {}

OktaAuthPlugin::OktaAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin) : OktaAuthPlugin(dbc, next_plugin, nullptr, nullptr) {}

namespace {
    std::shared_ptr<SamlUtil> CreateOktaSamlUtil(
        DBC* dbc,
        const std::shared_ptr<SamlUtil>& saml_util)
    {
        if (saml_util) {
            return saml_util;
        }
        return std::make_shared<OktaSamlUtil>(dbc->conn_attr);
    }

    std::shared_ptr<AuthProvider> CreateOktaAuthProvider(
        DBC* dbc,
        const std::shared_ptr<SamlUtil>& resolved_saml,
        const std::shared_ptr<AuthProvider>& auth_provider)
    {
        if (auth_provider) {
            return auth_provider;
        }
        std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
        if (region.empty()) {
            region = dbc->conn_attr.contains(KEY_SERVER) ?
                RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
                : Aws::Region::US_EAST_1;
        }
        const std::string assertion = resolved_saml->GetSamlAssertion();
        return std::make_shared<AuthProvider>(region, resolved_saml->GetAwsCredentials(assertion));
    }
}  // namespace

OktaAuthPlugin::OktaAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin, const std::shared_ptr<SamlUtil> &saml_util, const std::shared_ptr<AuthProvider> &auth_provider)
    : OktaAuthPlugin(dbc, next_plugin, saml_util, auth_provider, nullptr, nullptr) {}

OktaAuthPlugin::OktaAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin,
                               const std::shared_ptr<SamlUtil> &saml_util, const std::shared_ptr<AuthProvider> &auth_provider,
                               std::shared_ptr<Dialect> dialect, std::shared_ptr<OdbcHelper> odbc_helper)
    : BaseSamlAuthPlugin(dbc, next_plugin,
        CreateOktaSamlUtil(dbc, saml_util),
        CreateOktaAuthProvider(dbc, CreateOktaSamlUtil(dbc, saml_util), auth_provider),
        dialect, odbc_helper)
{
    this->plugin_name = "OKTA";
}

OktaSamlUtil::OktaSamlUtil(const std::map<std::string, std::string> &connection_attributes)
    : OktaSamlUtil(connection_attributes, nullptr, nullptr) {}

OktaSamlUtil::OktaSamlUtil(
    const std::map<std::string, std::string> &connection_attributes,
    const std::shared_ptr<Aws::Http::HttpClient> &http_client,
    const std::shared_ptr<Aws::STS::STSClient> &sts_client)
    : SamlUtil(connection_attributes, http_client, sts_client)
{
    const std::string app_id = MapUtils::GetStringValue(connection_attributes, KEY_APP_ID, "");
    if (app_id.empty()) {
        throw std::runtime_error("Missing required parameters for Okta Authentication");
    }
    sign_in_url = "https://" + idp_endpoint + ":" + idp_port + "/app/amazon_aws/" + app_id + "/sso/saml" + "?onetimetoken=";
    session_token_url = "https://" + idp_endpoint + ":" + idp_port + "/api/v1/authn";

    const std::string mfa_type_str = MapUtils::GetStringValue(connection_attributes, KEY_MFA_TYPE, "");
    if (mfa_type_table.contains(mfa_type_str)) {
        mfa_type = mfa_type_table.at(mfa_type_str);
    }
    mfa_port = MapUtils::GetStringValue(connection_attributes, KEY_MFA_PORT, DEFAULT_PORT);
    mfa_timeout = MapUtils::GetStringValue(connection_attributes, KEY_MFA_TIMEOUT, DEFAULT_MFA_TIMEOUT);
}

std::string OktaSamlUtil::GetSamlAssertion()
{
    LOG(INFO) << "OKTA Sign In URL w/o Session Token: " << sign_in_url;
    const std::string session_token = GetSessionToken();
    if (session_token.empty()) {
        LOG(ERROR) << "No session token generated for SAML request";
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
        LOG(ERROR) << "OKTA request returned bad HTTP response code: " << response->GetResponseCode();
        if (response->HasClientError()) {
            LOG(ERROR) << "Client error: " << response->GetClientErrorMessage();
        }
        return retval;
    }

    const std::istreambuf_iterator<char> eos;
    const std::string body(std::istreambuf_iterator<char>(response->GetResponseBody().rdbuf()), eos);

    if (std::smatch matches; std::regex_search(body, matches, std::regex(SAML_RESPONSE_PATTERN))) {
        return HtmlUtil::EscapeHtmlEntity(matches.str(1));
    }
    LOG(ERROR) << "No SAML response found in response";
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
        LOG(ERROR) << "OKTA request returned bad HTTP response code: " << response->GetResponseCode();
        if (response->HasClientError()) {
            LOG(ERROR) << "HTTP Client Error: " << response->GetClientErrorMessage();
        }
        return "";
    }

    // Get response session token
    const Aws::Utils::Json::JsonValue json_val(response->GetResponseBody());
    if (!json_val.WasParseSuccessful()) {
        LOG(ERROR) << "Unable to parse JSON from response";
        return "";
    }

    if (mfa_type != NONE) {
        const Aws::Utils::Json::JsonView json_view = json_val.View();

        if (!json_view.KeyExists("stateToken")) {
            LOG(ERROR) << "Could not find state token in JSON";
            return "";
        }

        const std::string state_token = json_view.GetString("stateToken");
        const Aws::Utils::Json::JsonView embedded_view = json_view.GetObject("_embedded");
        Aws::Utils::Array<Aws::Utils::Json::JsonView> factor_views = embedded_view.GetArray("factors");

        std::string factor_id;
        for (int i = 0; i < factor_views.GetLength(); i++) {
            const std::string type = factor_views[i].GetString("factorType");
            if (mfa_type == TOTP && type == "token:software:totp" || mfa_type == PUSH && type == "push") {
                factor_id = factor_views[i].GetString("id");
            }
        }

        if (factor_id.empty()) {
            LOG(ERROR) << "Could not find factor in JSON";
            return "";
        }

        const std::string verify_url = session_token_url + "/factors/" + factor_id + "/verify";
        if (mfa_type == TOTP) {
            return VerifyTOTPChallenge(verify_url, state_token);
        }
        if (mfa_type == PUSH) {
            return VerifyPushChallenge(verify_url, state_token);
        }
    }

    const Aws::Utils::Json::JsonView json_view = json_val.View();
    if (!json_view.KeyExists("sessionToken")) {
        LOG(ERROR) << "Could not find session token in JSON";
        return "";
    }

    return json_view.GetString("sessionToken");
}

std::string OktaSamlUtil::VerifyTOTPChallenge(
    const std::string &verify_url,
    const std::string &state_token)
{
    std::string state = WebServerUtils::GenerateState();
    WEBServer srv(state, mfa_port, mfa_timeout);

    srv.LaunchServer();

    const std::string mfa_form_url =  WEBSERVER_HOST + ":" + mfa_port;
    try {
#if (defined(_WIN32) || defined(_WIN64))
        const HINSTANCE result = ShellExecute(NULL, RDS_TSTR(std::string("open")).c_str(), RDS_TSTR(mfa_form_url).c_str(), NULL, NULL, SW_SHOWNORMAL);
        if (reinterpret_cast<intptr_t>(result) <= 32) {
            srv.Cancel();
        }
#else
#if (defined(LINUX) || defined(__linux__))
        const int result = system(("xdg-open " + mfa_form_url).c_str()); // NOLINT(bugprone-command-processor)
#else
        const int result = system(("open " + mfa_form_url).c_str()); // NOLINT(bugprone-command-processor)
#endif
        if (result != 0) {
            srv.Cancel();
        }
#endif
    } catch (const std::exception & e) {
        srv.Cancel();
        srv.Join();
        LOG(ERROR) << "Could not open browser to obtain MFA token: " << e.what();
        return "";
    }

    srv.Join();

    const std::string pass_code = srv.GetCode();
    if (pass_code.empty()) {
        LOG(ERROR) << "MFA Authorization code was not obtained";
        return "";
    }

    const std::shared_ptr<Aws::Http::HttpRequest> req = Aws::Http::CreateHttpRequest(
        verify_url, Aws::Http::HttpMethod::HTTP_POST, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
    Aws::Utils::Json::JsonValue json_body;
    json_body
        .WithString("stateToken", state_token)
        .WithString("passCode", pass_code);
    const Aws::String json_str = json_body.View().WriteReadable();
    const Aws::String json_len = Aws::Utils::StringUtils::to_string(json_str.size());
    req->SetContentType("application/json");
    req->AddContentBody(Aws::MakeShared<Aws::StringStream>("", json_str));
    req->SetContentLength(json_len);
    const std::shared_ptr<Aws::Http::HttpResponse> response = http_client->MakeRequest(req);

    // Check resp status
    if (response->GetResponseCode() != Aws::Http::HttpResponseCode::OK) {
        LOG(ERROR) << "OKTA request returned bad HTTP response code: " << response->GetResponseCode();
        if (response->HasClientError()) {
            LOG(ERROR) << "HTTP Client Error: " << response->GetClientErrorMessage();
        }
        return "";
    }

    const Aws::Utils::Json::JsonValue json_val(response->GetResponseBody());
    if (!json_val.WasParseSuccessful()) {
        LOG(ERROR) << "Unable to parse JSON from response";
        return "";
    }

    const Aws::Utils::Json::JsonView json_view = json_val.View();
    if (!json_view.KeyExists("sessionToken")) {
        LOG(ERROR) << "Could not find session token in JSON";
        return "";
    }

    return json_view.GetString("sessionToken");
}

std::string OktaSamlUtil::VerifyPushChallenge(
    const std::string &verify_url,
    const std::string &state_token)
{
    const std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now() + std::chrono::seconds(std::strtol(mfa_timeout.c_str(), nullptr, 0));
    while (std::chrono::steady_clock::now() < end_time) {
        const std::shared_ptr<Aws::Http::HttpRequest> req = Aws::Http::CreateHttpRequest(
            verify_url, Aws::Http::HttpMethod::HTTP_POST, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
        Aws::Utils::Json::JsonValue json_body;
        json_body.WithString("stateToken", state_token);
        const Aws::String json_str = json_body.View().WriteReadable();
        const Aws::String json_len = Aws::Utils::StringUtils::to_string(json_str.size());
        req->SetContentType("application/json");
        req->AddContentBody(Aws::MakeShared<Aws::StringStream>("", json_str));
        req->SetContentLength(json_len);
        const std::shared_ptr<Aws::Http::HttpResponse> response = http_client->MakeRequest(req);

        // Check resp status
        if (response->GetResponseCode() != Aws::Http::HttpResponseCode::OK) {
            LOG(ERROR) << "OKTA request returned bad HTTP response code: " << response->GetResponseCode();
            if (response->HasClientError()) {
                LOG(ERROR) << "HTTP Client Error: " << response->GetClientErrorMessage();
            }
        } else {
            const Aws::Utils::Json::JsonValue json_val(response->GetResponseBody());
            if (!json_val.WasParseSuccessful()) {
                LOG(ERROR) << "Unable to parse JSON from response";
            } else {
                const Aws::Utils::Json::JsonView json_view = json_val.View();
                if (json_view.KeyExists("sessionToken")) {
                    return json_view.GetString("sessionToken");
                }
                LOG(ERROR) << "Could not find session token in JSON";
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(VERIFY_PUSH_INTERVAL));
    }
    LOG(ERROR) << "The MFA challenge was not completed in time";
    return "";
}
