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
#include "../util/odbc_helper.h"

class DialectAuroraPostgres : virtual public Dialect, DialectBlueGreen {
public:
    int GetDefaultPort() override { return DEFAULT_POSTGRES_PORT; };
    std::string GetTopologyQuery() override { return TOPOLOGY_QUERY; };
    std::string GetWriterIdQuery() override { return WRITER_ID_QUERY; };
    std::string GetNodeIdQuery() override { return NODE_ID_QUERY; };
    std::string GetIsReaderQuery() override { return IS_READER_QUERY; };
    std::string GetBlueGreenStatusAvailableQuery() override { return BG_TOPOLOGY_EXISTS_QUERY; };
    std::string GetBlueGreenStatusQuery() override { return BG_STATUS_QUERY; };
    std::string GetSetReadOnlyQuery() override { return SET_READ_ONLY_QUERY; };
    std::string GetSetReadWriteQuery() override { return SET_READ_WRITE_QUERY; };
    DatabaseDialectType GetUpdateCandidate() override { return MULTI_AZ_PG; };

    bool IsSqlStateAccessError(const char* sql_state) override {
        std::string state(sql_state);
        return std::ranges::any_of(ACCESS_ERRORS, [&state](const std::string &prefix) {
            return state.rfind(prefix, 0) == 0;
        });
    };

    bool IsSqlStateAccessError(const char* sql_state, const std::string& error_message) override {
        if (IsSqlStateAccessError(sql_state)) {
            return true;
        }
        // psqlODBC wraps PAM authentication failures in 08001 (connection exception)
        // instead of 28P01/28000. Check the error message for auth-related keywords.
        if (!error_message.empty()
            && (error_message.find("PAM authentication failed") != std::string::npos
                || error_message.find("password authentication failed") != std::string::npos))
        {
            return true;
        }
        return false;
    };

    bool IsSqlStateNetworkError(const char* sql_state) override {
        std::string state(sql_state);
        return std::ranges::any_of(NETWORK_ERRORS, [&state](const std::string &prefix) {
            return state.rfind(prefix, 0) == 0;
        });
    };

    virtual DatabaseDialectType GetDialectType() override { return DatabaseDialectType::AURORA_POSTGRESQL; };

    std::optional<bool> DoesStatementSetReadOnly(std::string statement) override {
        if (statement.starts_with(SET_READ_ONLY_QUERY)) {
            return true;
        }
        if (statement.starts_with(SET_READ_WRITE_QUERY)) {
            return false;
        }
        return {};
    }

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

    const std::string BG_TOPOLOGY_EXISTS_QUERY =
        "SELECT 'pg_catalog.get_blue_green_fast_switchover_metadata'::regproc";

    // "id", "endpoint", "port", "role", "status", "version", "update_stamp"
    const std::string BG_STATUS_QUERY =
        "SELECT * FROM pg_catalog.get_blue_green_fast_switchover_metadata('aws_odbc_driver-1.1.0')";

    const std::string SET_READ_ONLY_QUERY = "SET SESSION CHARACTERISTICS AS TRANSACTION READ ONLY";

    const std::string SET_READ_WRITE_QUERY = "SET SESSION CHARACTERISTICS AS TRANSACTION READ WRITE";

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

class DialectMultiAzClusterPostgres : public DialectMultiAzCluster, public DialectAuroraPostgres {
public:
    DatabaseDialectType GetDialectType() override { return DatabaseDialectType::MULTI_AZ_PG; }
    std::string GetWriterIdColumnName() override { return WRITER_ID_QUERY_COLUMN_NAME; };
    std::string GetReplicaSourceQuery() override { return REPLICA_SOURCE_QUERY; };
    std::string GetTopologyQuery() override { return TOPOLOGY_QUERY; };
    std::string GetNodeIdQuery() override { return NODE_ID_QUERY; };
    DatabaseDialectType GetUpdateCandidate() override { return UNKNOWN_DIALECT; };

    bool IsDialect(DBC* dbc, std::shared_ptr<OdbcHelper> odbc_helper) override {
        SQLHSTMT stmt = SQL_NULL_HANDLE;
        SQLTCHAR query_res[BUFFER_SIZE * 2] = {0};
        SQLLEN len = 0;
        odbc_helper->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);
        bool is_dialect = false;
        if (const RdsLibResult res = odbc_helper->ExecDirect(&stmt, IS_RDS_CLUSTER_QUERY); SQL_SUCCEEDED(res.fn_result)) {
            odbc_helper->BindCol(&stmt, 1, SQL_C_TCHAR, &query_res, BUFFER_SIZE, &len);
            odbc_helper->Fetch(&stmt);
#ifdef UNICODE
            Convert4To2ByteString(odbc_helper->GetUse4BytesBaseDriver(), query_res, nullptr, BUFFER_SIZE);
#endif
            const std::string query_res_str(AS_UTF8_CSTR(query_res));
            if (!query_res_str.empty()) {
                is_dialect = true;
            }
        }
        odbc_helper->BaseFreeStmt(&stmt);
        return is_dialect;
    }

private:
    const std::string IS_RDS_CLUSTER_QUERY = "SELECT multi_az_db_cluster_source_dbi_resource_id FROM rds_tools.multi_az_db_cluster_source_dbi_resource_id()";
    const std::string TOPOLOGY_QUERY = "SELECT id, endpoint, port FROM rds_tools.show_topology()";
    const std::string NODE_ID_QUERY = "SELECT id, SUBSTRING(endpoint FROM 0 FOR POSITION('.' IN endpoint)) FROM rds_tools.show_topology() WHERE id OPERATOR(pg_catalog.=) rds_tools.dbi_resource_id()";
    const std::string REPLICA_SOURCE_QUERY = "SELECT multi_az_db_cluster_source_dbi_resource_id FROM rds_tools.multi_az_db_cluster_source_dbi_resource_id() " \
        "WHERE multi_az_db_cluster_source_dbi_resource_id OPERATOR(pg_catalog.!=) " \
        "(SELECT dbi_resource_id FROM rds_tools.dbi_resource_id())";
    const std::string WRITER_ID_QUERY_COLUMN_NAME = "multi_az_db_cluster_source_dbi_resource_id";
};

#endif  // DIALECT_AURORA_POSTGRES_H
