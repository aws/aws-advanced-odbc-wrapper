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

#ifndef AUTH_MOCK_OBJECTS_H_
#define AUTH_MOCK_OBJECTS_H_

#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/Aws.h>
#include <aws/core/http/HttpClient.h>
#include <aws/rds/RDSClient.h>
#include <aws/secretsmanager/SecretsManagerClient.h>
#include <aws/sts/STSClient.h>

#include "gmock/gmock.h"

#include "../../driver/plugin/base_plugin.h"
#include "../../driver/plugin/federated/saml_util.h"
#include "../../driver/util/auth_provider.h"
#include "../../driver/util/aws_sdk_helper.h"

#include <sqlext.h>

class MOCK_SAML_UTIL : public SamlUtil {
public:
    MOCK_SAML_UTIL() : SamlUtil() { AwsSdkHelper::Init(); };

    MOCK_METHOD(Aws::Auth::AWSCredentials, GetAwsCredentials, (const std::string &assertion), ());
    MOCK_METHOD(std::string, GetSamlAssertion, (), ());
};

class MOCK_AUTH_PROVIDER : public AuthProvider {
public:
    MOCK_AUTH_PROVIDER() : AuthProvider() { AwsSdkHelper::Init(); };

    MOCK_METHOD((std::pair<std::string, bool>), GetToken, (const std::string &server, const std::string &region,
        const std::string &port, const std::string &username, bool use_cache, bool extra_url_encode,
        std::chrono::milliseconds time_to_expire_ms), ());

    MOCK_METHOD(void, UpdateAwsCredential, (const Aws::Auth::AWSCredentials &credentials, const std::string &region), ());
};

class MOCK_RDS_CLIENT : public Aws::RDS::RDSClient {
public:
    MOCK_RDS_CLIENT() : Aws::RDS::RDSClient() {};

    MOCK_METHOD(Aws::String, GenerateConnectAuthToken,
        (const char* dbHostName, const char* dbRegion, unsigned port, const char* dbUserName), (const));
};

class MOCK_SECRETS_MANAGER_CLIENT : public Aws::SecretsManager::SecretsManagerClient {
public:
    MOCK_SECRETS_MANAGER_CLIENT() : Aws::SecretsManager::SecretsManagerClient() {};

    MOCK_METHOD(Aws::SecretsManager::Model::GetSecretValueOutcome, GetSecretValue,
        (const Aws::SecretsManager::Model::GetSecretValueRequest&), (const));
};

class MOCK_BASE_PLUGIN : public BasePlugin {
public:
    MOCK_BASE_PLUGIN() : BasePlugin() {}
    ~MOCK_BASE_PLUGIN() override {}

    MOCK_METHOD(SQLRETURN, Connect,
        (SQLHDBC ConnectionHandle, SQLHWND WindowHandle, SQLTCHAR *OutConnectionString, SQLSMALLINT BufferLength,
        SQLSMALLINT *StringLengthPtr, SQLUSMALLINT DriverCompletion), ());
};

class MOCK_HTTP_RESP : public Aws::Http::HttpResponse {
public:
    MOCK_HTTP_RESP() : Aws::Http::HttpResponse(nullptr) {}
    ~MOCK_HTTP_RESP() override {}

    MOCK_METHOD(Aws::Http::HttpResponseCode, GetResponseCode, (), (const));
    MOCK_METHOD(std::shared_ptr<Aws::Http::HttpResponse>, MakeRequest, (
        const std::shared_ptr<Aws::Http::HttpRequest>&), (const));
    MOCK_METHOD(Aws::IOStream&, GetResponseBody, (), (const));
    MOCK_METHOD(bool, HasClientError, (), (const));
    MOCK_METHOD(Aws::String&, GetClientErrorMessage, (), (const));

    // Not used in test, required to instantiate abstract class
    MOCK_METHOD(Aws::Http::HttpRequest&, GetOriginatingRequest, (), (const));
    MOCK_METHOD(void, SetOriginatingRequest, (const std::shared_ptr<const Aws::Http::HttpRequest>&), ());
    MOCK_METHOD(Aws::Http::HeaderValueCollection, GetHeaders, (), (const));
    MOCK_METHOD(bool, HasHeader, (const char* headerName), (const));
    MOCK_METHOD(Aws::String&, GetHeader, (const Aws::String& headerName), (const));
    MOCK_METHOD(void, SetResponseCode, (Aws::Http::HttpResponseCode httpResponseCode), (const));
    MOCK_METHOD(Aws::String&, GetContentType, (), (const));
    MOCK_METHOD(Aws::Utils::Stream::ResponseStream&&, SwapResponseStreamOwnership, (), ());
    MOCK_METHOD(void, AddHeader, (const Aws::String&, const Aws::String&), ());
    MOCK_METHOD(void, AddHeader, (const Aws::String&, const Aws::String&&), ());
    MOCK_METHOD(void, SetContentType, (const Aws::String&), ());
};

class MOCK_HTTP_CLIENT : public Aws::Http::HttpClient {
public:
    MOCK_METHOD(std::shared_ptr<Aws::Http::HttpResponse>, MakeRequest, (
        const std::shared_ptr<Aws::Http::HttpRequest>&), (const));

    // Not used in test, required to instantiate abstract class
    MOCK_METHOD(std::shared_ptr<Aws::Http::HttpResponse>, MakeRequest, (
        const std::shared_ptr<Aws::Http::HttpRequest>&,
        Aws::Utils::RateLimits::RateLimiterInterface*,
        Aws::Utils::RateLimits::RateLimiterInterface*), (const));
    MOCK_METHOD(bool, SupportsChunkedTransferEncoding, (), (const));
};

class MOCK_STS_CLIENT : public Aws::STS::STSClient {
public:
    MOCK_METHOD(Aws::STS::Model::AssumeRoleWithSAMLOutcome, AssumeRoleWithSAML, (const Aws::STS::Model::AssumeRoleWithSAMLRequest&), (const));
    MOCK_METHOD(bool, SupportsChunkedTransferEncoding, (), (const));
};

#endif // AUTH_MOCK_OBJECTS_H_
