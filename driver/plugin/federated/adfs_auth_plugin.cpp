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
#include "saml_util.h"

AdfsAuthPlugin::AdfsAuthPlugin(DBC *dbc) : AdfsAuthPlugin(dbc, nullptr) {}

AdfsAuthPlugin::AdfsAuthPlugin(DBC *dbc, BasePlugin *next_plugin) : BasePlugin(dbc, next_plugin)
{
    this->plugin_name = "ADFS";

    auto map_end_itr = dbc->conn_attr.end();
    std::string region = dbc->conn_attr.find(KEY_REGION) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_REGION)) : "";
    saml_util = std::make_shared<AdfsSamlUtil>(dbc->conn_attr);
    std::string saml_assertion = saml_util->GetSamlAssertion();
    auth_provider = std::make_shared<AuthProvider>(region, saml_util->GetAwsCredentials(saml_assertion));
}

AdfsAuthPlugin::~AdfsAuthPlugin()
{
    if (auth_provider) auth_provider.reset();
    if (saml_util) saml_util.reset();
}

SQLRETURN AdfsAuthPlugin::Connect(
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    auto map_end_itr = dbc->conn_attr.end();
    std::string server = dbc->conn_attr.find(KEY_SERVER) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_SERVER)) : "";
    // TODO - Helper to parse from URL
    std::string region = dbc->conn_attr.find(KEY_REGION) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_REGION)) : "";
    std::string port = dbc->conn_attr.find(KEY_PORT) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_PORT)) : "";
    std::string username = dbc->conn_attr.find(KEY_DB_USERNAME) != map_end_itr ?
        ToStr(dbc->conn_attr.at(KEY_DB_USERNAME)) : "";

    // TODO - Proper error handling for missing parameters

    // TODO - Custom expiration time
    std::pair<std::string, bool> token = auth_provider->GetToken(server, region, port, username, true);

    SQLRETURN ret = SQL_ERROR;

    dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, ToRdsStr(token.first));
    ret = next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);

    // Unsuccessful connection using cached token
    //  Skip cache and generate a new token to retry
    if (!SQL_SUCCEEDED(ret) && token.second) {
        // Update AWS Credentials
        std::string saml_assertion = saml_util->GetSamlAssertion();
        Aws::Auth::AWSCredentials credentials = saml_util->GetAwsCredentials(saml_assertion);
        auth_provider->UpdateAwsCredential(credentials);
        //  and retry without cache
        token = auth_provider->GetToken(server, region, port, username, false);
        dbc->conn_attr.insert_or_assign(KEY_DB_PASSWORD, ToRdsStr(token.first));
        ret = next_plugin->Connect(WindowHandle, OutConnectionString, BufferLength, StringLengthPtr, DriverCompletion);
    }

    return ret;
}

const std::string AdfsSamlUtil::FORM_ACTION_PATTERN = "<form.*?action=\"([^\"]+)\"";
const std::string AdfsSamlUtil::INPUT_TAG_PATTERN = "<input id=(.*)";
const std::string AdfsSamlUtil::SAML_RESPONSE_PATTERN = "name=\"SAMLResponse\"\\s+value=\"([^\"]+)\"";
const std::string AdfsSamlUtil::URL_PATTERN = "^(https)://[-a-zA-Z0-9+&@#/%?=~_!:,.']*[-a-zA-Z0-9+&@#/%=~_']";

AdfsSamlUtil::AdfsSamlUtil(std::map<RDS_STR, RDS_STR> connection_attributes) : SamlUtil(connection_attributes) {
    std::string relaying_party_id = connection_attributes.find(KEY_RELAY_PARTY_ID) != connection_attributes.end() ?
        ToStr(connection_attributes.at(KEY_RELAY_PARTY_ID)) : "";
    sign_in_url = "https://" + idp_endpoint + ":" + idp_port + "/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=" + relaying_party_id;
}

std::string AdfsSamlUtil::GetSamlAssertion()
{
    std::string url = sign_in_url;
    std::shared_ptr<Aws::Http::HttpRequest> http_request = Aws::Http::CreateHttpRequest(url, Aws::Http::HttpMethod::HTTP_GET, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
    std::shared_ptr<Aws::Http::HttpResponse> http_response = http_client->MakeRequest(http_request);

    std::string retval;
    if (Aws::Http::HttpResponseCode::OK != http_response->GetResponseCode()) {
        LOG(WARNING) << "ADFS request returned bad HTTP response code: " << http_response->GetResponseCode();
        return retval;
    }

    std::istreambuf_iterator<char> eos;
    std::string body(std::istreambuf_iterator<char>(http_response->GetResponseBody().rdbuf()), eos);

    // Retrieve SAMLResponse value
    std::smatch matches;
    std::string action;
    if (std::regex_search(body, matches, std::regex(FORM_ACTION_PATTERN))) {
        action = HtmlUtil::EscapeHtmlEntity(matches.str(1));
    } else {
        LOG(WARNING) << "Could not extract login action from the response body";
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

    std::map<std::string, std::string> sign_in_parameters = GetParameterFromBody(body);
    std::string sign_in_content = GetFormActionBody(url, sign_in_parameters);

    if (std::regex_search(sign_in_content, matches, std::regex(SAML_RESPONSE_PATTERN))) {
        DLOG(INFO) << "SAML Response: " << matches.str(1);
        return matches.str(1);
    }
    LOG(WARNING) << "Failed SAML Asesertion";
    return retval;
}

std::map<std::string, std::string> AdfsSamlUtil::GetParameterFromBody(std::string &body)
{
    std::map<std::string, std::string> parameters;
    for (auto& input_tag : GetInputTagsFromBody(body)) {
        std::string name = GetValueByKey(input_tag, std::string("name"));
        std::string value = GetValueByKey(input_tag, std::string("value"));
        DLOG(INFO) << "Input Tag [" << input_tag << "], Name [" << name << "], Value [" << value << "]";
        std::string name_lower(name);
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), [](unsigned char c) {
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
    for (auto& itr : parameters) {
        DLOG(INFO) << "Parameter Key [" << itr.first << "], Value Size [" << itr.second.size() << "]";
    }

    return parameters;
}

std::string AdfsSamlUtil::GetFormActionBody(std::string &url, std::map<std::string, std::string> &params)
{
    if (!ValidateUrl(url)) {
        return "";
    }

    std::shared_ptr<Aws::Http::HttpRequest> req = Aws::Http::CreateHttpRequest(url, Aws::Http::HttpMethod::HTTP_POST, Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);

    // Set content
    std::string body;
    for (auto& itr : params) {
        body += itr.first + "=" + itr.second;
        body += "&";
    }
    body.pop_back();

    std::shared_ptr<Aws::StringStream> string_stream = std::make_shared<Aws::StringStream>();
    *string_stream << body;

    req->AddContentBody(string_stream);
    req->SetContentLength(std::to_string(body.size()));

    // Check response code
    std::shared_ptr<Aws::Http::HttpResponse> response = http_client->MakeRequest(req);
    if (response->GetResponseCode() != Aws::Http::HttpResponseCode::OK) {
        LOG(WARNING) << "ADFS request returned bad HTTP response code: " << response->GetResponseCode();
        if (response->HasClientError()) {
            LOG(WARNING) << "HTTP Client Error: " << response->GetClientErrorMessage();
        }
        return "";
    }

    std::istreambuf_iterator<char> eos;
    std::string resp_body(std::istreambuf_iterator<char>(response->GetResponseBody().rdbuf()), eos);
    return resp_body;
}

bool AdfsSamlUtil::ValidateUrl(const std::string &url)
{
    std::regex pattern(URL_PATTERN);

    if (!regex_match(url, pattern)) {
        LOG(WARNING) << "Invalid URL, failed to match ADFS URL pattern";
        return false;
    }
    return true;
}

std::vector<std::string> AdfsSamlUtil::GetInputTagsFromBody(const std::string &body)
{
    std::unordered_set<std::string> hash_set;
    std::vector<std::string> retval;

    std::smatch matches;
    std::regex pattern(INPUT_TAG_PATTERN);
    std::string source = body;
    while (std::regex_search(source,matches,pattern)) {
        std::string tag = matches.str(0);
        std::string tag_name = GetValueByKey(tag, std::string("name"));
        DLOG(INFO) << "Tag [" << tag << "], Tag Name [" << tag_name << "]";
        std::transform(tag_name.begin(), tag_name.end(), tag_name.begin(), [](unsigned char c) {
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

    std::smatch matches;
    if (std::regex_search(input, matches, std::regex(pattern))) {
        return HtmlUtil::EscapeHtmlEntity(matches.str(2));
    }
    return "";
}
