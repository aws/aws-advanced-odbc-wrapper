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

#include "logger_wrapper.h"

#include "../driver.h"
#include "../util/connection_string_helper.h"
#include "../util/connection_string_keys.h"

void OdbcDsnHelper::LoadAll(const std::string &dsn_key, std::map<std::string, std::string> &conn_map)
{
    int size = 0;

#ifdef UNICODE
    std::vector<uint16_t> dsn_key_vec = ConvertUTF8ToUTF16(dsn_key);
    uint16_t *dsn_key_ushort = dsn_key_vec.data();

    std::vector<uint16_t> empty_vec = ConvertUTF8ToUTF16("");
    uint16_t *empty = empty_vec.data();
    std::vector<uint16_t> odbc_ini_vec = ConvertUTF8ToUTF16(ODBC_INI);
    odbc_ini_vec.push_back(0);
    uint16_t *odbc_ini = odbc_ini_vec.data();

    uint16_t buffer_utf16[MAX_VAL_SIZE];
    // Check DSN if it is valid and contains entries
    size = SQLGetPrivateProfileString(reinterpret_cast<SQLWCHAR*>(dsn_key_ushort), nullptr, reinterpret_cast<SQLWCHAR*>(empty), reinterpret_cast<SQLWCHAR*>(buffer_utf16), MAX_VAL_SIZE, reinterpret_cast<SQLWCHAR*>(odbc_ini));
    std::vector<std::string> entries_vec;
    uint16_t *buffer_utf16_ptr = buffer_utf16;
    while(*buffer_utf16_ptr) {
        entries_vec.push_back(ConvertUTF16ToUTF8(buffer_utf16_ptr));
        buffer_utf16_ptr += entries_vec.back().size() + 1;
    }
    char buffer[MAX_VAL_SIZE];
    char* buf_ptr = buffer;
    for (std::string s : entries_vec) {
        std::copy(s.begin(), s.end(), buf_ptr);
        buf_ptr += strlen(s.c_str());
        *buf_ptr = '\0';
        buf_ptr += 1;
    }
    *buf_ptr = '\0';
#else
    RDS_CHAR buffer[MAX_VAL_SIZE];
    size = SQLGetPrivateProfileString(dsn_key.c_str(), nullptr, EMPTY_RDS_STR, buffer, MAX_VAL_SIZE, ODBC_INI);
#endif
    const RDS_CHAR *entries = buffer;
    if (size < 1) {
        // No entries in DSN
        // TODO - Error handling?
        LOG(WARNING) << "No DSN entry found for: " << dsn_key;
        return;
    }

    // Load entries into map
    for (size_t used = 0; used < MAX_VAL_SIZE && entries[0];
        used += RDS_STR_LEN(entries) + 1, entries += RDS_STR_LEN(entries) + 1)
    {
        const std::string raw_key(entries);
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
                conn_map.try_emplace(key, val);
            }
        }
    }
}

std::string OdbcDsnHelper::Load(const std::string &dsn_key, const std::string &entry_key)
{
    int size = 0;

#ifdef UNICODE
    std::vector<uint16_t> dsn_key_vec = ConvertUTF8ToUTF16(dsn_key);
    uint16_t *dsn_key_ushort = dsn_key_vec.data();

    std::vector<uint16_t> entry_key_vec = ConvertUTF8ToUTF16(entry_key);
    uint16_t *entry_key_ushort = entry_key_vec.data();

    std::vector<uint16_t> empty_vec = ConvertUTF8ToUTF16("");
    uint16_t *empty = empty_vec.data();
    std::vector<uint16_t> odbc_ini_vec = ConvertUTF8ToUTF16(ODBC_INI);
    odbc_ini_vec.push_back(0);
    uint16_t *odbc_ini = odbc_ini_vec.data();

    uint16_t buffer_utf16[MAX_VAL_SIZE];
    // Check DSN if it is valid and contains entries
    size = SQLGetPrivateProfileString(reinterpret_cast<SQLWCHAR*>(dsn_key_ushort), reinterpret_cast<SQLWCHAR*>(entry_key_ushort), reinterpret_cast<SQLWCHAR*>(empty), reinterpret_cast<SQLWCHAR*>(buffer_utf16), MAX_VAL_SIZE, reinterpret_cast<SQLWCHAR*>(odbc_ini));

    std::string buffer_utf8 = ConvertUTF16ToUTF8(buffer_utf16);
    char buffer[MAX_VAL_SIZE];
    std::copy(buffer_utf8.begin(), buffer_utf8.end(), buffer);
    buffer[buffer_utf8.length()] = '\0';
#else
    RDS_CHAR buffer[MAX_VAL_SIZE];
    size = SQLGetPrivateProfileString(dsn_key.c_str(), entry_key.c_str(), EMPTY_RDS_STR, buffer, MAX_VAL_SIZE, ODBC_INI);
#endif

    if (size < 0) {
        // No entries
        LOG(WARNING) << "No value found for DSN entry key: " << dsn_key << ", " << entry_key;
        return {};
    }
    return { buffer };
}
