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

#include <algorithm>

#include "dialect.h"

#include <vector>

#include "../util/rds_strings.h"

class DialectAuroraPostgres : virtual public Dialect {
public:
    int GetDefaultPort() override { return DEFAULT_POSTGRES_PORT; };
    std::string GetTopologyQuery() override { return TOPOLOGY_QUERY; };
    std::string GetWriterIdQuery() override { return WRITER_ID_QUERY; };
    std::string GetNodeIdQuery() override { return NODE_ID_QUERY; };
    std::string GetIsReaderQuery() override { return IS_READER_QUERY; };

    bool IsSqlStateAccessError(const char* sql_state) override {
        std::string state(sql_state);
        return std::ranges::any_of(ACCESS_ERRORS, [&state](const std::string &prefix) {
            return state.rfind(prefix, 0) == 0;
        });
    };

    bool IsSqlStateNetworkError(const char* sql_state) override {
        std::string state(sql_state);
        return std::ranges::any_of(NETWORK_ERRORS, [&state](const std::string &prefix) {
            return state.rfind(prefix, 0) == 0;
        });
    };

    virtual DatabaseDialectType GetDialectType() override { return DatabaseDialectType::AURORA_POSTGRESQL; };

private:
    const int DEFAULT_POSTGRES_PORT = 5432;
    const std::string TOPOLOGY_QUERY =
        "SELECT SERVER_ID, CASE WHEN SESSION_ID operator(pg_catalog.=) 'MASTER_SESSION_ID' THEN TRUE ELSE FALSE END, \
        CPU, COALESCE(REPLICA_LAG_IN_MSEC, 0) \
        FROM pg_catalog.aurora_replica_status() \
        WHERE EXTRACT(EPOCH FROM(pg_catalog.NOW() operator(pg_catalog.-) LAST_UPDATE_TIMESTAMP)) operator(pg_catalog.<=) 300 OR SESSION_ID operator(pg_catalog.=) 'MASTER_SESSION_ID' \
        OR LAST_UPDATE_TIMESTAMP IS NULL";

    const std::string WRITER_ID_QUERY =
        "SELECT SERVER_ID FROM pg_catalog.aurora_replica_status() WHERE SESSION_ID operator(pg_catalog.=) 'MASTER_SESSION_ID' \
        AND SERVER_ID operator(pg_catalog.=) pg_catalog.aurora_db_instance_identifier()";

    const std::string NODE_ID_QUERY = "SELECT pg_catalog.aurora_db_instance_identifier()";

    const std::string IS_READER_QUERY = "SELECT pg_catalog.pg_is_in_recovery()";

    const std::vector<std::string> ACCESS_ERRORS = {
        "28P01",
        "28000"   // PAM authentication errors
    };

    const std::vector<std::string> NETWORK_ERRORS = {
        "53",       // insufficient resources
        "57P01",    // admin shutdown
        "57P02",    // crash shutdown
        "57P03",    // cannot connect now
        "58",       // system error (backend)
        "08",       // connection error
        "99",       // unexpected error
        "F0",       // configuration file error (backend)
        "XX"        // internal error (backend)
    };
};

class DialectAuroraPostgresLimitless : public DialectLimitless, public DialectAuroraPostgres {
public:
    std::string GetLimitlessRouterEndpointQuery() override { return LIMITLESS_ROUTER_ENDPOINT_QUERY; };
    DatabaseDialectType GetDialectType() override { return DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS; };

private:
    const std::string LIMITLESS_ROUTER_ENDPOINT_QUERY = "SELECT router_endpoint, load FROM pg_catalog.aurora_limitless_router_endpoints()";
};

#endif  // DIALECT_AURORA_POSTGRES_H
