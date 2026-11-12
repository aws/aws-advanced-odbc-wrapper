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
#include <set>
#include <sstream>
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
    constexpr char LITERAL_KEYS_SEPARATOR = ',';
    constexpr const char* WHITESPACE = " \t\r\n";

    // Matching single attribute value:
    // - brace quoted value,
    // - where an embedded closing brace is escaped by doubling it, 
    // - or a plain value running up to the next attribute separator
    // The quoted form repeats a group once per '}}' rather than once per character: MSVC STL before 14.51
    // matches such a group recursively and throws regex_error(error_stack) after a few hundred characters,
    // which a braced IAM token exceeds.
    const std::string VALUE_PATTERN(R"((\{[^}]*(?:\}\}[^}]*)*\}|[^;]*))");

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

    // Pattern matching the attribute with the given key and its value, followed by the suffix
    std::regex AttributePattern(const std::string& key, const char* suffix = "") {
        std::string pattern("(");
        pattern += key;
        pattern += "=)";
        pattern += VALUE_PATTERN;
        pattern += suffix;
        return std::regex(pattern);
    }

    std::set<std::string> GetLiteralKeys(const std::map<std::string, std::string>& conn_map) {
        std::set<std::string> literal_keys;
        const auto itr = conn_map.find(KEY_LITERAL_KEYS);
        if (itr == conn_map.end()) {
            return literal_keys;
        }

        std::istringstream stream(itr->second);
        std::string key;
        while (std::getline(stream, key, LITERAL_KEYS_SEPARATOR)) {
            if (!key.empty()) {
                literal_keys.insert(key);
            }
        }
        return literal_keys;
    }

    void SetLiteralKeys(std::map<std::string, std::string>& conn_map, const std::set<std::string>& literal_keys) {
        if (literal_keys.empty()) {
            conn_map.erase(KEY_LITERAL_KEYS);
            return;
        }

        std::string value;
        for (const std::string& key : literal_keys) {
            if (!value.empty()) {
                value.push_back(LITERAL_KEYS_SEPARATOR);
            }
            value += key;
        }
        conn_map.insert_or_assign(KEY_LITERAL_KEYS, value);
    }
}  // namespace

bool ConnectionStringHelper::ParseConnectionString(const std::string &conn_str, std::map<std::string, std::string> &conn_map)
{
    std::set<std::string> literal_keys = GetLiteralKeys(conn_map);
    bool well_formed = true;
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
                // Only whitespace may follow the closing brace, anything else up to the next attribute is ignored
                const size_t attribute_separator_pos = conn_str.find(ATTRIBUTE_SEPARATOR, close_pos);
                const size_t attribute_end_pos = (attribute_separator_pos == std::string::npos) ? conn_str.length() : attribute_separator_pos;
                if (conn_str.find_first_not_of(WHITESPACE, close_pos) < attribute_end_pos) {
                    well_formed = false;
                }
                next_pos = (attribute_separator_pos == std::string::npos) ? conn_str.length() : attribute_separator_pos + 1;
            } else {
                well_formed = false;
            }
        }

        if (!quoted) {
            // An unterminated quoted value is treated as a plain value
            const size_t attribute_separator_pos = conn_str.find(ATTRIBUTE_SEPARATOR, value_pos);
            value = conn_str.substr(value_pos,
                (attribute_separator_pos == std::string::npos) ? std::string::npos : attribute_separator_pos - value_pos);
            next_pos = (attribute_separator_pos == std::string::npos) ? conn_str.length() : attribute_separator_pos + 1;
        }

        // An empty value enclosed in braces is kept, it is an explicit empty value such as an empty password
        if (!key.empty() && (quoted || !value.empty())) {
            const std::string upper_key = RDS_STR_UPPER(key);
            if (upper_key == KEY_LITERAL_KEYS) {
                // Marker carried over from a connection string built by BuildFullConnectionString
                const std::map<std::string, std::string> carried_marker = {{KEY_LITERAL_KEYS, value}};
                literal_keys.merge(GetLiteralKeys(carried_marker));
            } else {
                // Connection String takes precedence
                conn_map.insert_or_assign(upper_key, value);
                // Remember whether the value was enclosed in braces, so it reaches the underlying driver as written
                if (quoted) {
                    literal_keys.insert(upper_key);
                } else {
                    literal_keys.erase(upper_key);
                }
            }
        }

        pos = next_pos;
    }

    SetLiteralKeys(conn_map, literal_keys);
    return well_formed;
}

void ConnectionStringHelper::SetGeneratedPassword(std::map<std::string, std::string> &conn_map, const std::string &password)
{
    conn_map.insert_or_assign(KEY_DB_PASSWORD, password);
    SetLiteralKey(conn_map, KEY_DB_PASSWORD, true);
}

void ConnectionStringHelper::MergeBaseConnectionString(const std::map<std::string, std::string> &base_conn_map, std::map<std::string, std::string> &conn_map)
{
    const std::set<std::string> base_literal_keys = GetLiteralKeys(base_conn_map);
    for (const auto& [key, value] : base_conn_map) {
        if (key == KEY_LITERAL_KEYS) {
            continue;
        }
        // Insert if absent, connection string keys take precedence
        if (conn_map.try_emplace(key, value).second && base_literal_keys.contains(key)) {
            SetLiteralKey(conn_map, key, true);
        }
    }
}

bool ConnectionStringHelper::IsLiteralKey(const std::map<std::string, std::string> &conn_map, const std::string &key)
{
    return GetLiteralKeys(conn_map).contains(key);
}

void ConnectionStringHelper::SetLiteralKey(std::map<std::string, std::string> &conn_map, const std::string &key, const bool literal)
{
    std::set<std::string> literal_keys = GetLiteralKeys(conn_map);
    if (literal) {
        literal_keys.insert(key);
    } else {
        literal_keys.erase(key);
    }
    SetLiteralKeys(conn_map, literal_keys);
}

std::string ConnectionStringHelper::QuoteValue(const std::string &value, const bool force)
{
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

std::string ConnectionStringHelper::BuildMinimumConnectionString(const std::map<std::string, std::string> &conn_map)
{
    const std::set<std::string> literal_keys = GetLiteralKeys(conn_map);

    std::ostringstream conn_stream;
    for (const auto& e : conn_map) {
        if (!IsAwsOdbcKey(e.first)) {
            if (conn_stream.tellp() > 0) {
                conn_stream << ";";
            }

            conn_stream << e.first << "=" << QuoteValue(e.second, literal_keys.contains(e.first));
        }
    }

    return conn_stream.str();
}

std::string ConnectionStringHelper::BuildFullConnectionString(const std::map<std::string, std::string> &conn_map)
{
    const std::set<std::string> literal_keys = GetLiteralKeys(conn_map);

    std::ostringstream conn_stream;
    for (const auto& e : conn_map) {
        if (conn_stream.tellp() > 0) {
            conn_stream << ";";
        }

        conn_stream << e.first << "=" << QuoteValue(e.second, literal_keys.contains(e.first));
    }
    return conn_stream.str();
}

std::string ConnectionStringHelper::MaskSensitiveInformation(const std::string &conn_str)
{
    std::string result(conn_str);
    for (const std::string& key : SENSITIVE_KEY_SET) {
        const std::regex pattern = AttributePattern(key);
        result = std::regex_replace(result, pattern, "$1[REDACTED]");
    }
    return result;
}

std::string ConnectionStringHelper::RemoveInternalWrapperKeys(const std::string& conn_str) {
    std::string result(conn_str);
    for (const std::string& key : INTERNAL_WRAPPER_KEY_SET) {
        const std::regex pattern = AttributePattern(key, ";?");
        result = std::regex_replace(result, pattern, "");
    }
    return result;
}

std::string ConnectionStringHelper::BuildDsnOnlyConnectionString(const std::map<std::string, std::string>& conn_map, const std::string& full_conn_str) {
    if (MapUtils::GetBooleanValue(conn_map, KEY_DSN_ONLY_OUTPUT, false)) {
        return std::string(KEY_DSN) + "=" + QuoteValue(MapUtils::GetStringValue(conn_map, KEY_DSN, "[empty]"), IsLiteralKey(conn_map, KEY_DSN));
    }
    return full_conn_str;
}

std::vector<std::string> ConnectionStringHelper::GetDeprecatedKeysInUse(const std::map<std::string, std::string> &conn_map)
{
    std::vector<std::string> keys_in_use;
    // Iterating the ordered connection map keeps the result deterministic
    for (const auto& e : conn_map) {
        if (DEPRECATED_KEY_SET.contains(e.first) && e.second == VALUE_BOOL_TRUE) {
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
