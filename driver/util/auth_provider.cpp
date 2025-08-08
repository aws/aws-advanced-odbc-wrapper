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

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/rds/RDSClient.h>

#include "auth_provider.h"

#include "logger_wrapper.h"

AuthProvider::AuthProvider(const std::string &region) {
    AwsSdkHelper::Init();
    UpdateAwsCredential(Aws::Auth::DefaultAWSCredentialsProviderChain().GetAWSCredentials(), region);
}

AuthProvider::AuthProvider(
    const std::string &region,
    Aws::Auth::AWSCredentials credentials)
{
    AwsSdkHelper::Init();
    UpdateAwsCredential(credentials, region);
}

AuthProvider::AuthProvider(std::shared_ptr<Aws::RDS::RDSClient> rds_client)
{
    AwsSdkHelper::Init();
    this->rds_client = rds_client;
}

AuthProvider::~AuthProvider()
{
    if (rds_client) rds_client.reset();
    AwsSdkHelper::Shutdown();
}

std::pair<std::string, bool> AuthProvider::GetToken(
    const std::string &server,
    const std::string &region,
    const std::string &port,
    const std::string &username,
    bool use_cache,
    bool extra_url_encode,
    std::chrono::milliseconds time_to_expire_ms)
{
    std::string cache_key = BuildCacheKey(server, region, port, username);
    std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
    TokenInfo token_info{"", curr_time + time_to_expire_ms};

    if (use_cache) {
        std::lock_guard<std::recursive_mutex> lock_guard(token_cache_mutex);
        if (token_cache.contains(cache_key)) {
            token_info = token_cache.at(cache_key);
            if (curr_time < token_info.expiration_point) {
                return std::pair<std::string, bool>(token_info.token, true);
            } else {
                token_cache.erase(cache_key);
            }
        }
    }

    std::string aws_token = rds_client->GenerateConnectAuthToken(server.c_str(), region.c_str(), std::strtol(port.c_str(), nullptr, 10), username.c_str());
    token_info.token = extra_url_encode ? ExtraUrlEncodeString(aws_token) : aws_token;
    {
        std::lock_guard<std::recursive_mutex> lock_guard(token_cache_mutex);
        token_cache.insert_or_assign(cache_key, token_info);
    }
    DLOG(INFO) << "Returning Token Length: " << token_info.token.length();
    return std::pair<std::string, bool>(token_info.token, false);
}

void AuthProvider::UpdateAwsCredential(Aws::Auth::AWSCredentials credentials, const std::string &region) {
    if (rds_client) rds_client.reset();

    Aws::RDS::RDSClientConfiguration client_config;
    if (!region.empty()) client_config.region = region;

    rds_client = std::make_shared<Aws::RDS::RDSClient>(
        credentials,
        client_config
    );
}

std::string AuthProvider::ExtraUrlEncodeString(const std::string &url_str) {
    DLOG(INFO) << "Additional URL Encode: " << url_str;
    std::string result;
    result = std::regex_replace(url_str, std::regex("%"), "%25");
    DLOG(INFO) << "URL Encoded: " << result;
    return result;
}

AuthType AuthProvider::AuthTypeFromString(const RDS_STR& auth_type) {
    RDS_STR local_str = auth_type;
    RDS_STR_UPPER(local_str);
    if (auth_table.contains(local_str)) {
        return auth_table.at(local_str);
    } else {
        return AuthType::INVALID;
    }
}

std::string AuthProvider::BuildCacheKey(
    const std::string &server,
    const std::string &region,
    const std::string &port,
    const std::string &username)
{
    std::stringstream key_builder;
    key_builder << server << "-" << region << "-" << port << "-" << username;
    return key_builder.str();
}

void AuthProvider::ClearCache() {
    std::lock_guard<std::recursive_mutex> lock_guard(token_cache_mutex);
    token_cache.clear();
}
