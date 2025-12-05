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

#ifndef FEDERATION_H_
#define FEDERATION_H_

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/http/HttpClient.h>
#include <aws/sts/STSClient.h>

#include "../../util/rds_strings.h"

class SamlUtil {
public:
    SamlUtil() = default;
    SamlUtil(std::map<std::string, std::string> connection_attributes);
    SamlUtil(std::map<std::string, std::string> connection_attributes, const std::shared_ptr<Aws::Http::HttpClient>& http_client, const std::shared_ptr<Aws::STS::STSClient>& sts_client);
    virtual ~SamlUtil();

    virtual Aws::Auth::AWSCredentials GetAwsCredentials(const std::string &assertion);
    virtual std::string GetSamlAssertion() = 0;

    const int DEFAULT_SOCKET_TIMEOUT_MS = 3000;
    const int DEFAULT_CONNECT_TIMEOUT_MS = 1000;

protected:
    std::string idp_endpoint;
    std::string idp_port;
    std::string idp_username;
    std::string idp_password;
    std::string idp_role_arn;
    std::string idp_saml_arn;

    std::shared_ptr<Aws::Http::HttpClient> http_client;
    std::shared_ptr<Aws::STS::STSClient> sts_client;

private:
    void ParseIdpConfig(std::map<std::string, std::string> connection_attributes);
};

#endif // FEDERATION_H_
