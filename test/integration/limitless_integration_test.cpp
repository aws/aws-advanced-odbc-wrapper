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

#include "../common/connection_string_builder.h"
#include "integration_test_utils.h"

// Generally accepted URL endpoint max length (2048) + 1 for null terminator
#define ROUTER_ENDPOINT_LENGTH  2049
#define LOAD_LENGTH             5
#define WEIGHT_SCALING          10
#define MAX_WEIGHT              10
#define MIN_WEIGHT              1

#define MONITOR_INTERVAL_MS     15000

#define NUM_CONNECTIONS_TO_OVERLOAD_ROUTER  20

class LimitlessIntegrationTest : public testing::Test {
public:
    SQLTCHAR *ROUTER_ENDPOINT_QUERY = AS_SQLTCHAR(TEXT("SELECT router_endpoint, load FROM aurora_limitless_router_endpoints();"));
    SQLTCHAR *SELECT_QUERY = AS_SQLTCHAR(TEXT("SELECT 1;"));
    struct LimitlessHostInfo {
        std::string host_name;
        int64_t weight;
    };

    // lossy conversion
    std::string SQLTCHARToString(SQLTCHAR* input, size_t len) {
        char* buf = new char[len];
        for (size_t i = 0; i < len; i++) {
            buf[i] = (char)input[i];
        }
        std::string res(buf);
        delete[] buf;
        return res;
    }

    LimitlessHostInfo CreateHost(SQLTCHAR* load, SQLTCHAR* endpoint) {
        #if UNICODE
        double load_num = std::strtod(SQLTCHARToString(load, wcslen(load)).c_str(), nullptr);
        #else
        double load_num = std::strtod(reinterpret_cast<const char*>(load), nullptr);
        #endif
        int64_t weight = WEIGHT_SCALING - std::floor(load_num * WEIGHT_SCALING);

        if (weight < MIN_WEIGHT || weight > MAX_WEIGHT) {
            weight = MIN_WEIGHT;
        }

        #if UNICODE
        std::string router_endpoint_str(SQLTCHARToString(endpoint, wcslen(endpoint)));
        #else
        std::string router_endpoint_str(reinterpret_cast<const char*>(endpoint));
        #endif
        
        return LimitlessHostInfo{
            .host_name = router_endpoint_str,
            .weight = weight
        };
    }

    std::vector<LimitlessHostInfo> QueryRouters(SQLHDBC dbc) {
        SQLRETURN ret = SQL_ERROR;
        SQLHSTMT hstmt = SQL_NULL_HSTMT;

        ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
        SQLTCHAR router_endpoint_value[ROUTER_ENDPOINT_LENGTH] = {0};
        SQLLEN ind_router_endpoint_value = 0;

        SQLTCHAR load_value[LOAD_LENGTH] = {0};
        SQLLEN ind_load_value = 0;

        ret = SQLBindCol(hstmt, 1, SQL_C_TCHAR, &router_endpoint_value, sizeof(router_endpoint_value), &ind_router_endpoint_value);
        ret = SQLBindCol(hstmt, 2, SQL_C_TCHAR, &load_value, sizeof(load_value), &ind_load_value);

        ret = SQLExecDirect(hstmt, ROUTER_ENDPOINT_QUERY, SQL_NTS);

        std::vector<LimitlessHostInfo> limitless_routers;
        while (SQL_SUCCEEDED(ret = SQLFetch(hstmt))) {
            limitless_routers.push_back(CreateHost(load_value, router_endpoint_value));
        }

        INTEGRATION_TEST_UTILS::odbc_cleanup(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);

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

    void LoadThreads(SQLTCHAR *conn_in) {
        SQLHDBC local_dbc;
        SQLHSTMT stmt;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &local_dbc);
        SQLDriverConnect(local_dbc, nullptr, conn_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        SQLAllocHandle(SQL_HANDLE_STMT, local_dbc, &stmt);

        // Repeated queries within two monitor intervals
        int num_queries = 2 * (MONITOR_INTERVAL_MS / 1000);
        for (int i = 0; i < num_queries; i++) {
            SQLExecDirect(stmt, SELECT_QUERY, SQL_NTS);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        INTEGRATION_TEST_UTILS::odbc_cleanup(SQL_NULL_HENV, local_dbc, stmt);
    }

protected:
    std::string test_dsn = std::getenv("TEST_DSN");
    std::string test_db = std::getenv("TEST_DATABASE");
    std::string test_uid = std::getenv("TEST_USERNAME");
    std::string test_pwd = std::getenv("TEST_PASSWORD");
    int test_port = INTEGRATION_TEST_UTILS::str_to_int(
        INTEGRATION_TEST_UTILS::get_env_var("TEST_PORT", (char*) "5432"));
    std::string test_region = INTEGRATION_TEST_UTILS::get_env_var("TEST_REGION", (char*) "us-west-1");
    std::string test_server = std::getenv("TEST_SERVER");

    std::string connection_string = "";

    SQLHENV env = nullptr;
    SQLHDBC monitor_dbc = nullptr;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
        SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
        SQLAllocHandle(SQL_HANDLE_DBC, env, &monitor_dbc);

        auto monitor_connection_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
            .withLimitlessEnabled(false)
            .getRdsString();
        SQLDriverConnect(monitor_dbc, nullptr, AS_SQLTCHAR(monitor_connection_string.c_str()), SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    }

    void TearDown() override {
        INTEGRATION_TEST_UTILS::odbc_cleanup(env, monitor_dbc, SQL_NULL_HSTMT);
    }
};

TEST_F(LimitlessIntegrationTest, LimitlessImmediate) {
    std::vector<LimitlessHostInfo> all_hosts = QueryRouters(monitor_dbc);
    std::vector<LimitlessHostInfo> round_robin_hosts = SortedRoundRobin(all_hosts);
    ASSERT_FALSE(round_robin_hosts.empty());
    std::string expected_host = round_robin_hosts[0].host_name;

    auto conn_in_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
        .withLimitlessEnabled(true)
        .withLimitlessMode("immediate")
        .withLimitlessMonitorIntervalMs(1000)
        .getRdsString();
    SQLTCHAR *conn_in = AS_SQLTCHAR(conn_in_str.c_str());

    SQLHDBC dbc = nullptr;
    EXPECT_TRUE(SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc)));
    EXPECT_TRUE(SQL_SUCCEEDED(SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT)));

    SQLTCHAR server_name[ROUTER_ENDPOINT_LENGTH] = { 0 };
    SQLGetInfo(dbc, SQL_SERVER_NAME, server_name, sizeof(server_name), nullptr);
    EXPECT_EQ(expected_host, std::string((const char*) server_name));

    INTEGRATION_TEST_UTILS::odbc_cleanup(SQL_NULL_HENV, dbc, SQL_NULL_HSTMT);
}

TEST_F(LimitlessIntegrationTest, LimitlessLazy) {
    auto conn_in_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
        .withLimitlessEnabled(true)
        .withLimitlessMode("lazy")
        .withLimitlessMonitorIntervalMs(MONITOR_INTERVAL_MS)
        .getRdsString();
    SQLTCHAR *conn_in = AS_SQLTCHAR(conn_in_str.c_str());

    SQLHDBC dbc = nullptr;
    EXPECT_TRUE(SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc)));
    EXPECT_TRUE(SQL_SUCCEEDED(SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT)));

    // Wait for monitors to spinup and fetch routers
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * MONITOR_INTERVAL_MS));

    std::vector<LimitlessHostInfo> all_hosts = QueryRouters(monitor_dbc);
    std::vector<LimitlessHostInfo> round_robin_hosts = SortedRoundRobin(all_hosts);
    ASSERT_FALSE(round_robin_hosts.empty());
    std::string expected_host = round_robin_hosts[0].host_name;

    SQLHDBC second_dbc = SQL_NULL_HDBC;
    SQLAllocHandle(SQL_HANDLE_DBC, env, &second_dbc);
    EXPECT_TRUE(SQL_SUCCEEDED(SQLDriverConnect(second_dbc, nullptr, conn_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT)));

    SQLTCHAR server_name[ROUTER_ENDPOINT_LENGTH] = { 0 };
    SQLGetInfo(second_dbc, SQL_SERVER_NAME, server_name, sizeof(server_name), nullptr);
    EXPECT_EQ(expected_host, std::string((const char*) server_name));

    INTEGRATION_TEST_UTILS::odbc_cleanup(SQL_NULL_HENV, second_dbc, SQL_NULL_HSTMT);
    INTEGRATION_TEST_UTILS::odbc_cleanup(SQL_NULL_HENV, dbc, SQL_NULL_HSTMT);
}

TEST_F(LimitlessIntegrationTest, HeavyLoadInitial) {
    std::vector<LimitlessHostInfo> limitless_hosts = QueryRouters(monitor_dbc);
    std::vector<LimitlessHostInfo> round_robin_hosts = SortedRoundRobin(limitless_hosts);
    ASSERT_FALSE(round_robin_hosts.empty());
    std::string initial_host = round_robin_hosts[0].host_name;

    auto load_conn_string = ConnectionStringBuilder(test_dsn, initial_host, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withLimitlessEnabled(false)
        .getRdsString();
    SQLTCHAR *conn_in = AS_SQLTCHAR(load_conn_string.c_str());

    std::vector<std::shared_ptr<std::thread>> load_threads;
    for (int i = 0; i < NUM_CONNECTIONS_TO_OVERLOAD_ROUTER; i++) {
        std::shared_ptr<std::thread> thread = std::make_shared<std::thread>([this, conn_in]{ LoadThreads(conn_in); });
        load_threads.push_back(thread);
    }

    // Sleep for monitors to update router info
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * MONITOR_INTERVAL_MS));

    // Update Expected Hosts
    limitless_hosts = QueryRouters(monitor_dbc);
    round_robin_hosts = SortedRoundRobin(limitless_hosts);

    auto limitless_conn_string = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect("AURORA_POSTGRESQL_LIMITLESS")
        .withLimitlessEnabled(true)
        .withLimitlessMode("immediate")
        .withLimitlessMonitorIntervalMs(MONITOR_INTERVAL_MS)
        .getRdsString();
    conn_in = AS_SQLTCHAR(limitless_conn_string.c_str());

    std::vector<SQLHDBC> limitless_dbcs;
    for (int i = 0; i < 3; i++) {
        SQLHDBC limitless_dbc = SQL_NULL_HDBC;
        SQLAllocHandle(SQL_HANDLE_DBC, env, &limitless_dbc);
        limitless_dbcs.push_back(limitless_dbc);

        EXPECT_TRUE(SQL_SUCCEEDED(SQLDriverConnect(limitless_dbc, nullptr, conn_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT)));

        SQLTCHAR server_name[ROUTER_ENDPOINT_LENGTH] = { 0 };
        SQLGetInfo(limitless_dbc, SQL_SERVER_NAME, server_name, sizeof(server_name), nullptr);
        EXPECT_EQ(round_robin_hosts[i].host_name, std::string((const char*) server_name));
    }

    for (SQLHDBC limitless_dbc : limitless_dbcs) {
        INTEGRATION_TEST_UTILS::odbc_cleanup(SQL_NULL_HENV, limitless_dbc, SQL_NULL_HSTMT);
    }

    for (std::shared_ptr<std::thread> thread : load_threads) {
        thread->join();
    }
}
