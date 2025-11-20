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
#include "../../util/rds_strings.h"
#include "../../util/rds_utils.h"

SamlUtil::SamlUtil(std::map<RDS_STR, RDS_STR> connection_attributes)
    : SamlUtil(std::move(connection_attributes), nullptr, nullptr) {}

SamlUtil::SamlUtil(
    std::map<RDS_STR, RDS_STR> connection_attributes,
    const std::shared_ptr<Aws::Http::HttpClient>& http_client,
    const std::shared_ptr<Aws::STS::STSClient>& sts_client)
{
    ParseIdpConfig(connection_attributes);
    AwsSdkHelper::Init();
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
        std::string region = connection_attributes.contains(KEY_REGION) ?
            connection_attributes.at(KEY_REGION) : "";
        if (region.empty()) {
            region = connection_attributes.contains(KEY_SERVER) ?
                RdsUtils::GetRdsRegion(connection_attributes.at(KEY_SERVER))
                : Aws::Region::US_EAST_1;
        }
        Aws::STS::STSClientConfiguration sts_client_config;
        sts_client_config.region = region;
        this->sts_client = std::make_shared<Aws::STS::STSClient>(sts_client_config);
    }
}

SamlUtil::~SamlUtil()
{
    if (http_client) {
        http_client.reset();
    }
    if (sts_client) {
        sts_client.reset();
    }
    AwsSdkHelper::Shutdown();
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
        LOG(ERROR) << "STS failed to assume role with assertion: " << outcome.GetError().GetMessage();
        return {};
    }

    const Aws::STS::Model::AssumeRoleWithSAMLResult &result = outcome.GetResult();
    const Aws::STS::Model::Credentials &temp_credentials = result.GetCredentials();
    const Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(
        temp_credentials.GetAccessKeyId(), temp_credentials.GetSecretAccessKey(), temp_credentials.GetSessionToken());
    return credentials;
}

void SamlUtil::ParseIdpConfig(std::map<RDS_STR, RDS_STR> connection_attributes)
{
    idp_endpoint = connection_attributes.contains(KEY_IDP_ENDPOINT) ?
        connection_attributes.at(KEY_IDP_ENDPOINT) : "";
    idp_port = connection_attributes.contains(KEY_IDP_PORT) ?
        connection_attributes.at(KEY_IDP_PORT) : "443";
    idp_username = connection_attributes.contains(KEY_IDP_USERNAME) ?
        connection_attributes.at(KEY_IDP_USERNAME) : "";
    idp_password = connection_attributes.contains(KEY_IDP_PASSWORD) ?
        connection_attributes.at(KEY_IDP_PASSWORD) : "";
    idp_role_arn = connection_attributes.contains(KEY_IDP_ROLE_ARN) ?
        connection_attributes.at(KEY_IDP_ROLE_ARN) : "";
    idp_saml_arn = connection_attributes.contains(KEY_IDP_SAML_ARN) ?
        connection_attributes.at(KEY_IDP_SAML_ARN) : "";


    if (idp_endpoint.empty() || idp_username.empty() || idp_password.empty() || idp_role_arn.empty() || idp_saml_arn.empty()) {
        std::string err_msg = "Missing required parameter for Federated Auth:";
        err_msg += idp_endpoint.empty() ? std::string("\n\t") + KEY_IDP_ENDPOINT : "";
        err_msg += idp_username.empty() ? std::string("\n\t") + KEY_IDP_USERNAME : "";
        err_msg += idp_password.empty() ? std::string("\n\t") + KEY_IDP_PASSWORD : "";
        err_msg += idp_role_arn.empty() ? std::string("\n\t") + KEY_IDP_ROLE_ARN : "";
        err_msg += idp_saml_arn.empty() ? std::string("\n\t") + KEY_IDP_SAML_ARN : "";
        throw std::runtime_error(err_msg);
    }
}
