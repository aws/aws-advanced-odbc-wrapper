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

#include "adfs_auth_plugin.h"

#include <unordered_set>

#include "html_util.h"

#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/rds_utils.h"
#include "saml_util.h"

AdfsAuthPlugin::AdfsAuthPlugin(DBC *dbc) : AdfsAuthPlugin(dbc, nullptr) {}

AdfsAuthPlugin::AdfsAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : AdfsAuthPlugin(dbc, next_plugin, nullptr, nullptr) {}

AdfsAuthPlugin::AdfsAuthPlugin(DBC *dbc, BasePlugin *next_plugin, const std::shared_ptr<SamlUtil> &saml_util, const std::shared_ptr<AuthProvider> &auth_provider) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "ADFS";

    if (saml_util) {
        this->saml_util = saml_util;
    } else {
        this->saml_util = std::make_shared<AdfsSamlUtil>(dbc->conn_attr);
    }

    if (auth_provider) {
        this->auth_provider = auth_provider;
    } else {
        std::string region = dbc->conn_attr.contains(KEY_REGION) ?
            dbc->conn_attr.at(KEY_REGION) : "";
        if (region.empty()) {
            region = dbc->conn_attr.contains(KEY_SERVER) ?
                RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
                : Aws::Region::US_EAST_1;
        }
        const std::string saml_assertion = this->saml_util->GetSamlAssertion();
        this->auth_provider = std::make_shared<AuthProvider>(region, this->saml_util->GetAwsCredentials(saml_assertion));
    }
}

AdfsAuthPlugin::~AdfsAuthPlugin()
{
    if (auth_provider) {
        auth_provider.reset();
    }
    if (saml_util) {
        saml_util.reset();
    }
}

SQLRETURN AdfsAuthPlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering Connect";
    DBC* dbc = static_cast<DBC*>(ConnectionHandle);

    const std::string server = dbc->conn_attr.contains(KEY_SERVER) ?
        dbc->conn_attr.at(KEY_SERVER) : "";
    const std::string iam_host = dbc->conn_attr.contains(KEY_IAM_HOST) ?
        dbc->conn_attr.at(KEY_IAM_HOST) : server;
    std::string region = dbc->conn_attr.contains(KEY_REGION) ?
        dbc->conn_attr.at(KEY_REGION) : "";
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : Aws::Region::US_EAST_1;
    }
    std::string port = dbc->conn_attr.contains(KEY_IAM_PORT) ?
        dbc->conn_attr.at(KEY_IAM_PORT) : "";
    if (port.empty()) {
        port = dbc->conn_attr.contains(KEY_PORT) ?
        dbc->conn_attr.at(KEY_PORT) : "";
    }
    const std::string username = dbc->conn_attr.contains(KEY_DB_USERNAME) ?
        dbc->conn_attr.at(KEY_DB_USERNAME) : "";
    const std::chrono::milliseconds token_expiration = dbc->conn_attr.contains(KEY_TOKEN_EXPIRATION) ?
        std::chrono::seconds(std::strtol(dbc->conn_attr.at(KEY_TOKEN_EXPIRATION).c_str(), nullptr, 0)) : AuthProvider::DEFAULT_EXPIRATION_MS;
    const bool extra_url_encode = dbc->conn_attr.contains(KEY_EXTRA_URL_ENCODE) ?
        dbc->conn_attr.at(KEY_EXTRA_URL_ENCODE) == VALUE_BOOL_TRUE : false;

    if (iam_host.empty() || region.empty() || port.empty() || username.empty()) {
        LOG(ERROR) << "Missing required parameters for ADFS Authentication";
        CLEAR_DBC_ERROR(dbc);
        dbc->err = new ERR_INFO("Missing required parameters for ADFS Authentication", ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION);
        return SQL_ERROR;
    }
    std::pair<std::string, bool> token = auth_provider->GetToken(iam_host, region, port, username, true, extra_url_encode, token_expiration);

    SQLRETURN ret = SQL_ERROR;

    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
    ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    // Unsuccessful connection using cached token
    // Skip cache and generate a new token to retry
    if (!SQL_SUCCEEDED(ret) && token.second) {
        LOG(WARNING) << "Cached token failed to connect. Retrying with fresh token";
        // Update AWS Credentials
        const std::string saml_assertion = saml_util->GetSamlAssertion();
        const Aws::Auth::AWSCredentials credentials = saml_util->GetAwsCredentials(saml_assertion);
        auth_provider->UpdateAwsCredential(credentials);
        // and retry without cache
        token = auth_provider->GetToken(iam_host, region, port, username, false, extra_url_encode, token_expiration);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, token.first);
        ret = next_plugin->Connect(ConnectionHandle, WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    return ret;
}

AdfsSamlUtil::AdfsSamlUtil(const std::map<std::string, std::string> &connection_attributes)
    : AdfsSamlUtil(connection_attributes, nullptr, nullptr) {}

AdfsSamlUtil::AdfsSamlUtil(
    const std::map<std::string, std::string> &connection_attributes,
    const std::shared_ptr<Aws::Http::HttpClient> &http_client,
    const std::shared_ptr<Aws::STS::STSClient> &sts_client)
    : SamlUtil(connection_attributes, http_client, sts_client)
{
    std::string relaying_party_id = connection_attributes.contains(KEY_RELAY_PARTY_ID) ?
        connection_attributes.at(KEY_RELAY_PARTY_ID) : "";
    if (relaying_party_id.empty()) {
        LOG(INFO) << "Relaying party ID not supplied, using default: " << DEFAULT_RELAY_ID;
        relaying_party_id = DEFAULT_RELAY_ID;
    }

    sign_in_url = "https://" + idp_endpoint + ":" + idp_port + "/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=" + relaying_party_id;
}

std::string AdfsSamlUtil::GetSamlAssertion()
{
    std::string url = sign_in_url;
    const std::shared_ptr<Aws::Http::HttpRequest> http_request = Aws::Http::CreateHttpRequest(url, Aws::Http::HttpMethod::HTTP_GET, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
    const std::shared_ptr<Aws::Http::HttpResponse> http_response = http_client->MakeRequest(http_request);

    std::string retval;
    if (Aws::Http::HttpResponseCode::OK != http_response->GetResponseCode()) {
        LOG(ERROR) << "ADFS request returned bad HTTP response code: " << http_response->GetResponseCode();
        return retval;
    }

    const std::istreambuf_iterator<char> eos;
    std::string body(std::istreambuf_iterator<char>(http_response->GetResponseBody().rdbuf()), eos);

    // Retrieve SAMLResponse value
    std::smatch matches;
    std::string action;
    if (std::regex_search(body, matches, std::regex(FORM_ACTION_PATTERN))) {
        action = HtmlUtil::EscapeHtmlEntity(matches.str(1));
    } else {
        LOG(ERROR) << "Could not extract login action from the response body";
        return retval;
    }

    if (!action.empty() && action[0]=='/') {
        url = "https://";
        url += idp_endpoint;
        url += ":";
        url += idp_port;
        url += action;
    }
    DLOG(INFO) << "Updated URL [" << url << "] using Action [" << action << "]";

    const std::map<std::string, std::string> sign_in_parameters = GetParameterFromBody(body);

    if (const std::string sign_in_content = GetFormActionBody(url, sign_in_parameters);
        std::regex_search(sign_in_content, matches, std::regex(SAML_RESPONSE_PATTERN))) {
        return matches.str(1);
    }
    LOG(ERROR) << "Failed SAML Assertion";
    return retval;
}

std::map<std::string, std::string> AdfsSamlUtil::GetParameterFromBody(std::string &body)
{
    std::map<std::string, std::string> parameters;
    for (auto& input_tag : GetInputTagsFromBody(body)) {
        const std::string name = GetValueByKey(input_tag, std::string("name"));
        const std::string value = GetValueByKey(input_tag, std::string("value"));
        DLOG(INFO) << "Input Tag [" << input_tag << "], Name [" << name << "], Value [" << value << "]";
        std::string name_lower(name);
        std::ranges::transform(name_lower, name_lower.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (name_lower.find("username") != std::string::npos) {
            parameters.insert(std::pair<std::string, std::string>(name, std::string(idp_username)));
        } else if (name_lower.find("authmethod") != std::string::npos) {
            if (!value.empty()) {
                parameters.insert(std::pair<std::string, std::string>(name, value));
            }
        } else if (name_lower.find("password") != std::string::npos) {
            parameters.insert(std::pair<std::string, std::string>(name, std::string(idp_password)));
        } else if (!name.empty()) {
            parameters.insert(std::pair<std::string, std::string>(name, value));
        }
    }

    DLOG(INFO) << "parameters size: " << parameters.size();
    for (auto&[key, value] : parameters) {
        DLOG(INFO) << "Parameter Key [" << key << "], Value Size [" << value.size() << "]";
    }

    return parameters;
}

std::string AdfsSamlUtil::GetFormActionBody(const std::string &url, const std::map<std::string, std::string> &params)
{
    if (!ValidateUrl(url)) {
        return "";
    }

    const std::shared_ptr<Aws::Http::HttpRequest> req = Aws::Http::CreateHttpRequest(url, Aws::Http::HttpMethod::HTTP_POST, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);

    // Set content
    std::string body;
    for (const auto&[key, value] : params) {
        body += key;
        body += "=";
        body += value;
        body += "&";
    }
    if (!body.empty()) {
        body.pop_back();
    }

    const std::shared_ptr<Aws::StringStream> string_stream = std::make_shared<Aws::StringStream>();
    *string_stream << body;

    req->AddContentBody(string_stream);
    req->SetContentLength(std::to_string(body.size()));

    // Check response code
    const std::shared_ptr<Aws::Http::HttpResponse> response = http_client->MakeRequest(req);
    if (response->GetResponseCode() != Aws::Http::HttpResponseCode::OK) {
        LOG(WARNING) << "ADFS request returned bad HTTP response code: " << response->GetResponseCode();
        if (response->HasClientError()) {
            LOG(WARNING) << "HTTP Client Error: " << response->GetClientErrorMessage();
        }
        return "";
    }

    const std::istreambuf_iterator<char> eos;
    std::string resp_body(std::istreambuf_iterator<char>(response->GetResponseBody().rdbuf()), eos);
    return resp_body;
}

bool AdfsSamlUtil::ValidateUrl(const std::string &url)
{
    const std::regex pattern(URL_PATTERN);

    if (!regex_match(url, pattern)) {
        LOG(ERROR) << "Invalid URL, failed to match ADFS URL pattern";
        return false;
    }
    return true;
}

std::vector<std::string> AdfsSamlUtil::GetInputTagsFromBody(const std::string &body)
{
    std::unordered_set<std::string> hash_set;
    std::vector<std::string> retval;

    std::smatch matches;
    const std::regex pattern(INPUT_TAG_PATTERN);
    std::string source = body;
    while (std::regex_search(source,matches, pattern)) {
        const std::string tag = matches.str(0);
        std::string tag_name = GetValueByKey(tag, std::string("name"));
        DLOG(INFO) << "Tag [" << tag << "], Tag Name [" << tag_name << "]";
        std::ranges::transform(tag_name, tag_name.begin(), [](const unsigned char c) {
            return std::tolower(c);
        });
        if (!tag_name.empty() && !hash_set.contains(tag_name)) {
            hash_set.insert(tag_name);
            retval.push_back(tag);
            DLOG(INFO) << "Saved input_tag: " << tag;
        }

        source = matches.suffix().str();
    }

    DLOG(INFO) << "Input tags vector size: " << retval.size();
    return retval;
}

std::string AdfsSamlUtil::GetValueByKey(const std::string &input, const std::string &key)
{
    std::string pattern("(");
    pattern += key;
    pattern += ")\\s*=\\s*\"(.*?)\"";

    if (std::smatch matches; std::regex_search(input, matches, std::regex(pattern))) {
        return HtmlUtil::EscapeHtmlEntity(matches.str(2));
    }
    return "";
}
