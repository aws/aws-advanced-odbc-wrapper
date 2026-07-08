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

#include "saml_util.h"

#include <aws/sts/model/AssumeRoleWithSAMLRequest.h>

#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/map_utils.h"
#include "../../util/rds_strings.h"
#include "../../util/rds_utils.h"

SamlUtil::SamlUtil(std::map<std::string, std::string> connection_attributes)
    : SamlUtil(std::move(connection_attributes), nullptr, nullptr) {}

SamlUtil::SamlUtil(
    std::map<std::string, std::string> connection_attributes,
    const std::shared_ptr<Aws::Http::HttpClient>& http_client,
    const std::shared_ptr<Aws::STS::STSClient>& sts_client)
{
    ParseIdpConfig(connection_attributes);
    AwsSdkHelper::EnsureInitialized();
    if (http_client) {
        this->http_client = http_client;
    } else {
        Aws::Client::ClientConfiguration http_client_config;
        if (connection_attributes.contains(KEY_HTTP_SOCKET_TIMEOUT)) {
            const std::string socket_timeout_str = connection_attributes.at(KEY_HTTP_SOCKET_TIMEOUT);
            const int64_t socket_timeout = std::strtol(socket_timeout_str.c_str(), nullptr, 0);
            http_client_config.requestTimeoutMs = socket_timeout > 0 ? socket_timeout : DEFAULT_SOCKET_TIMEOUT_MS;
        }
        if (connection_attributes.contains(KEY_HTTP_CONNECT_TIMEOUT)) {
            const std::string connect_timeout_str = connection_attributes.at(KEY_HTTP_CONNECT_TIMEOUT);
            const int64_t connect_timeout = std::strtol(connect_timeout_str.c_str(), nullptr, 0);
            http_client_config.connectTimeoutMs = connect_timeout > 0 ? connect_timeout : DEFAULT_CONNECT_TIMEOUT_MS;
        }
        http_client_config.followRedirects = Aws::Client::FollowRedirectsPolicy::ALWAYS;
        this->http_client = Aws::Http::CreateHttpClient(http_client_config);
    }

    if (sts_client) {
        this->sts_client = sts_client;
    } else {
        std::string region = MapUtils::GetStringValue(connection_attributes, KEY_REGION, "");
        if (region.empty()) {
            region = connection_attributes.contains(KEY_SERVER) ?
                RdsUtils::GetRdsRegion(connection_attributes.at(KEY_SERVER))
                : Aws::Region::US_EAST_1;
        }
        Aws::STS::STSClientConfiguration sts_client_config;
        sts_client_config.region = region;
        // STS_ENDPOINT overrides the resolved endpoint for non-commercial partitions (e.g. GovCloud: https://sts.us-gov-west-1.amazonaws.com)
        // where the SDK does not reliably resolve the regional STS endpoint from region alone.
        const std::string sts_endpoint = MapUtils::GetStringValue(connection_attributes, KEY_STS_ENDPOINT, "");
        if (!sts_endpoint.empty()) {
            sts_client_config.endpointOverride = sts_endpoint;
        }
        this->sts_client = std::make_shared<Aws::STS::STSClient>(sts_client_config);
    }
}

SamlUtil::~SamlUtil()
{
    if (http_client) {
        http_client = nullptr;
    }
    if (sts_client) {
        sts_client = nullptr;
    }
}

Aws::Auth::AWSCredentials SamlUtil::GetAwsCredentials(const std::string &assertion)
{
    Aws::STS::Model::AssumeRoleWithSAMLRequest sts_req;
    sts_req.SetRoleArn(idp_role_arn);
    sts_req.SetPrincipalArn(idp_saml_arn);
    sts_req.SetSAMLAssertion(assertion);

    const Aws::Utils::Outcome<Aws::STS::Model::AssumeRoleWithSAMLResult, Aws::STS::STSError> outcome =
        sts_client->AssumeRoleWithSAML(sts_req);

    if (!outcome.IsSuccess()) {
        LOG(ERROR) << "STS failed to assume role: " << outcome.GetError().GetMessage();
        LOG(ERROR) << "STS error type: " << static_cast<int>(outcome.GetError().GetErrorType());
        return {};
    }

    LOG(INFO) << "STS AssumeRoleWithSAML succeeded";
    const Aws::STS::Model::AssumeRoleWithSAMLResult &result = outcome.GetResult();
    const Aws::STS::Model::Credentials &temp_credentials = result.GetCredentials();
    const Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(
        temp_credentials.GetAccessKeyId(), temp_credentials.GetSecretAccessKey(), temp_credentials.GetSessionToken());
    return credentials;
}

Aws::Auth::AWSCredentials SamlUtil::GetCredentials()
{
    const std::lock_guard<std::mutex> lock(cred_cache_mutex_);

    const auto now = std::chrono::system_clock::now();
    const auto it = cred_cache_.find(idp_role_arn);
    if (it != cred_cache_.end() && (now - it->second.fetched_at) < CRED_CACHE_TTL
        && !it->second.creds.IsExpiredOrEmpty()) {
        LOG(INFO) << "Reusing cached SAML credentials for role " << idp_role_arn;
        return it->second.creds;
    }

    // Cache miss: run the interactive assertion exchange exactly once, then cache.
    const std::string assertion = GetSamlAssertion();
    Aws::Auth::AWSCredentials creds = GetAwsCredentials(assertion);
    if (!creds.IsEmpty()) {
        cred_cache_[idp_role_arn] = {creds, now};
    }
    return creds;
}

void SamlUtil::InvalidateCachedCredentials()
{
    const std::lock_guard<std::mutex> lock(cred_cache_mutex_);
    cred_cache_.erase(idp_role_arn);
}

void SamlUtil::ClearCredentialsCache()
{
    const std::lock_guard<std::mutex> lock(cred_cache_mutex_);
    cred_cache_.clear();
}

void SamlUtil::ParseIdpConfig(const std::map<std::string, std::string> &connection_attributes)
{
    idp_endpoint = MapUtils::GetStringValue(connection_attributes, KEY_IDP_ENDPOINT, "");
    idp_port = MapUtils::GetStringValue(connection_attributes, KEY_IDP_PORT, "443");
    idp_username = MapUtils::GetStringValue(connection_attributes, KEY_IDP_USERNAME, "");
    idp_password = MapUtils::GetStringValue(connection_attributes, KEY_IDP_PASSWORD, "");
    idp_role_arn = MapUtils::GetStringValue(connection_attributes, KEY_IDP_ROLE_ARN, "");
    idp_saml_arn = MapUtils::GetStringValue(connection_attributes, KEY_IDP_SAML_ARN, "");

    // Browser mode only requires the login URL, role ARN, and provider ARN.
    browser_mode = !MapUtils::GetStringValue(connection_attributes, KEY_LOGIN_URL, "").empty();

    std::string err_keys("");
    if (idp_role_arn.empty() || idp_saml_arn.empty()) {
        err_keys += idp_role_arn.empty() ? std::string("\n\t") + KEY_IDP_ROLE_ARN : "";
        err_keys += idp_saml_arn.empty() ? std::string("\n\t") + KEY_IDP_SAML_ARN : "";
    }

    if (!browser_mode && (idp_endpoint.empty() || idp_username.empty() || idp_password.empty())) {
        err_keys += idp_endpoint.empty() ? std::string("\n\t") + KEY_IDP_ENDPOINT : "";
        err_keys += idp_username.empty() ? std::string("\n\t") + KEY_IDP_USERNAME : "";
        err_keys += idp_password.empty() ? std::string("\n\t") + KEY_IDP_PASSWORD : "";
    }

    if (!err_keys.empty()) {
        std::string err_msg = "Missing required parameter for federated authentication:";
        err_msg += err_keys;
        throw std::runtime_error(err_msg);
    }
}
