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

#include "connection_string_helper.h"

#include "connection_string_keys.h"
#include "odbc_dsn_helper.h"
#include "unicode/ucasemap.h"
#include "unicode/utypes.h"

void ConnectionStringHelper::ParseConnectionString(std::string conn_str, std::map<std::string, std::string> &conn_map)
{
    const std::regex pattern("([^;=]+)=([^;]+)");
    std::smatch match;
    std::string conn_str_itr = std::move(conn_str);

    while (std::regex_search(conn_str_itr, match, pattern)) {
        const std::string key = match[1].str();
        const std::string val = match[2].str();

        // Connection String takes precedence
        conn_map.insert_or_assign(RDS_STR_UPPER(key), val);
        conn_str_itr = match.suffix().str();
    }
}

std::string ConnectionStringHelper::BuildMinimumConnectionString(const std::map<std::string, std::string> &conn_map)
{
    std::ostringstream conn_stream;
    for (const auto& e : conn_map) {
        if (!IsAwsOdbcKey(e.first)) {
            if (conn_stream.tellp() > 0) {
                conn_stream << ";";
            }

            conn_stream << e.first << "=" << e.second;
        }
    }

    return conn_stream.str();
}

std::string ConnectionStringHelper::BuildFullConnectionString(const std::map<std::string, std::string> &conn_map)
{
    std::ostringstream conn_stream;
    for (const auto& e : conn_map) {
        if (conn_stream.tellp() > 0) {
            conn_stream << ";";
        }

        conn_stream << e.first << "=" << e.second;
    }
    return conn_stream.str();
}

std::string ConnectionStringHelper::MaskSensitiveInformation(const std::string &conn_str)
{
    std::string result(conn_str);
    for (const std::string& key : sensitive_key_set) {
        const std::regex pattern("(" + key + "=)([^;]+)");
        result = std::regex_replace(result, pattern, "$1[REDACTED]");
    }
    return result;
}

std::string ConnectionStringHelper::RemoveInternalWrapperKeys(const std::string& conn_str) {
    std::string result(conn_str);
    for (const std::string& key : internal_wrapper_key_set) {
        const std::regex pattern("(" + key + "=)([^;]+)");
        result = std::regex_replace(result, pattern, "");
    }
    return result;
}

bool ConnectionStringHelper::IsAwsOdbcKey(const std::string &aws_odbc_key)
{
    return aws_odbc_key_set.contains(aws_odbc_key);
}

bool ConnectionStringHelper::IsSensitiveData(const std::string &sensitive_key)
{
    return sensitive_key_set.contains(sensitive_key);
}

std::string ConnectionStringHelper::GetRealKeyName(const std::string& alias_key)
{
    auto key_it = alias_to_real_map.find(alias_key);
    if (key_it != alias_to_real_map.end()) {
        return key_it->second;
    }
    return alias_key;
}
