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
#include "../util/connection_string_keys.h"
#include "../util/connection_string_helper.h"

void OdbcDsnHelper::LoadAll(const RDS_STR &dsn_key, std::map<RDS_STR, RDS_STR> &conn_map)
{
    int size = 0;

#ifdef UNICODE
    icu::StringPiece dsn_key_string_piece(dsn_key.c_str());
    icu::UnicodeString dsn_key_utf16 = icu::UnicodeString::fromUTF8(dsn_key_string_piece);
    unsigned short *dsn_key_ushort = (unsigned short *)(dsn_key_utf16.getBuffer());

    unsigned short buffer_utf16[MAX_VAL_SIZE];
    icu::StringPiece empty_string_piece("");
    icu::UnicodeString empty_string_utf16 = icu::UnicodeString::fromUTF8(empty_string_piece);
    unsigned short *empty = (unsigned short *)(empty_string_utf16.getBuffer());
    icu::StringPiece odbc_ini_piece(ODBC_INI);
    icu::UnicodeString odbc_ini_utf16 = icu::UnicodeString::fromUTF8(odbc_ini_piece);
    unsigned short *odbc_ini = (unsigned short *)(odbc_ini_utf16.getBuffer());

    // Check DSN if it is valid and contains entries
    size = SQLGetPrivateProfileString(dsn_key_ushort, nullptr, empty, buffer_utf16, MAX_VAL_SIZE, odbc_ini);
    char buffer[MAX_VAL_SIZE];
    icu::UnicodeString unicode_str(reinterpret_cast<const char16_t*>(buffer_utf16));
    std::string buffer_utf8;
    unicode_str.toUTF8String(buffer_utf8);
    std::copy(buffer_utf8.begin(), buffer_utf8.end(), buffer);
#else
    RDS_CHAR buffer[MAX_VAL_SIZE];
    size = SQLGetPrivateProfileString(dsn_key.c_str(), nullptr, EMPTY_RDS_STR, buffer, MAX_VAL_SIZE, ODBC_INI);
#endif
    RDS_CHAR *entries = buffer;

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
        RDS_STR raw_key(entries);
        std::string key = RDS_STR_UPPER(raw_key);
        RDS_STR val = Load(dsn_key, key);

        // Insert if value exists
        if (!val.empty()) {
            if (key.compare(KEY_BASE_CONN) == 0) {
                std::map<RDS_STR, RDS_STR> base_conn_map;
                ConnectionStringHelper::ParseConnectionString(val, base_conn_map);
                for (const auto& pair : base_conn_map) {
                    RDS_STR base_conn_val = pair.second;
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

RDS_STR OdbcDsnHelper::Load(const RDS_STR &dsn_key, const RDS_STR &entry_key)
{
    int size = 0;

#ifdef UNICODE
    icu::StringPiece dsn_key_string_piece(dsn_key.c_str());
    icu::UnicodeString dsn_key_utf16 = icu::UnicodeString::fromUTF8(dsn_key_string_piece);
    unsigned short *dsn_key_ushort = (unsigned short *)(dsn_key_utf16.getBuffer());

    icu::StringPiece entry_key_string_piece(entry_key.c_str());
    icu::UnicodeString entry_key_utf16 = icu::UnicodeString::fromUTF8(entry_key_string_piece);
    unsigned short *entry_key_ushort = (unsigned short *)(entry_key_utf16.getBuffer());

    unsigned short buffer_utf16[MAX_VAL_SIZE];
    icu::StringPiece empty_string_piece("");
    icu::UnicodeString empty_string_utf16 = icu::UnicodeString::fromUTF8(empty_string_piece);
    unsigned short *empty = (unsigned short *)(empty_string_utf16.getBuffer());
    icu::StringPiece odbc_ini_piece(ODBC_INI);
    icu::UnicodeString odbc_ini_utf16 = icu::UnicodeString::fromUTF8(odbc_ini_piece);
    unsigned short *odbc_ini = (unsigned short *)(odbc_ini_utf16.getBuffer());

    // Check DSN if it is valid and contains entries
    size = SQLGetPrivateProfileString(dsn_key_ushort, nullptr, empty, buffer_utf16, MAX_VAL_SIZE, odbc_ini);

    char buffer[MAX_VAL_SIZE];
    icu::UnicodeString unicode_str(reinterpret_cast<const char16_t*>(buffer_utf16));
    std::string buffer_utf8;
    unicode_str.toUTF8String(buffer_utf8);
    std::copy(buffer_utf8.begin(), buffer_utf8.end(), buffer);
#else
    RDS_CHAR buffer[MAX_VAL_SIZE];
    size = SQLGetPrivateProfileString(dsn_key.c_str(), entry_key.c_str(), EMPTY_RDS_STR, buffer, MAX_VAL_SIZE, ODBC_INI);
#endif

    if (size < 0) {
        // No entries
        LOG(WARNING) << "No value found for DSN entry key: " << dsn_key << ", " << entry_key;
        return RDS_STR();
    }
    return buffer;
}
