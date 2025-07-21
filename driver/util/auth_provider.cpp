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

#include "auth_provider.h"

AuthProvider::AuthProvider(const std::string &region)
{
    AwsSdkHelper::Init();

    Aws::RDS::RDSClientConfiguration client_config;
    if (!region.empty()) {
        client_config.region = region;
    }

    rds_client = std::make_shared<Aws::RDS::RDSClient>(
        Aws::Auth::DefaultAWSCredentialsProviderChain().GetAWSCredentials(),
        client_config
    );
}

AuthProvider::AuthProvider(
    const std::string &region,
    Aws::Auth::AWSCredentials credentials)
{
    Aws::RDS::RDSClientConfiguration client_config;
    if (!region.empty()) {
        client_config.region = region;
    }

    rds_client = std::make_shared<Aws::RDS::RDSClient>(
        credentials,
        client_config
    );
}

AuthProvider::~AuthProvider()
{
    this->rds_client.reset();
    AwsSdkHelper::Shutdown();
}

std::pair<std::string, bool> AuthProvider::GetToken(
    const std::string &server,
    const std::string &region,
    const std::string &port,
    const std::string &username,
    bool use_cache,
    std::chrono::milliseconds time_to_expire_ms)
{
    std::string cache_key = BuildCacheKey(server, region, port, username);
    std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
    TokenInfo token_info{"", curr_time + time_to_expire_ms};

    if (use_cache) {
        std::lock_guard<std::recursive_mutex> lock_guard(token_cache_mutex);
        if (token_cache.find(cache_key) != token_cache.end()) {
            token_info = token_cache.at(cache_key);
            if (curr_time > token_info.expiration_point) {
                return std::pair<std::string, bool>(token_info.token, true);
            }
        }
    }

    token_info.token = rds_client->GenerateConnectAuthToken(server.c_str(), region.c_str(), std::atoi(port.c_str()), username.c_str());
    {
        std::lock_guard<std::recursive_mutex> lock_guard(token_cache_mutex);
        token_cache.insert_or_assign(cache_key, token_info);
    }
    return std::pair<std::string, bool>(token_info.token, false);
}

AuthType AuthProvider::AuthTypeFromString(const RDS_STR& auth_type) {
    auto itr = auth_table.find(auth_type);
    if (itr != auth_table.end()) {
        return itr->second;
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
