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

void ConnectionStringHelper::ParseConnectionString(const RDS_STR &conn_str, std::map<RDS_STR, RDS_STR> &conn_map)
{
    RDS_REGEX pattern(TEXT("([^;=]+)=([^;]+)"));
    RDS_MATCH match;
    RDS_STR conn_str_itr = conn_str;

    while (std::regex_search(conn_str_itr, match, pattern)) {
        RDS_STR key = match[1].str();
        RDS_STR_UPPER(key);
        RDS_STR val = match[2].str();

        // Connection String takes precedence
        conn_map.insert_or_assign(key, val);
        conn_str_itr = match.suffix().str();
    }
}

RDS_STR ConnectionStringHelper::BuildMinimumConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map, bool redact_sensitive)
{
    RDS_STR_STREAM conn_stream;
    for (const auto& e : conn_map) {
        if (!IsAwsOdbcKey(e.first)) {
            if (conn_stream.tellp() > 0) {
                conn_stream << TEXT(";");
            }

            if (redact_sensitive && IsSensitiveData(e.first)) {
                conn_stream << e.first << TEXT("=") << "[REDACTED]";
            } else {
                conn_stream << e.first << TEXT("=") << e.second;
            }
        }
    }
    return conn_stream.str();
}

RDS_STR ConnectionStringHelper::BuildFullConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map, bool redact_sensitive)
{
    RDS_STR_STREAM conn_stream;
    for (const auto& e : conn_map) {
        if (conn_stream.tellp() > 0) {
            conn_stream << TEXT(";");
        }

        if (redact_sensitive && IsSensitiveData(e.first)) {
            conn_stream << e.first << TEXT("=") << "[REDACTED]";
        } else {
            conn_stream << e.first << TEXT("=") << e.second;
        }
    }
    return conn_stream.str();
}

bool ConnectionStringHelper::IsAwsOdbcKey(const RDS_STR &aws_odbc_key)
{
    return aws_odbc_key_set.contains(aws_odbc_key);
}

bool ConnectionStringHelper::IsSensitiveData(const RDS_STR &sensitive_key)
{
    return sensitive_key_set.contains(sensitive_key);
}
