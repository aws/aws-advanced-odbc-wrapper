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

#include "odbc_dsn_helper.h"

#include <odbcinst.h>

#include <algorithm>
#include <string>
#include <vector>

#include "logger_wrapper.h"

#include "../driver.h"
#include "../util/connection_string_helper.h"
#include "../util/connection_string_keys.h"

void OdbcDsnHelper::LoadAll(const std::string &dsn_key, std::map<std::string, std::string> &conn_map)
{
    int size = 0;
    std::vector<std::string> entry_names;

    #ifdef UNICODE
    std::vector<uint16_t> dsn_key_vec = ConvertUTF8ToUTF16(dsn_key);
    std::vector<uint16_t> empty_vec = ConvertUTF8ToUTF16("");
    std::vector<uint16_t> odbc_ini_vec = ConvertUTF8ToUTF16(ODBC_INI);
    odbc_ini_vec.push_back(0);

    std::vector<uint16_t> buffer(MAX_VAL_SIZE, 0);
    // Check DSN if it is valid and contains entries
    size = SQLGetPrivateProfileString(reinterpret_cast<SQLWCHAR*>(dsn_key_vec.data()), nullptr, reinterpret_cast<SQLWCHAR*>(empty_vec.data()), reinterpret_cast<SQLWCHAR*>(buffer.data()), MAX_VAL_SIZE, reinterpret_cast<SQLWCHAR*>(odbc_ini_vec.data()));

    for (size_t pos = 0; pos < buffer.size() && buffer[pos] != 0;) {
        const auto start = buffer.begin() + static_cast<std::ptrdiff_t>(pos);
        const auto end = std::find(start, buffer.end(), static_cast<uint16_t>(0));
        // ConvertUTF16ToUTF8 requires a NUL-terminated input, so copy the entry out.
        std::vector<uint16_t> entry(start, end);
        entry.push_back(0);
        entry_names.push_back(ConvertUTF16ToUTF8(entry.data()));
        pos = static_cast<size_t>(std::distance(buffer.begin(), end)) + 1;
    }
#else
    std::vector<char> buffer(MAX_VAL_SIZE, '\0');
    size = SQLGetPrivateProfileString(dsn_key.c_str(), nullptr, EMPTY_RDS_STR, buffer.data(), MAX_VAL_SIZE, ODBC_INI);

    for (size_t pos = 0; pos < buffer.size() && buffer[pos] != '\0';) {
        const auto start = buffer.begin() + static_cast<std::ptrdiff_t>(pos);
        const auto end = std::find(start, buffer.end(), '\0');
        entry_names.emplace_back(start, end);
        pos = static_cast<size_t>(std::distance(buffer.begin(), end)) + 1;
    }
#endif

    if (size < 1) {
        // No entries in DSN
        // TODO - Error handling?
        LOG(WARNING) << "No DSN entry found for: " << dsn_key;
        return;
    }

    // Load entries into map
    for (const std::string& raw_key : entry_names) {
        const std::string key = RDS_STR_UPPER(raw_key);
        const std::string val = Load(dsn_key, key);

        // Insert if value exists
        if (!val.empty()) {
            if (key == KEY_BASE_CONN) {
                std::map<std::string, std::string> base_conn_map;
                ConnectionStringHelper::ParseConnectionString(val, base_conn_map);
                for (const auto& pair : base_conn_map) {
                    std::string base_conn_val = pair.second;
                    if (base_conn_val.back() == ';') {
                        base_conn_val.pop_back();
                    }
                    if (!base_conn_val.empty()) {
                        conn_map.try_emplace(pair.first, base_conn_val);
                    }
                }
            }
            else {
                // Insert if absent, connection string keys take precedence
                conn_map.try_emplace(ConnectionStringHelper::GetRealKeyName(key), val);
            }
        }
    }
}

std::string OdbcDsnHelper::ResolveDriverName(const std::string &driver_name)
{
    int size = 0;

#ifdef UNICODE
    std::vector<uint16_t> driver_name_vec = ConvertUTF8ToUTF16(driver_name);
    std::vector<uint16_t> entry_key_vec = ConvertUTF8ToUTF16(KEY_DRIVER);
    std::vector<uint16_t> empty_vec = ConvertUTF8ToUTF16("");
    std::vector<uint16_t> odbcinst_ini_vec = ConvertUTF8ToUTF16(ODBCINST_INI);
    odbcinst_ini_vec.push_back(0);

    std::vector<uint16_t> buffer(MAX_VAL_SIZE, 0);
    size = SQLGetPrivateProfileString(reinterpret_cast<SQLWCHAR*>(driver_name_vec.data()), reinterpret_cast<SQLWCHAR*>(entry_key_vec.data()), reinterpret_cast<SQLWCHAR*>(empty_vec.data()), reinterpret_cast<SQLWCHAR*>(buffer.data()), MAX_VAL_SIZE, reinterpret_cast<SQLWCHAR*>(odbcinst_ini_vec.data()));

    if (size < 1) {
        LOG(WARNING) << "No driver registered under name: " << driver_name;
        return {};
    }
    // Guarantee NUL termination in case the driver manager filled the buffer.
    buffer.back() = 0;
    return ConvertUTF16ToUTF8(buffer.data());
#else
    std::vector<char> buffer(MAX_VAL_SIZE, '\0');
    size = SQLGetPrivateProfileString(driver_name.c_str(), KEY_DRIVER, EMPTY_RDS_STR, buffer.data(), MAX_VAL_SIZE, ODBCINST_INI);

    if (size < 1) {
        LOG(WARNING) << "No driver registered under name: " << driver_name;
        return {};
    }
    buffer.back() = '\0';
    return { buffer.data() };
#endif
}

std::string OdbcDsnHelper::Load(const std::string &dsn_key, const std::string &entry_key)
{
    int size = 0;

#ifdef UNICODE
    std::vector<uint16_t> dsn_key_vec = ConvertUTF8ToUTF16(dsn_key);
    std::vector<uint16_t> entry_key_vec = ConvertUTF8ToUTF16(entry_key);
    std::vector<uint16_t> empty_vec = ConvertUTF8ToUTF16("");
    std::vector<uint16_t> odbc_ini_vec = ConvertUTF8ToUTF16(ODBC_INI);
    odbc_ini_vec.push_back(0);

    std::vector<uint16_t> buffer(MAX_VAL_SIZE, 0);
    // Check DSN if it is valid and contains entries
    size = SQLGetPrivateProfileString(reinterpret_cast<SQLWCHAR*>(dsn_key_vec.data()), reinterpret_cast<SQLWCHAR*>(entry_key_vec.data()), reinterpret_cast<SQLWCHAR*>(empty_vec.data()), reinterpret_cast<SQLWCHAR*>(buffer.data()), MAX_VAL_SIZE, reinterpret_cast<SQLWCHAR*>(odbc_ini_vec.data()));

    // Guarantee NUL termination in case the driver manager filled the buffer
    buffer.back() = 0;
    std::string value = ConvertUTF16ToUTF8(buffer.data());
#else
    std::vector<char> buffer(MAX_VAL_SIZE, '\0');
    size = SQLGetPrivateProfileString(dsn_key.c_str(), entry_key.c_str(), EMPTY_RDS_STR, buffer.data(), MAX_VAL_SIZE, ODBC_INI);

    buffer.back() = '\0';
    std::string value(buffer.data());
#endif

    if (size < 0) {
        // No entries
        LOG(WARNING) << "No value found for DSN entry key: " << dsn_key << ", " << entry_key;
        return {};
    }
    return value;
}
