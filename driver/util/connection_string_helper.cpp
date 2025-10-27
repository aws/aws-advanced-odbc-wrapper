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
    const RDS_REGEX pattern("([^;=]+)=([^;]+)");
    RDS_MATCH match;
    RDS_STR conn_str_itr = conn_str;

    while (std::regex_search(conn_str_itr, match, pattern)) {
        RDS_STR key = match[1].str();
        const RDS_STR val = match[2].str();

        // Connection String takes precedence
        conn_map.insert_or_assign(RDS_STR_UPPER(key), val);
        conn_str_itr = match.suffix().str();
    }
}

RDS_STR ConnectionStringHelper::BuildMinimumConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map)
{
    RDS_STR_STREAM conn_stream;
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

RDS_STR ConnectionStringHelper::MaskSensitiveInformation(const RDS_STR &conn_str)
{
    RDS_STR result(conn_str);
    for (const RDS_STR& key : sensitive_key_set) {
        const RDS_REGEX pattern("(" + key + "=)([^;]+)");
        result = std::regex_replace(result, pattern, "$1[REDACTED]");
    }
    return result;
}

bool ConnectionStringHelper::IsAwsOdbcKey(const RDS_STR &aws_odbc_key)
{
    return aws_odbc_key_set.contains(aws_odbc_key);
}

bool ConnectionStringHelper::IsSensitiveData(const RDS_STR &sensitive_key)
{
    return sensitive_key_set.contains(sensitive_key);
}
