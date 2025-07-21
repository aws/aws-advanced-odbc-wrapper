#ifndef ODBC_DSN_HELPER_H_
#define ODBC_DSN_HELPER_H_


#include <map>

#include "rds_strings.h"

#define MAX_KEY_SIZE 8192
#define MAX_VAL_SIZE 8192
#define ODBC_INI TEXT("ODBC.INI")
#define ODBCINST_INI TEXT("ODBCINST.INI")

namespace OdbcDsnHelper {
    void LoadAll(const RDS_STR &dsn_key, std::map<RDS_STR, RDS_STR> &conn_map);
    RDS_STR Load(const RDS_STR &dsn_key, const RDS_STR &entry_key);
}

#endif // ODBC_DSN_HELPER_H_
