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
    RDS_CHAR buffer[MAX_VAL_SIZE];
    RDS_CHAR *entries = buffer;
    int size = 0;

#ifdef UNICODE
    #include "unicode/utypes.h"
    icu_77::StringPiece dsn_key_string_piece(dsn_key.c_str());
    icu_77::UnicodeString dsn_key_utf16 = icu_77::UnicodeString::fromUTF8(dsn_key_string_piece);
    const char16_t *dsn_key_ushort = dsn_key_utf16.getBuffer();

    // Check DSN if it is valid and contains entries
    size = SQLGetPrivateProfileString(dsn_key_ushort, nullptr, EMPTY_RDS_STR, buffer, MAX_VAL_SIZE, ODBC_INI);
#else
    size = SQLGetPrivateProfileString(dsn_key.c_str(), nullptr, EMPTY_RDS_STR, buffer, MAX_VAL_SIZE, ODBC_INI);
#endif
    if (size < 1) {
        // No entries in DSN
        // TODO - Error handling?
        LOG(WARNING) << "No DSN entry found for: " << ToStr(dsn_key);
        return;
    }

    // Load entries into map
    for (size_t used = 0; used < MAX_VAL_SIZE && entries[0];
        used += RDS_STR_LEN(entries) + 1, entries += RDS_STR_LEN(entries) + 1)
    {
        RDS_STR key(entries);
        RDS_STR_UPPER(key);
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
    RDS_CHAR buffer[MAX_VAL_SIZE];
    int size = 0;
    size = SQLGetPrivateProfileString(dsn_key.c_str(), entry_key.c_str(), EMPTY_RDS_STR, buffer, MAX_VAL_SIZE, ODBC_INI);
    if (size < 0) {
        // No entries
        LOG(WARNING) << "No value found for DSN entry key: " << ToStr(dsn_key) << ", " << ToStr(entry_key);
        return RDS_STR();
    }
    return RDS_STR(buffer);
}
