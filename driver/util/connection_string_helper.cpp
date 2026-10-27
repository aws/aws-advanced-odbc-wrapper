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

#include <optional>
#include <string>

#include "connection_string_keys.h"
#include "map_utils.h"
#include "odbc_dsn_helper.h"
#include "unicode/ucasemap.h"
#include "unicode/utypes.h"

namespace {
    constexpr char KEY_VALUE_SEPARATOR = '=';
    constexpr char ATTRIBUTE_SEPARATOR = ';';
    constexpr char QUOTE_OPEN = '{';
    constexpr char QUOTE_CLOSE = '}';

    // Matching single attribute value:
    // - brace quoted value,
    // - where an embedded closing brace is escaped by doubling it, 
    // - or a plain value running up to the next attribute separator
    const std::string VALUE_PATTERN(R"((\{(?:[^}]|\}\})*\}|[^;]*))");

    std::string TrimWhitespace(const std::string& str) {
        const size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }
        const size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    std::optional<std::string> ReadQuotedValue(const std::string& conn_str, const size_t open_pos, size_t& close_pos) {
        std::string value;
        for (size_t i = open_pos + 1; i < conn_str.length(); i++) {
            if (conn_str[i] != QUOTE_CLOSE) {
                value.push_back(conn_str[i]);
                continue;
            }

            if (i + 1 < conn_str.length() && conn_str[i + 1] == QUOTE_CLOSE) {
                value.push_back(QUOTE_CLOSE);
                i++;
                continue;
            }

            close_pos = i + 1;
            return value;
        }

        return std::nullopt;
    }

    bool RequiresQuoting(const std::string& value) {
        return value.find(ATTRIBUTE_SEPARATOR) != std::string::npos || value.starts_with(QUOTE_OPEN);
    }

    std::string QuoteValue(const std::string& value, const bool force = false) {
        if (!force && !RequiresQuoting(value)) {
            return value;
        }

        std::string quoted(1, QUOTE_OPEN);
        for (const char c : value) {
            quoted.push_back(c);
            if (c == QUOTE_CLOSE) {
                // Escape the literal brace by doubling it
                quoted.push_back(QUOTE_CLOSE);
            }
        }
        quoted.push_back(QUOTE_CLOSE);

        return quoted;
    }
}  // namespace

void ConnectionStringHelper::ParseConnectionString(const std::string &conn_str, std::map<std::string, std::string> &conn_map)
{
    size_t pos = 0;

    while (pos < conn_str.length()) {
        const size_t separator_pos = conn_str.find(KEY_VALUE_SEPARATOR, pos);
        if (separator_pos == std::string::npos) {
            // No remaining key value pair
            break;
        }

        const size_t attribute_end = conn_str.find(ATTRIBUTE_SEPARATOR, pos);
        if (attribute_end < separator_pos) {
            // Attribute holds no value, skip it
            pos = attribute_end + 1;
            continue;
        }

        const std::string key = TrimWhitespace(conn_str.substr(pos, separator_pos - pos));
        const size_t value_pos = separator_pos + 1;

        std::string value;
        size_t next_pos = std::string::npos;
        bool quoted = false;

        if (value_pos < conn_str.length() && conn_str[value_pos] == QUOTE_OPEN) {
            size_t close_pos = 0;
            if (std::optional<std::string> quoted_value = ReadQuotedValue(conn_str, value_pos, close_pos)) {
                value = std::move(quoted_value.value());
                quoted = true;
                // Anything between the closing brace and the next attribute is ignored
                const size_t attribute_separator_pos = conn_str.find(ATTRIBUTE_SEPARATOR, close_pos);
                next_pos = (attribute_separator_pos == std::string::npos) ? conn_str.length() : attribute_separator_pos + 1;
            }
        }

        if (!quoted) {
            // An unterminated quoted value is treated as a plain value
            const size_t attribute_separator_pos = conn_str.find(ATTRIBUTE_SEPARATOR, value_pos);
            value = conn_str.substr(value_pos,
                (attribute_separator_pos == std::string::npos) ? std::string::npos : attribute_separator_pos - value_pos);
            next_pos = (attribute_separator_pos == std::string::npos) ? conn_str.length() : attribute_separator_pos + 1;
        }

        if (!key.empty() && !value.empty()) {
            // Connection String takes precedence
            conn_map.insert_or_assign(RDS_STR_UPPER(key), value);
        }

        pos = next_pos;
    }
}

void ConnectionStringHelper::SetGeneratedPassword(std::map<std::string, std::string> &conn_map, const std::string &password)
{
    conn_map.insert_or_assign(KEY_DB_PASSWORD, password);
    conn_map.insert_or_assign(KEY_LITERAL_CREDENTIALS, VALUE_BOOL_TRUE);
}

std::string ConnectionStringHelper::BuildMinimumConnectionString(const std::map<std::string, std::string> &conn_map)
{
    const bool literal_credentials = MapUtils::GetBooleanValue(conn_map, KEY_LITERAL_CREDENTIALS, false);

    std::ostringstream conn_stream;
    for (const auto& e : conn_map) {
        if (!IsAwsOdbcKey(e.first)) {
            if (conn_stream.tellp() > 0) {
                conn_stream << ";";
            }

            conn_stream << e.first << "=" << QuoteValue(e.second, literal_credentials && e.first == KEY_DB_PASSWORD);
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

        conn_stream << e.first << "=" << QuoteValue(e.second);
    }
    return conn_stream.str();
}

std::string ConnectionStringHelper::MaskSensitiveInformation(const std::string &conn_str)
{
    std::string result(conn_str);
    for (const std::string& key : SENSITIVE_KEY_SET) {
        const std::regex pattern("(" + key + "=)" + VALUE_PATTERN);
        result = std::regex_replace(result, pattern, "$1[REDACTED]");
    }
    return result;
}

std::string ConnectionStringHelper::RemoveInternalWrapperKeys(const std::string& conn_str) {
    std::string result(conn_str);
    for (const std::string& key : INTERNAL_WRAPPER_KEY_SET) {
        const std::regex pattern("(" + key + "=)" + VALUE_PATTERN + ";?");
        result = std::regex_replace(result, pattern, "");
    }
    return result;
}

std::string ConnectionStringHelper::BuildDsnOnlyConnectionString(const std::map<std::string, std::string>& conn_map, const std::string& full_conn_str) {
    if (MapUtils::GetBooleanValue(conn_map, KEY_DSN_ONLY_OUTPUT, false)) {
        return std::string(KEY_DSN) + "=" + QuoteValue(MapUtils::GetStringValue(conn_map, KEY_DSN, "[empty]"));
    }
    return full_conn_str;
}

std::vector<std::string> ConnectionStringHelper::GetDeprecatedKeysInUse(const std::map<std::string, std::string> &conn_map)
{
    std::vector<std::string> keys_in_use;
    // Iterating the ordered connection map keeps the result deterministic
    for (const auto& e : conn_map) {
        if (DEPRECATED_KEY_SET.contains(e.first) && !e.second.empty() && e.second != VALUE_BOOL_FALSE) {
            keys_in_use.push_back(e.first);
        }
    }
    return keys_in_use;
}

bool ConnectionStringHelper::IsAwsOdbcKey(const std::string &aws_odbc_key)
{
    return AWS_ODBC_KEY_SET.contains(aws_odbc_key);
}

bool ConnectionStringHelper::IsSensitiveData(const std::string &sensitive_key)
{
    return SENSITIVE_KEY_SET.contains(sensitive_key);
}

std::string ConnectionStringHelper::GetRealKeyName(const std::string& alias_key)
{
    const auto key_it = ALIAS_TO_REAL_MAP.find(alias_key);
    if (key_it != ALIAS_TO_REAL_MAP.end()) {
        return key_it->second;
    }
    return alias_key;
}
