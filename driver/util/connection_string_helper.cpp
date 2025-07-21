#include "connection_string_helper.h"

// TODO - Will need to modify this function to only put if absent
//   Ideally we would still want this at the lowest level, as the base plugin
//   But since we will have other plugins such as IAM replacing the PWD,
//   we don't want the DSN to replace that generated token
std::map<RDS_STR, RDS_STR> ConnectionStringHelper::ParseConnectionString(const RDS_STR &conn_str)
{
    RDS_REGEX pattern(TEXT("([^;=]+)=([^;]+)"));
    RDS_MATCH match;
    RDS_STR conn_str_itr = conn_str;

    std::map<RDS_STR, RDS_STR> conn_map;

    while (std::regex_search(conn_str_itr, match, pattern)) {
        RDS_STR key = match[1].str();
        std::transform(key.begin(), key.end(), key.begin(), [](RDS_CHAR c) {
            return TO_UPPER(c);
        });
        RDS_STR val = match[2].str();
        conn_map[key] = val;

        conn_str_itr = match.suffix().str();
    }

    return conn_map;
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
