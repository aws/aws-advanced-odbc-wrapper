#include "connection_string_helper.h"

#include "connection_string_keys.h"

void ConnectionStringHelper::ParseConnectionString(const RDS_STR &conn_str, std::map<RDS_STR, RDS_STR> &conn_map)
{
    RDS_REGEX pattern(TEXT("([^;=]+)=([^;]+)"));
    RDS_MATCH match;
    RDS_STR conn_str_itr = conn_str;

    while (std::regex_search(conn_str_itr, match, pattern)) {
        RDS_STR key = match[1].str();
        std::transform(key.begin(), key.end(), key.begin(), [](RDS_CHAR c) {
            return TO_UPPER(c);
        });
        RDS_STR val = match[2].str();

        // Insert if absent
        conn_map.try_emplace(key, val);
        conn_str_itr = match.suffix().str();
    }

    // Remove original connection's DSN & Driver
    // We don't want the underlying connection
    //  to look back to the wrapper
    conn_map.erase(KEY_DRIVER);
    conn_map.erase(KEY_DSN);

    // Set the DSN use the Base
    auto map_end_itr = conn_map.end();
    if (conn_map.find(KEY_BASE_DSN) != map_end_itr) {
        conn_map.insert_or_assign(KEY_DSN, conn_map.at(KEY_BASE_DSN));
    }
    // Set to use Base Driver if Base DSN is empty
    //  Base DSN should already contain the underlying driver
    else if (conn_map.find(KEY_BASE_DRIVER) != map_end_itr) {
        conn_map.insert_or_assign(KEY_DRIVER, conn_map.at(KEY_BASE_DRIVER));
    }
}

RDS_STR ConnectionStringHelper::BuildConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map)
{
    RDS_STR_STREAM conn_stream;
    for (const auto& e : conn_map) {
        if (conn_stream.tellp() > 0) {
            conn_stream << TEXT(";");
        }
        conn_stream << e.first << TEXT("=") << e.second;
    }
    return conn_stream.str();
}
