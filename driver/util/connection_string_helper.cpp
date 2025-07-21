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
