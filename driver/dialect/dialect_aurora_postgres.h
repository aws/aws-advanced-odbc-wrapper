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

#ifndef DIALECT_AURORA_POSTGRES_H
#define DIALECT_AURORA_POSTGRES_H

#include "dialect.h"

#include <vector>

#include "../util/rds_strings.h"

class DialectAuroraPostgres : public Dialect {
public:
    int GetDefaultPort() override { return DEFAULT_POSTGRES_PORT; };
    RDS_STR GetTopologyQuery() override { return TOPOLOGY_QUERY; };
    RDS_STR GetWriterIdQuery() override { return WRITER_ID_QUERY; };
    RDS_STR GetNodeIdQuery() override { return NODE_ID_QUERY; };
    RDS_STR GetIsReaderQuery() override { return IS_READER_QUERY; };

    bool IsSqlStateAccessError(RDS_CHAR* sql_state) override {
        RDS_STR state(sql_state);
        for (RDS_STR prefix : ACCESS_ERRORS) {
            if (state.rfind(prefix, 0) == 0) return true;
        }
        return false;
    };
    bool IsSqlStateNetworkError(RDS_CHAR* sql_state) override {
        RDS_STR state(sql_state);
        for (RDS_STR prefix : NETWORK_ERRORS) {
            if (state.rfind(prefix, 0) == 0) return true;
        }
        return false;
    };

private:
    const int DEFAULT_POSTGRES_PORT = 5432;
    const RDS_STR TOPOLOGY_QUERY = AS_RDS_STR(TEXT(
        "SELECT SERVER_ID, CASE WHEN SESSION_ID = 'MASTER_SESSION_ID' THEN TRUE ELSE FALSE END, \
        CPU, COALESCE(REPLICA_LAG_IN_MSEC, 0) \
        FROM aurora_replica_status() \
        WHERE EXTRACT(EPOCH FROM(NOW() - LAST_UPDATE_TIMESTAMP)) <= 300 OR SESSION_ID = 'MASTER_SESSION_ID' \
        OR LAST_UPDATE_TIMESTAMP IS NULL"));

    const RDS_STR WRITER_ID_QUERY = AS_RDS_STR(TEXT(
        "SELECT SERVER_ID FROM aurora_replica_status() WHERE SESSION_ID = 'MASTER_SESSION_ID' \
        AND SERVER_ID = aurora_db_instance_identifier()"));

    const RDS_STR NODE_ID_QUERY = AS_RDS_STR(TEXT("SELECT aurora_db_instance_identifier()"));

    const RDS_STR IS_READER_QUERY = AS_RDS_STR(TEXT("SELECT pg_is_in_recovery()"));

    const std::vector<RDS_STR> ACCESS_ERRORS = {
        AS_RDS_STR(TEXT("28P01")),
        AS_RDS_STR(TEXT("28000"))   // PAM authentication errors
    };

    const std::vector<RDS_STR> NETWORK_ERRORS = {
        AS_RDS_STR(TEXT("53")),       // insufficient resources
        AS_RDS_STR(TEXT("57P01")),    // admin shutdown
        AS_RDS_STR(TEXT("57P02")),    // crash shutdown
        AS_RDS_STR(TEXT("57P03")),    // cannot connect now
        AS_RDS_STR(TEXT("58")),       // system error (backend)
        AS_RDS_STR(TEXT("08")),       // connection error
        AS_RDS_STR(TEXT("99")),       // unexpected error
        AS_RDS_STR(TEXT("F0")),       // configuration file error (backend)
        AS_RDS_STR(TEXT("XX"))        // internal error (backend)
    };
};

#endif  // DIALECT_AURORA_POSTGRES_H
