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

#ifndef DIALECT_H
#define DIALECT_H

#include <map>
#include <memory>
#include <string>

#include "../util/connection_string_keys.h"
#include "../util/rds_strings.h"

struct DBC;
class OdbcHelper;

typedef enum {
    AURORA_POSTGRESQL,
    AURORA_POSTGRESQL_LIMITLESS,
    AURORA_MYSQL,
    MULTI_AZ_MYSQL,
    MULTI_AZ_PG,
    UNKNOWN_DIALECT
} DatabaseDialectType;

static std::map<std::string, DatabaseDialectType> const database_dialect_table = {
    {VALUE_DB_DIALECT_AURORA_POSTGRESQL,            DatabaseDialectType::AURORA_POSTGRESQL},
    {VALUE_DB_DIALECT_AURORA_POSTGRESQL_LIMITLESS,  DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS},
    {VALUE_DB_DIALECT_AURORA_MYSQL,                 DatabaseDialectType::AURORA_MYSQL},
    {VALUE_DB_DIALECT_MULTI_AZ_MYSQL,               DatabaseDialectType::MULTI_AZ_MYSQL},
    {VALUE_DB_DIALECT_MULTI_AZ_PG,                  DatabaseDialectType::MULTI_AZ_PG}
};

class Dialect {
public:
    virtual int GetDefaultPort() { return 0; }
    virtual std::string GetWriterIdColumnName() { return ""; };
    virtual std::string GetTopologyQuery() { return ""; };
    virtual std::string GetWriterIdQuery() { return ""; };
    virtual std::string GetReplicaSourceQuery() { return ""; };
    virtual std::string GetNodeIdQuery() { return ""; };
    virtual std::string GetIsReaderQuery() { return ""; };
    virtual std::string GetSetReadOnlyQuery() { return ""; };
    virtual std::string GetSetReadWriteQuery() { return ""; };
    virtual DatabaseDialectType GetUpdateCandidate() { return UNKNOWN_DIALECT; };

    virtual bool IsSqlStateAccessError(const char* sql_state) { return false; };
    virtual bool IsSqlStateAccessError(const char* sql_state, const std::string& error_message) {
        return IsSqlStateAccessError(sql_state);
    };
    virtual bool IsSqlStateNetworkError(const char* sql_state) { return false; };

    virtual DatabaseDialectType GetDialectType() { return DatabaseDialectType::UNKNOWN_DIALECT; };
    virtual bool IsDialect(DBC* dbc, std::shared_ptr<OdbcHelper> odbc_helper) { return true; };

    virtual std::optional<bool> DoesStatementSetReadOnly(std::string statement) { return {}; };

    static DatabaseDialectType DatabaseDialectFromString(const std::string &database_dialect) {
        std::string local_str = database_dialect;
        std::string upper_local_str = RDS_STR_UPPER(local_str);
        if (database_dialect_table.contains(upper_local_str)) {
            return database_dialect_table.at(upper_local_str);
        }
        return DatabaseDialectType::UNKNOWN_DIALECT;
    }

    static constexpr int BUFFER_SIZE = 1024;
};

class DialectLimitless : virtual public Dialect {
public:
    virtual std::string GetLimitlessRouterEndpointQuery() { return ""; };
};

class DialectBlueGreen : virtual public Dialect {
public:
    virtual std::string GetBlueGreenStatusAvailableQuery() { return ""; };
    virtual std::string GetBlueGreenStatusQuery() { return ""; };
};

class DialectMultiAzCluster : virtual public Dialect {
public:
    virtual std::string GetWriterIdColumnName() { return ""; };
};

#endif // DIALECT_H
