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

class DialectAuroraMySql : virtual public Dialect {
public:
    int GetDefaultPort() override { return DEFAULT_MYSQL_PORT; };
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

#endif // DIALECT_AURORA_MYSQL_H
