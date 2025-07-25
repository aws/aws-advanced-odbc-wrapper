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

SamlUtil::SamlUtil(std::map<RDS_STR, RDS_STR> connection_attributes)
{
    AwsSdkHelper::Init();
    ParseIdpConfig(connection_attributes);

    Aws::Client::ClientConfiguration http_client_config;
    auto map_end_itr = connection_attributes.end();
    if (connection_attributes.find(KEY_HTTP_SOCKET_TIMEOUT) != map_end_itr) {
        std::string socket_timeout_str = ToStr(connection_attributes.at(KEY_HTTP_SOCKET_TIMEOUT));
        int socket_timeout = std::atoi(socket_timeout_str.c_str());
        http_client_config.requestTimeoutMs = socket_timeout > 0 ? socket_timeout : DEFAULT_SOCKET_TIMEOUT_MS;
    }
    if (connection_attributes.find(KEY_HTTP_CONNECT_TIMEOUT) != map_end_itr) {
        std::string connect_timeout_str = ToStr(connection_attributes.at(KEY_HTTP_CONNECT_TIMEOUT));
        int connect_timeout = std::atoi(connect_timeout_str.c_str());
        http_client_config.connectTimeoutMs = connect_timeout > 0 ? connect_timeout : DEFAULT_CONNECT_TIMEOUT_MS;
    }
    http_client_config.verifySSL = true;
    http_client_config.followRedirects = Aws::Client::FollowRedirectsPolicy::ALWAYS;
    http_client = Aws::Http::CreateHttpClient(http_client_config);

    Aws::STS::STSClientConfiguration sts_client_config;
    if (connection_attributes.find(KEY_REGION) != map_end_itr) {
        sts_client_config.region = ToStr(connection_attributes.at(KEY_REGION));
    }
    sts_client = std::make_shared<Aws::STS::STSClient>(sts_client_config);
}

SamlUtil::~SamlUtil()
{
    if (http_client) http_client.reset();
    if (sts_client) sts_client.reset();
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
        LOG(ERROR) << "STS failed to assume role with assertion";
        return Aws::Auth::AWSCredentials();
    }

    const Aws::STS::Model::AssumeRoleWithSAMLResult &result = outcome.GetResult();
    const Aws::STS::Model::Credentials &temp_credentials = result.GetCredentials();
    const Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(
        temp_credentials.GetAccessKeyId(), temp_credentials.GetSecretAccessKey(), temp_credentials.GetSessionToken());
    return credentials;
}

void SamlUtil::ParseIdpConfig(std::map<RDS_STR, RDS_STR> connection_attributes)
{
    auto map_end_itr = connection_attributes.end();
    idp_endpoint = connection_attributes.find(KEY_IDP_ENDPOINT) != map_end_itr ?
        ToStr(connection_attributes.at(KEY_IDP_ENDPOINT)) : "";
    idp_port = connection_attributes.find(KEY_IDP_PORT) != map_end_itr ?
        ToStr(connection_attributes.at(KEY_IDP_PORT)) : "";
    idp_username = connection_attributes.find(KEY_IDP_USERNAME) != map_end_itr ?
        ToStr(connection_attributes.at(KEY_IDP_USERNAME)) : "";
    idp_password = connection_attributes.find(KEY_IDP_PASSWORD) != map_end_itr ?
        ToStr(connection_attributes.at(KEY_IDP_PASSWORD)) : "";
    idp_role_arn = connection_attributes.find(KEY_IDP_ROLE_ARN) != map_end_itr ?
        ToStr(connection_attributes.at(KEY_IDP_ROLE_ARN)) : "";
    idp_saml_arn = connection_attributes.find(KEY_IDP_SAML_ARN) != map_end_itr ?
        ToStr(connection_attributes.at(KEY_IDP_SAML_ARN)) : "";
}
