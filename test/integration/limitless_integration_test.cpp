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

#include <gtest/gtest.h>

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

// Generally accepted URL endpoint max length (2048) + 1 for null terminator
#define ROUTER_ENDPOINT_LENGTH  2049
#define LOAD_LENGTH             5
#define WEIGHT_SCALING          10
#define MAX_WEIGHT              10
#define MIN_WEIGHT              1

#define MONITOR_INTERVAL_MS     15000

#define NUM_CONNECTIONS_TO_OVERLOAD_ROUTER  20

class LimitlessIntegrationTest : public BaseConnectionTest {
public:
    std::string ROUTER_ENDPOINT_QUERY = "SELECT router_endpoint, load FROM aurora_limitless_router_endpoints()";
    std::string SELECT_QUERY = "SELECT 1";
    struct LimitlessHostInfo {
        std::string host_name;
        int64_t weight;
    };

    LimitlessHostInfo CreateHost(SQLTCHAR* load, SQLTCHAR* endpoint) {
        double load_num = std::strtod(STRING_HELPER::SqltcharToAnsi(load), nullptr);
        int64_t weight = WEIGHT_SCALING - std::floor(load_num * WEIGHT_SCALING);

        if (weight < MIN_WEIGHT || weight > MAX_WEIGHT) {
            weight = MIN_WEIGHT;
        }

        std::string router_endpoint_str(STRING_HELPER::SqltcharToAnsi(endpoint));

        return LimitlessHostInfo{
            .host_name = router_endpoint_str,
            .weight = weight
        };
    }

    std::vector<LimitlessHostInfo> QueryRouters() {
        SQLRETURN ret = SQL_ERROR;
        SQLHSTMT hstmt = SQL_NULL_HSTMT;

        ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
        SQLTCHAR router_endpoint_value[ROUTER_ENDPOINT_LENGTH] = {0};
        SQLLEN ind_router_endpoint_value = 0;

        SQLTCHAR load_value[LOAD_LENGTH] = {0};
        SQLLEN ind_load_value = 0;

        ret = SQLBindCol(hstmt, 1, SQL_C_TCHAR, &router_endpoint_value, sizeof(router_endpoint_value), &ind_router_endpoint_value);
        ret = SQLBindCol(hstmt, 2, SQL_C_TCHAR, &load_value, sizeof(load_value), &ind_load_value);

        ret = ODBC_HELPER::ExecuteQuery(hstmt, ROUTER_ENDPOINT_QUERY);
        std::vector<LimitlessHostInfo> limitless_routers;
        while (SQL_SUCCEEDED(ret = SQLFetch(hstmt))) {
            limitless_routers.push_back(CreateHost(load_value, router_endpoint_value));
        }

        ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);

        return limitless_routers;
    }

    std::vector<LimitlessHostInfo> SortedRoundRobin(const std::vector<LimitlessHostInfo> &hosts) {
        std::vector<LimitlessHostInfo> round_robin_hosts, all_hosts;
        std::copy(hosts.begin(), hosts.end(), std::back_inserter(all_hosts));

        // Sort
        struct {
            bool operator()(const LimitlessHostInfo& a, const LimitlessHostInfo& b) const {
                return a.host_name < b.host_name;
            }
        } host_name_sort;
        std::sort(all_hosts.begin(), all_hosts.end(), host_name_sort);

        // Expand
        for (LimitlessHostInfo host : all_hosts) {
            for (int i = 0; i < host.weight; i++) {
                round_robin_hosts.push_back(host);
            }
        }
        return round_robin_hosts;
    }

    void LoadThreads(std::string conn_in) {
        SQLHDBC local_dbc;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &local_dbc);
        ODBC_HELPER::DriverConnect(local_dbc, conn_in);

        SQLHSTMT stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, local_dbc, &stmt);

        // Repeated queries within two monitor intervals
        int num_queries = 2 * (MONITOR_INTERVAL_MS / 1000);
        for (int i = 0; i < num_queries; i++) {
            ODBC_HELPER::ExecuteQuery(stmt, SELECT_QUERY);
            SQLCloseCursor(stmt);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, local_dbc, stmt);
    }

protected:
    static void SetUpTestSuite() {
        BaseConnectionTest::SetUpTestSuite();
    }

    static void TearDownTestSuite() {
        BaseConnectionTest::TearDownTestSuite();
    }

    void SetUp() override {
        BaseConnectionTest::SetUp();
        conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
            .withLimitlessEnabled(false)
            .getString();
        ODBC_HELPER::DriverConnect(dbc, conn_str);
    }

    void TearDown() override {
        BaseConnectionTest::TearDown();
    }
};

TEST_F(LimitlessIntegrationTest, LimitlessImmediate) {
    std::vector<LimitlessHostInfo> all_hosts = QueryRouters();
    std::vector<LimitlessHostInfo> round_robin_hosts = SortedRoundRobin(all_hosts);
    ASSERT_FALSE(round_robin_hosts.empty());
    std::string expected_host = round_robin_hosts[0].host_name;

    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
        .withLimitlessEnabled(true)
        .withLimitlessMode("immediate")
        .withLimitlessMonitorIntervalMs(1000)
        .getString();

    SQLHDBC local_dbc = nullptr;
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &local_dbc);
    ASSERT_TRUE(SQL_SUCCEEDED(rc));
    rc = ODBC_HELPER::DriverConnect(local_dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    SQLTCHAR server_name[ROUTER_ENDPOINT_LENGTH] = { 0 };
    SQLGetInfo(local_dbc, SQL_SERVER_NAME, server_name, sizeof(server_name), nullptr);
    EXPECT_EQ(expected_host, STRING_HELPER::SqltcharToAnsi(server_name));

    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, local_dbc, SQL_NULL_HSTMT);
}

TEST_F(LimitlessIntegrationTest, LimitlessLazy) {
    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
        .withLimitlessEnabled(true)
        .withLimitlessMode("lazy")
        .withLimitlessMonitorIntervalMs(MONITOR_INTERVAL_MS)
        .getString();

    SQLHDBC first_dbc = nullptr;
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &first_dbc);
    ASSERT_TRUE(SQL_SUCCEEDED(rc));
    rc = ODBC_HELPER::DriverConnect(first_dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    // Wait for monitors to spinup and fetch routers
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * MONITOR_INTERVAL_MS));

    std::vector<LimitlessHostInfo> all_hosts = QueryRouters();
    std::vector<LimitlessHostInfo> round_robin_hosts = SortedRoundRobin(all_hosts);
    EXPECT_FALSE(round_robin_hosts.empty());
    std::string expected_host = round_robin_hosts[0].host_name;

    SQLHDBC second_dbc = SQL_NULL_HDBC;
    rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &second_dbc);
    ASSERT_TRUE(SQL_SUCCEEDED(rc));
    rc = ODBC_HELPER::DriverConnect(second_dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    // Get 2nd DBC's server
    SQLTCHAR server_name[ROUTER_ENDPOINT_LENGTH] = { 0 };
    SQLGetInfo(second_dbc, SQL_SERVER_NAME, server_name, sizeof(server_name), nullptr);
    EXPECT_EQ(expected_host, STRING_HELPER::SqltcharToAnsi(server_name));

    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, second_dbc, SQL_NULL_HSTMT);
    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, first_dbc, SQL_NULL_HSTMT);
}

TEST_F(LimitlessIntegrationTest, HeavyLoadInitial) {
    std::vector<LimitlessHostInfo> limitless_hosts = QueryRouters();
    std::vector<LimitlessHostInfo> round_robin_hosts = SortedRoundRobin(limitless_hosts);
    ASSERT_FALSE(round_robin_hosts.empty());
    std::string initial_host = round_robin_hosts[0].host_name;

    std::string load_conn_str = ConnectionStringBuilder(test_dsn, initial_host, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withLimitlessEnabled(false)
        .getString();

    std::vector<std::shared_ptr<std::thread>> load_threads;
    for (int i = 0; i < NUM_CONNECTIONS_TO_OVERLOAD_ROUTER; i++) {
        std::shared_ptr<std::thread> thread = std::make_shared<std::thread>([this, load_conn_str]{ LoadThreads(load_conn_str); });
        load_threads.push_back(thread);
    }

    // Sleep for monitors to update router info
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * MONITOR_INTERVAL_MS));

    // Update Expected Hosts
    limitless_hosts = QueryRouters();
    round_robin_hosts = SortedRoundRobin(limitless_hosts);

    std::string limitless_conn_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
        .withLimitlessEnabled(true)
        .withLimitlessMode("immediate")
        .withLimitlessMonitorIntervalMs(MONITOR_INTERVAL_MS)
        .getString();

    std::vector<SQLHDBC> limitless_dbcs;
    int num_limitless_conn = 3;
    ASSERT_TRUE(round_robin_hosts.size() >= num_limitless_conn);
    for (int i = 0; num_limitless_conn < 3; i++) {
        SQLHDBC limitless_dbc = SQL_NULL_HDBC;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &limitless_dbc);
        limitless_dbcs.push_back(limitless_dbc);

        SQLRETURN rc = ODBC_HELPER::DriverConnect(limitless_dbc, limitless_conn_string);
        EXPECT_EQ(SQL_SUCCESS, rc);

        SQLTCHAR server_name[ROUTER_ENDPOINT_LENGTH] = { 0 };
        SQLGetInfo(limitless_dbc, SQL_SERVER_NAME, server_name, sizeof(server_name), nullptr);
        EXPECT_EQ(round_robin_hosts[i].host_name, STRING_HELPER::SqltcharToAnsi(server_name));
    }

    for (SQLHDBC limitless_dbc : limitless_dbcs) {
        ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, limitless_dbc, SQL_NULL_HSTMT);
    }

    for (std::shared_ptr<std::thread> thread : load_threads) {
        thread->join();
    }
}
