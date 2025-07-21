#ifndef CONNECTION_STRING_HELPER_H_
#define CONNECTION_STRING_HELPER_H_

#include <map>

#include "rds_strings.h"

namespace ConnectionStringHelper {
    void ParseConnectionString(const RDS_STR &conn_str, std::map<RDS_STR, RDS_STR> &conn_map);
    RDS_STR BuildConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map);
}

#endif // CONNECTION_STRING_HELPER_H_
