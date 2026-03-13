// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base_failover_integration_test.h"

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/rds/RDSClient.h>
#include <aws/rds/model/BlueGreenDeployment.h>
#include <aws/rds/model/DBCluster.h>
#include <aws/rds/model/DBInstance.h>
#include <aws/rds/model/SwitchoverBlueGreenDeploymentRequest.h>

#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <sstream>

namespace {
    struct TimeHolder {
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        std::optional<std::chrono::steady_clock::time_point> hold_nano;
        std::optional<std::string> error;

        TimeHolder(std::chrono::steady_clock::time_point start_time, std::chrono::steady_clock::time_point end_time)
            : start_time(start_time), end_time(end_time) {}

        TimeHolder(std::chrono::steady_clock::time_point start_time, std::chrono::steady_clock::time_point end_time, std::chrono::steady_clock::time_point hold_nano)
            : start_time(start_time), end_time(end_time), hold_nano(hold_nano) {}

        TimeHolder(std::chrono::steady_clock::time_point start_time, std::chrono::steady_clock::time_point end_time, const std::string& error)
            : start_time(start_time), end_time(end_time), error(error) {}

        TimeHolder(std::chrono::steady_clock::time_point start_time, std::chrono::steady_clock::time_point end_time, std::chrono::steady_clock::time_point hold_nano, const std::string& error)
            : start_time(start_time), end_time(end_time), hold_nano(hold_nano), error(error) {}
    };

    struct HostVerificationResult {
        const std::chrono::steady_clock::time_point timestamp;
        const std::string connected_host;
        const std::string original_blue_ip;
        const bool connected_to_blue;
        const std::string error;

        HostVerificationResult(std::chrono::steady_clock::time_point timestamp, const std::string& connected_host, const std::string& original_blue_ip, const std::string& error)
            : timestamp(timestamp), connected_host(connected_host), original_blue_ip(original_blue_ip), connected_to_blue(!connected_host.empty() && connected_host == original_blue_ip), error(error) {}

        static HostVerificationResult Success(std::chrono::steady_clock::time_point timestamp, const std::string& connected_host, const std::string& original_blue_ip) {
            return HostVerificationResult(timestamp, connected_host, original_blue_ip, "");
        }

        static HostVerificationResult Failure(std::chrono::steady_clock::time_point timestamp, const std::string& original_blue_ip, const std::string& error) {
            return HostVerificationResult(timestamp, "", original_blue_ip, error);
        }
    };

    struct BlueGreenResults {
        std::atomic<std::chrono::steady_clock::time_point> start_time{};
        std::atomic<std::chrono::steady_clock::time_point> threads_sync_time{};
        std::atomic<std::chrono::steady_clock::time_point> bg_trigger_time{};
        std::atomic<std::chrono::steady_clock::time_point> direct_blue_lost_connection_time{};
        std::atomic<std::chrono::steady_clock::time_point> direct_blue_idle_lost_connection_time{};
        std::atomic<std::chrono::steady_clock::time_point> wrapper_blue_idle_lost_connection_time{};
        std::atomic<std::chrono::steady_clock::time_point> wrapper_green_lost_connection_time{};
        std::atomic<std::chrono::steady_clock::time_point> dns_blue_changed_time{};
        std::string dns_blue_error;
        std::atomic<std::chrono::steady_clock::time_point> dns_green_removed_time{};
        std::atomic<std::chrono::steady_clock::time_point> green_node_changed_name_time{};
        // TODO - Check concurrency for this
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> blue_status_time{};
        // TODO - Check concurrency for this
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> green_status_time{};
        std::deque<TimeHolder> blue_wrapper_connect_times;
        std::deque<TimeHolder> blue_wrapper_pre_switchover_execute_times;
        std::deque<TimeHolder> blue_wrapper_post_switchover_execute_times;
        std::deque<TimeHolder> green_wrapper_execute_times;
        std::deque<TimeHolder> green_direct_iam_ip_with_blue_node_connect_times;
        std::deque<TimeHolder> green_direct_iam_ip_with_green_node_connect_times;
        std::deque<HostVerificationResult> host_verification_results;
    };
} // namespace

namespace ThreadSynchronization {
    int total_threads = 0;

    std::condition_variable start_latch;
    std::atomic<int> ready_count = 0;
    std::atomic<bool> start_flag = false;

    void ReadyAndWait() {
        ThreadSynchronization::ready_count++;
        std::mutex latch_mutex;
        std::unique_lock<std::mutex> lock(latch_mutex);
        ThreadSynchronization::start_latch.wait(lock, [] {
            return ThreadSynchronization::start_flag
                && ThreadSynchronization::ready_count == ThreadSynchronization::total_threads;
        });
    };

    std::condition_variable finish_latch;
    std::atomic<int> finish_count = 0;
    std::atomic<bool> stop_flag = false;

    void ThreadFinished() {
        ThreadSynchronization::finish_count++;
        finish_latch.notify_all();
    }

    void WaitForFinish(std::chrono::milliseconds wait_ms) {
        std::mutex latch_mutex;
        std::unique_lock<std::mutex> lock(latch_mutex);
        ThreadSynchronization::finish_latch.wait_for(lock, wait_ms, [] {
            return true;
        });
    };
}

class BlueGreenIntegrationTest : public BaseFailoverIntegrationTest {

protected:
    std::string blue_green_deployment_id = TEST_UTILS::GetEnvVar("TEST_BG_RESOURCE_ID");
    std::string access_key = TEST_UTILS::GetEnvVar("AWS_ACCESS_KEY_ID");
    std::string secret_access_key = TEST_UTILS::GetEnvVar("AWS_SECRET_ACCESS_KEY");
    std::string session_token = TEST_UTILS::GetEnvVar("AWS_SESSION_TOKEN");
    std::string rds_endpoint = TEST_UTILS::GetEnvVar("RDS_ENDPOINT");
    std::string rds_region = TEST_UTILS::GetEnvVar("TEST_REGION", "us-west-1");
    Aws::RDS::RDSClientConfiguration client_config;
    Aws::RDS::RDSClient rds_client;

    std::unordered_map<std::string, BlueGreenResults> results;
    std::deque<std::exception_ptr> unhandled_exceptions;
    std::atomic<bool> rollback_detected{false};
    std::unique_ptr<std::string> rollback_details;

    ConnectionStringBuilder conn_str_builder;
    std::string test_iam_user = TEST_UTILS::GetEnvVar("TEST_IAM_USER", "");

    std::string sleep_query;
    std::string server_ip_query;
    std::string bg_status_query;
    std::string simple_select_query = "SELECT 1";

    static void SetUpTestSuite() {
        BaseFailoverIntegrationTest::SetUpTestSuite();
        Aws::InitAPI(options);
    }

    static void TearDownTestSuite() {
        Aws::ShutdownAPI(options);
        BaseFailoverIntegrationTest::TearDownTestSuite();
    }

    void SetUp() override {
        if (test_dialect == "AURORA_MYSQL") {
            GTEST_SKIP() << "MySQL ODBC Connector does not support IAM with session tokens due to limitations on connection key-value pairs.";
        }

        BaseFailoverIntegrationTest::SetUp();
        Aws::Auth::AWSCredentials credentials =
            session_token.empty() ? Aws::Auth::AWSCredentials(access_key, secret_access_key)
                                  : Aws::Auth::AWSCredentials(access_key, secret_access_key, session_token);
        client_config.region = rds_region;
        if (!rds_endpoint.empty()) {
            client_config.endpointOverride = rds_endpoint;
        }
        rds_client = Aws::RDS::RDSClient(credentials, client_config);
        WaitForDbReady(rds_client, cluster_id);

        sleep_query = test_dialect == "AURORA_POSTGRESQL"
            ? "SELECT pg_catalog.pg_sleep(5)"   // APG
            : "SELECT sleep(5)";                // AMS

        server_ip_query = test_dialect == "AURORA_POSTGRESQL"
            ? "SELECT inet_server_addr()"   // APG
            : "SELECT @@hostname";          // AMS

        bg_status_query = test_dialect == "AURORA_POSTGRESQL"
            ? "SELECT id, SPLIT_PART(endpoint, '.', 1) as hostId, endpoint, port, role, status, version "
                "FROM pg_catalog.get_blue_green_fast_switchover_metadata('aws-odbc-wrapper')"   // APG
            : "SELECT id, SUBSTRING_INDEX(endpoint, '.', 1) as hostId, endpoint, port, role, status, version "
                "FROM mysql.rds_topology";                                                      // AMS

        conn_str_builder = ConnectionStringBuilder("")
            .withDSN(test_dsn)
            .withPort(test_port)
            .withSslMode("prefer")
            .withAuthRegion(test_region)
            .withDatabase(test_db)
            .withDatabaseDialect(test_dialect)
            .withClusterId(cluster_id)
            .withBlueGreenId(blue_green_deployment_id);
    }

    void TearDown() override {
        BaseFailoverIntegrationTest::TearDown();
    }

    std::vector<std::string> GetBlueGreenEndpoints(const std::string& blue_green_deployment_id) {
        std::vector<std::string> endpoints;
        std::vector<std::string> blue_endpoints;
        std::vector<std::string> green_endpoints;
        Aws::RDS::Model::DescribeBlueGreenDeploymentsRequest request;
        // NOTE: This needs *Resource ID*
        request.SetBlueGreenDeploymentIdentifier(blue_green_deployment_id);
        auto outcome = rds_client.DescribeBlueGreenDeployments(request);

        if (!outcome.IsSuccess()) {
            std::cerr << "Error describing BlueGreen deployment. " << outcome.GetError().GetMessage() << std::endl;
            return endpoints;
        }

        const auto& deployments = outcome.GetResult().GetBlueGreenDeployments();
        const auto& deployment = deployments.front();
        blue_endpoints = GetTopologyViaSdk(rds_client, deployment.GetSource());
        green_endpoints = GetTopologyViaSdk(rds_client, deployment.GetTarget());

        return endpoints;
    }

    std::string GetConnectedServerIp(SQLHDBC dbc) {
        SQLTCHAR buf[SQL_MAX_MESSAGE_LENGTH] = {0};
        SQLLEN buflen;
        SQLHSTMT hstmt;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
        ODBC_HELPER::ExecuteQuery(hstmt, server_ip_query);
        SQLFetch(hstmt);
        SQLGetData(hstmt, 1, SQL_C_TCHAR, buf, sizeof(buf), &buflen);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return std::string(STRING_HELPER::SqltcharToAnsi(buf));
    }

    void OpenConnectionWithRetry(SQLHDBC hdbc, std::string conn_str) {
        SQLRETURN rc = SQL_ERROR;
        int connect_count = 0;
        const int max_connect_count = 10;
        while (connect_count++ < max_connect_count)
        {
            SQLDisconnect(hdbc);
            if (SQL_SUCCEEDED(rc = ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        if (!SQL_SUCCEEDED(rc)) {
            GTEST_FAIL() << "Could not connect: " << conn_str;
        }
    }

    std::string GetDirectConnStr(const std::string& host, const bool& use_iam = true) {
        return use_iam
            ? ConnectionStringBuilder(conn_str_builder.getString())
                .withUID(test_iam_user)
                .withAuthMode("IAM")
                .withServer(host)
                .getString()
            : ConnectionStringBuilder(conn_str_builder.getString())
                .withUID(test_uid)
                .withPWD(test_pwd)
                .withServer(host)
                .getString();

    }

    std::string GetBlueGreenEnabledConnStr(const std::string& host, const bool& use_iam = true) {
        return use_iam
            ? ConnectionStringBuilder(conn_str_builder.getString())
                .withUID(test_iam_user)
                .withAuthMode("IAM")
                .withBlueGreenEnabled(true)
                .withServer(host)
                .getString()
            : ConnectionStringBuilder(conn_str_builder.getString())
                .withUID(test_uid)
                .withPWD(test_pwd)
                .withBlueGreenEnabled(true)
                .withServer(host)
                .getString();
    }

    void PrintMetrics(const std::unordered_map<std::string, BlueGreenResults>& results) {
    }

    void LogUnhandledExceptions(const std::deque<std::exception_ptr>& exceptions) {
    }

    void AssertTest() {
    }

    void AssertSwitchoverCompleted() {
    }

    void AssertWrapperBehavior() {
    }

    std::chrono::steady_clock::time_point GetBgTriggerTime() {
    }

    std::chrono::steady_clock::time_point GetSwitchoverCompleteTime() {
    }

    std::chrono::steady_clock::time_point GetSwitchoverCompleteTimeFromStatusTable() {
    }

    std::chrono::steady_clock::time_point GetMaxGreenNodeChangeTime() {
    }

    std::chrono::steady_clock::time_point GetTimeOffsetMs(
        std::chrono::steady_clock::time_point nano_time, std::chrono::steady_clock::time_point bg_trigger_time)
    {
    }

    std::chrono::steady_clock::time_point GetSwitchoverInitiatedTime(
        std::chrono::steady_clock::time_point bg_trigger_time)
    {
    }

    std::chrono::steady_clock::time_point GetSwitchoverInProgressTime(
        std::chrono::steady_clock::time_point bg_trigger_time)
    {
    }

    void AssertNoConnectionsToOldBlueCluster(std::chrono::steady_clock::time_point bg_trigger_time)
    {
    }

    std::thread CreateDirectBlueIdleConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetDirectConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            std::cout << "[DirectBlueIdleConnectivityMonitoringThread: " << host_id << "] started monitoring idle connection." << std::endl;
            while (!ThreadSynchronization::stop_flag) {
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    std::cout << "[DirectBlueIdleConnectivityMonitoringThread: " << host_id << "] lost connection." << std::endl;
                    results.direct_blue_idle_lost_connection_time.store(std::chrono::steady_clock::now());
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateDirectBlueConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetDirectConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            std::cout << "[DirectBlueConnectivityMonitoringThread: " << host_id << "] started monitoring connection." << std::endl;
            SQLHSTMT hstmt;
            SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
            while (!ThreadSynchronization::stop_flag) {
                if (!SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, simple_select_query))) {
                    std::cout << "[DirectBlueIdleConnectivityMonitoringThread: " << host_id << "] failed to execute." << std::endl;
                    results.direct_blue_lost_connection_time.store(std::chrono::steady_clock::now());
                    break;
                }
                SQLCloseCursor(hstmt);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, hstmt);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateWrapperBlueIdleConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Actual Work
            std::cout << "[WrapperBlueIdleConnectivityMonitoringThread: " << host_id << "] started monitoring idle connection." << std::endl;
            while (!ThreadSynchronization::stop_flag) {
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    std::cout << "[WrapperBlueIdleConnectivityMonitoringThread: " << host_id << "] lost connection." << std::endl;
                    results.wrapper_blue_idle_lost_connection_time.store(std::chrono::steady_clock::now());
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateWrapperBlueExecutingConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            std::cout << "[WrapperBlueExecutingConnectivityMonitoringThread: " << host_id << "] started monitoring connection." << std::endl;

            // Phase 1 - Execute until connection closes during switchover
            SQLHSTMT hstmt;
            SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
            while (!ThreadSynchronization::stop_flag) {
                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time{};
                if (SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, sleep_query))) {
                    end_time = std::chrono::steady_clock::now();
                    SQLCloseCursor(hstmt);
                    results.blue_wrapper_pre_switchover_execute_times.push_back(
                        TimeHolder(start_time, end_time)
                    );
                } else {
                    end_time = std::chrono::steady_clock::now();
                    SQLCloseCursor(hstmt);
                    std::string err = ODBC_HELPER::PrintHandleError(hstmt, SQL_HANDLE_STMT);
                    std::cout << "[WrapperBlueExecutingConnectivityMonitoringThread: " << host_id << "] Failed to connect: " << err << std::endl;
                    results.blue_wrapper_pre_switchover_execute_times.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                    results.direct_blue_lost_connection_time.store(std::chrono::steady_clock::now());
                    if (ODBC_HELPER::IsClosed(hdbc)) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Phase 2 - Post-Switchover, reconnect and continue executing
            while (!ThreadSynchronization::stop_flag) {
                // Reconnect if needed
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);
                    SQLDisconnect(hdbc);
                    OpenConnectionWithRetry(hdbc, conn_str);
                    SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
                }

                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time{};
                if (SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, sleep_query))) {
                    end_time = std::chrono::steady_clock::now();
                    SQLCloseCursor(hstmt);
                    results.blue_wrapper_post_switchover_execute_times.push_back(
                        TimeHolder(start_time, end_time)
                    );
                } else {
                    end_time = std::chrono::steady_clock::now();
                    SQLCloseCursor(hstmt);
                    std::string err = ODBC_HELPER::PrintHandleError(hstmt, SQL_HANDLE_STMT);
                    std::cout << "[WrapperBlueExecutingConnectivityMonitoringThread: " << host_id << "] Failed to connect: " << err << std::endl;
                    results.blue_wrapper_post_switchover_execute_times.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);
                    SQLDisconnect(hdbc);
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, hstmt);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateWrapperBlueNewConnectionMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            while (!ThreadSynchronization::stop_flag) {
                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time{};

                if (SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                    end_time = std::chrono::steady_clock::now();
                    results.blue_wrapper_connect_times.push_back(
                        TimeHolder(start_time, end_time)
                    );
                } else {
                    end_time = std::chrono::steady_clock::now();
                    std::string err = ODBC_HELPER::PrintHandleError(hdbc, SQL_HANDLE_DBC);
                    std::cout << "[WrapperBlueNewConnectionMonitoringThread: " << host_id << "] Failed to connect: " << err << std::endl;
                    results.blue_wrapper_connect_times.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                }

                SQLDisconnect(hdbc);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateWrapperBlueHostVerificationThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);
            const std::string original_ip = GetConnectedServerIp(hdbc);
            SQLDisconnect(hdbc);

            if (original_ip.empty()) {
                GTEST_FAIL() << "Unable to fetch original blue IP from initial connection.";
            }

            std::cout << "[WrapperBlueHostVerificationThread: " << host_id << "] Original Blue IP: " << original_ip << std::endl;

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            while (!ThreadSynchronization::stop_flag) {
                std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
                if (SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                    std::string latest_ip = GetConnectedServerIp(hdbc);
                    HostVerificationResult result = HostVerificationResult::Success(timestamp, latest_ip, original_ip);
                    results.host_verification_results.push_back(result);
                    if (result.connected_to_blue) {
                        std::cout << "[WrapperBlueHostVerificationThread: " << host_id << "] Connected to Blue Cluster on IP: " << latest_ip << std::endl;
                    } else {
                        std::cout << "[WrapperBlueHostVerificationThread: " << host_id << "] Connected to Green Cluster on IP: " << latest_ip << std::endl;
                    }
                } else {
                    std::string err = ODBC_HELPER::PrintHandleError(hdbc, SQL_HANDLE_DBC);
                    std::cout << "[WrapperBlueHostVerificationThread: " << host_id << "] Failed to connect: " << err << std::endl;
                    results.host_verification_results.push_back(
                        HostVerificationResult::Failure(timestamp, original_ip, err)
                    );
                }

                SQLDisconnect(hdbc);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateGreenDnsMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            const std::string original_ip_address = TEST_UTILS::HostToIp(host);
            std::cout << "[GreenDnsMonitoringThread: " << host_id << "] IP Address: " << original_ip_address << std::endl;

            while (!ThreadSynchronization::stop_flag) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                const std::string current_ip = TEST_UTILS::HostToIp(host);
                if (current_ip != original_ip_address) {
                    std::cout << "[GreenDnsMonitoringThread: " << host_id << "] IP Address Changed: " << current_ip << std::endl;
                    results.dns_green_removed_time.store(std::chrono::steady_clock::now());
                    break;
                }
            }

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateBlueDnsMonitoringThread(
        const std::string& host_id, const std::string& host,
        BlueGreenResults& results)
    {
        return std::thread([&]{
            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            const std::string original_ip_address = TEST_UTILS::HostToIp(host);
            std::cout << "[BlueDnsMonitoringThread: " << host_id << "] IP Address: " << original_ip_address << std::endl;

            while (!ThreadSynchronization::stop_flag) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                const std::string current_ip = TEST_UTILS::HostToIp(host);
                if (current_ip != original_ip_address) {
                    std::cout << "[BlueDnsMonitoringThread: " << host_id << "] IP Address Changed: " << current_ip << std::endl;
                    results.dns_blue_changed_time.store(std::chrono::steady_clock::now());
                    break;
                }
            }

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateDirectTopologyMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetDirectConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);
            const int BUFFER_SIZE = 512;

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();
            std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now()
                + std::chrono::minutes(15);

            std::cout << "[DirectTopologyMonitoringThread: " << host_id << "] Starting BG Status Monitoring." << std::endl;

            while (!ThreadSynchronization::stop_flag && std::chrono::steady_clock::now() < end_time) {
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    SQLDisconnect(hdbc);
                    OpenConnectionWithRetry(hdbc, conn_str);
                    std::cout << "[DirectTopologyMonitoringThread: " << host_id << "] Connection re-opened." << std::endl;
                }

                SQLHSTMT hstmt;
                SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
                if (SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, bg_status_query))) {
                    SQLLEN len = 0;
                    SQLTCHAR role[BUFFER_SIZE] = {0};
                    SQLTCHAR status[BUFFER_SIZE] = {0};
                    SQLBindCol(hstmt, 4, SQL_C_TCHAR, &role, BUFFER_SIZE, &len);
                    SQLBindCol(hstmt, 5, SQL_C_TCHAR, &status, BUFFER_SIZE, &len);

                    if (SQL_SUCCEEDED(SQLFetch(hstmt))) {
                        std::string status_str = STRING_HELPER::SqltcharToAnsi(status);
                        bool is_green = STRING_HELPER::SqltcharToAnsi(role) == "BLUE_GREEN_DEPLOYMENT_TARGET";

                        if (is_green) {
                            std::cout << "[DirectTopologyMonitoringThread: " << host_id << "] Green Status Changed to: " << status_str << std::endl;
                            results.green_status_time.try_emplace(
                                status_str,
                                std::chrono::steady_clock::now()
                            );
                        } else {
                            std::cout << "[DirectTopologyMonitoringThread: " << host_id << "] Blue Status Changed to: " << status_str << std::endl;
                            results.blue_status_time.try_emplace(
                                status_str,
                                std::chrono::steady_clock::now()
                            );
                        }
                    }
                    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
                    SQLDisconnect(hdbc);
                }
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateWrapperGreenConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results)
    {
        return std::thread([&]{
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            std::cout << "[WrapperGreenConnectivityMonitoringThread: " << host_id << "] started monitoring connection." << std::endl;
            SQLHSTMT hstmt;
            SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
            while (!ThreadSynchronization::stop_flag) {
                if (!SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, simple_select_query))) {
                    std::cout << "[WrapperGreenConnectivityMonitoringThread: " << host_id << "] failed to execute." << std::endl;
                    results.wrapper_green_lost_connection_time.store(std::chrono::steady_clock::now());
                    break;
                }
                SQLCloseCursor(hstmt);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, hstmt);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateGreenIamConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, BlueGreenResults& results,
        std::deque<TimeHolder> result_queue, const std::string& iam_host, const std::string& thread_prefix,
        bool notify_on_first_error, bool exit_on_first_success)
    {
        return std::thread([&]{
            // Setup
            const std::string iam_conn_str_prefix = ConnectionStringBuilder(conn_str_builder.getString())
                .withServer(host)
                .withUID(test_iam_user)
                .getString();
            const std::string green_node_original_ip = TEST_UTILS::HostToIp(host);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            std::cout << "[GreenIamConnectivityMonitoringThread_" << thread_prefix << ": " << host_id << "] started monitoring connection." << std::endl;

            // Work
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            while (!ThreadSynchronization::stop_flag) {
                const std::string iam_token = rds_client.GenerateConnectAuthToken(
                    iam_host.c_str(), test_region.c_str(), test_port, test_iam_user.c_str());
                const std::string conn_str = ConnectionStringBuilder(iam_conn_str_prefix)
                    .withPWD(iam_token)
                    .getString();

                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time{};
                if (SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                    end_time = std::chrono::steady_clock::now();
                    result_queue.push_back(
                        TimeHolder(start_time, end_time)
                    );

                    if (exit_on_first_success) {
                        std::chrono::steady_clock::time_point empty{};
                        std::cout << "[GreenIamConnectivityMonitoringThread_" << thread_prefix << ": " << host_id << "] Successfully connecting, exiting." << std::endl;
                        results.green_node_changed_name_time.compare_exchange_strong(empty, std::chrono::steady_clock::now());
                        ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
                        return;
                    }
                } else {
                    end_time = std::chrono::steady_clock::now();
                    std::string err = ODBC_HELPER::PrintHandleError(hdbc, SQL_HANDLE_DBC);
                    std::cout << "[GreenIamConnectivityMonitoringThread_" << thread_prefix << ": " << host_id << "] Failed to connect: " << err << std::endl;
                    result_queue.push_back(
                        TimeHolder(start_time, end_time, err)
                    );

                    if (notify_on_first_error
                        && (
                            err.find("Access denied") != std::string::npos
                            || err.find("PAM authentication failed")  != std::string::npos
                        ))
                    {
                        std::chrono::steady_clock::time_point empty{};
                        results.green_node_changed_name_time.compare_exchange_strong(empty, std::chrono::steady_clock::now());
                        std::cout << "[GreenIamConnectivityMonitoringThread_" << thread_prefix << ": " << host_id << "] Exiting thread, first authentication failed: " << err << std::endl;
                        ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
                        return;
                    }
                }

                SQLDisconnect(hdbc);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }

    std::thread CreateBlueGreenSwitchoverTriggerThread(
        const std::string& blue_green_deployment_id, std::unordered_map<std::string, BlueGreenResults>& results)
    {
        return std::thread([&]{
            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Actual Work
            std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
            for (auto& [_, result] : results) {
                result.threads_sync_time.store(start_time);
            }

            std::this_thread::sleep_for(std::chrono::seconds(30));

            Aws::RDS::Model::SwitchoverBlueGreenDeploymentRequest request;
            request.WithBlueGreenDeploymentIdentifier(blue_green_deployment_id);
            auto outcome = rds_client.SwitchoverBlueGreenDeployment(request);
            if (!outcome.IsSuccess()) {
                std::cout << "Failed to send Blue Green Switchover request: " << outcome.GetError().GetMessage() << std::endl;
            } else {
                std::cout << "Successfully sent Blue Green Switchover request." << std::endl;
            }

            std::chrono::steady_clock::time_point trigger_time = std::chrono::steady_clock::now();
            for (auto& [_, result] : results) {
                result.bg_trigger_time.store(trigger_time);
            }

            // Finished
            ThreadSynchronization::ThreadFinished();
        });
    }
};

TEST_F(BlueGreenIntegrationTest, SwitchoverTest)
{
    results.clear();
    unhandled_exceptions.clear();
    rollback_detected.store(false);
    rollback_details.reset();

    const std::vector<std::string> topology_instances = GetBlueGreenEndpoints(blue_green_deployment_id);

    auto start_time = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (std::string host : topology_instances) {
        size_t pos = host.find('.');
        std::string host_id = pos != std::string::npos
            ? host_id.substr(0, pos)
            : "";
        ASSERT_FALSE(host_id.empty());

        results.insert_or_assign(host_id, BlueGreenResults());

        if (true /* rdsUtil.isNotGreenAndOldPrefixInstance(host) */) {

        }

        if (true /* rdsUtil.isGreenInstance(host) */) {

        }
    }

    for (auto& [_, v] : results) {
        v.start_time.store(start_time);
    }

    // Notify all threads are allocated and ready to start
    ThreadSynchronization::total_threads = threads.size();
    ThreadSynchronization::start_flag = true;
    ThreadSynchronization::start_latch.notify_all();
    std::cout << "Threads started" << std::endl;

    ThreadSynchronization::WaitForFinish(std::chrono::minutes(6));
    std::cout << "Threads finished" << std::endl;

    // Allow threads to finish immediate work
    std::this_thread::sleep_for(std::chrono::minutes(3));
    std::cout << "Stopping threads" << std::endl;

    // Stop threads
    ThreadSynchronization::stop_flag = true;
    std::cout << "Join threads" << std::endl;
    for (auto& thread : threads) {
        thread.join();
    }

    /* TODO - Print Test Results & Assert success cases*/
}
