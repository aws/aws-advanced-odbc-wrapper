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

#ifndef DIALECT_AURORA_MYSQL_H
#define DIALECT_AURORA_MYSQL_H

#include <algorithm>

#include "dialect.h"

#include <vector>

#include "../util/rds_strings.h"
#include "../util/odbc_helper.h"

class DialectAuroraMySql : virtual public Dialect, DialectBlueGreen {
public:
    int GetDefaultPort() override { return DEFAULT_MYSQL_PORT; };
    std::string GetTopologyQuery() override { return TOPOLOGY_QUERY; };
    std::string GetWriterIdQuery() override { return WRITER_ID_QUERY; };
    std::string GetNodeIdQuery() override { return NODE_ID_QUERY; };
    std::string GetIsReaderQuery() override { return IS_READER_QUERY; };
    std::string GetBlueGreenStatusAvailableQuery() override { return BG_TOPOLOGY_EXISTS_QUERY; };
    std::string GetBlueGreenStatusQuery() override { return BG_STATUS_QUERY; };
    std::string GetSetReadOnlyQuery() override { return SET_READ_ONLY_QUERY; };
    std::string GetSetReadWriteQuery() override { return SET_READ_WRITE_QUERY; };
    DatabaseDialectType GetUpdateCandidate() override { return MULTI_AZ_MYSQL; };

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

    DatabaseDialectType GetDialectType() override { return DatabaseDialectType::AURORA_MYSQL; };

protected:
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
    const int DEFAULT_MYSQL_PORT = 3306;
    const std::string TOPOLOGY_QUERY =
        "SELECT SERVER_ID, CASE WHEN SESSION_ID = 'MASTER_SESSION_ID' THEN TRUE ELSE FALSE END, \
        CPU, REPLICA_LAG_IN_MILLISECONDS, LAST_UPDATE_TIMESTAMP \
        FROM information_schema.replica_host_status \
        WHERE time_to_sec(timediff(now(), LAST_UPDATE_TIMESTAMP)) <= 300 OR SESSION_ID = 'MASTER_SESSION_ID'";

    const std::string WRITER_ID_QUERY =
        "SELECT SERVER_ID FROM information_schema.replica_host_status \
        WHERE SESSION_ID = 'MASTER_SESSION_ID' AND SERVER_ID = @@aurora_server_id";

    const std::string NODE_ID_QUERY = "SELECT @@aurora_server_id";

    const std::string IS_READER_QUERY = "SELECT @@innodb_read_only";

    const std::string BG_TOPOLOGY_EXISTS_QUERY =
        "SELECT 1 AS tmp FROM information_schema.tables WHERE \
        table_schema = 'mysql' AND table_name = 'rds_topology'";

    const std::string BG_STATUS_QUERY =
        "SELECT * FROM mysql.rds_topology";

    const std::string SET_READ_ONLY_QUERY = "SET SESSION TRANSACTION READ ONLY";

    const std::string SET_READ_WRITE_QUERY = "SET SESSION TRANSACTION READ WRITE";

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
        "XX",       // internal error (backend)
        "HYT00",    // connection/login timeout expired
        "HY000"     // generic error class used for can't-connect / unknown-host failures
    };
};

class DialectMultiAzClusterMySql : public DialectMultiAzCluster, public DialectAuroraMySql {
public:
    DatabaseDialectType GetDialectType() override { return DatabaseDialectType::MULTI_AZ_MYSQL; };
    std::string GetWriterIdColumnName() override { return WRITER_ID_QUERY_COLUMN_NAME; };
    std::string GetReplicaSourceQuery() override { return REPLICA_SOURCE_QUERY; };
    std::string GetIsReaderQuery() override { return IS_READER_QUERY; };
    std::string GetTopologyQuery() override { return TOPOLOGY_QUERY; };
    std::string GetNodeIdQuery() override { return NODE_ID_QUERY; };
    DatabaseDialectType GetUpdateCandidate() override { return UNKNOWN_DIALECT; };

    bool IsDialect(DBC* dbc, std::shared_ptr<OdbcHelper> odbc_helper) override {
        SQLHSTMT stmt = SQL_NULL_HANDLE;
        SQLTCHAR query_res[BUFFER_SIZE * 2] = {0};
        SQLLEN len = 0;
        odbc_helper->BaseAllocStmt(&dbc->wrapped_dbc, &stmt);
        bool is_dialect = false;
        if (const RdsLibResult res = odbc_helper->ExecDirect(&stmt, REPORT_HOST_EXISTS_QUERY); SQL_SUCCEEDED(res.fn_result)) {
            odbc_helper->BindCol(&stmt, 2, SQL_C_TCHAR, &query_res, BUFFER_SIZE, &len);
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
    const std::string REPORT_HOST_EXISTS_QUERY = "SHOW VARIABLES LIKE 'report_host'";
    const std::string TOPOLOGY_QUERY = "SELECT id, endpoint, port FROM mysql.rds_topology";
    const std::string NODE_ID_QUERY = "SELECT @@server_id";
    const std::string IS_READER_QUERY = "SELECT @@read_only";
    const std::string REPLICA_SOURCE_QUERY = "SHOW REPLICA STATUS";
    const std::string WRITER_ID_QUERY_COLUMN_NAME = "Source_Server_Id";
};

#endif // DIALECT_AURORA_MYSQL_H
