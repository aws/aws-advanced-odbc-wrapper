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
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace {
    struct TimeHolder {
        std::chrono::steady_clock::time_point start_time{};
        std::chrono::steady_clock::time_point end_time{};
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
        std::chrono::steady_clock::time_point start_time{};
        std::chrono::steady_clock::time_point threads_sync_time{};
        std::chrono::steady_clock::time_point bg_trigger_time{};
        std::chrono::steady_clock::time_point direct_blue_lost_connection_time{};
        std::chrono::steady_clock::time_point direct_blue_idle_lost_connection_time{};
        std::chrono::steady_clock::time_point wrapper_blue_idle_lost_connection_time{};
        std::chrono::steady_clock::time_point wrapper_green_lost_connection_time{};
        std::chrono::steady_clock::time_point dns_blue_changed_time{};
        std::chrono::steady_clock::time_point dns_green_removed_time{};
        std::chrono::steady_clock::time_point green_node_changed_name_time_first_success{};
        std::chrono::steady_clock::time_point green_node_changed_name_time_first_error{};
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> blue_status_time{};
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
        ThreadSynchronization::start_latch.wait_for(lock, std::chrono::minutes(5), [] {
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
            return finish_count == total_threads;
        });
    };

    std::mutex cout_mutex;

    void Print(const std::string& message) {
        // std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << message << std::endl;
    }
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

    std::chrono::steady_clock::time_point empty_time_point{};
    std::chrono::steady_clock::time_point global_start_time;

    ConnectionStringBuilder conn_str_builder;
    std::string test_iam_user = TEST_UTILS::GetEnvVar("TEST_IAM_USER", "");

    std::string sleep_query;
    std::string server_ip_query;
    std::string bg_status_query;
    std::string simple_select_query = "SELECT 1";

    std::unordered_map<std::string, BlueGreenResults> results;

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
            return {};
        }

        const auto& deployments = outcome.GetResult().GetBlueGreenDeployments();
        if (deployments.empty()) {
            std::cerr << "No deployments found for resource ID: " << blue_green_deployment_id;
            return {};
        }
        const auto& deployment = deployments.front();
        blue_endpoints = GetTopologyViaSdk(rds_client, deployment.GetSource());
        green_endpoints = GetTopologyViaSdk(rds_client, deployment.GetTarget());

        endpoints.insert(endpoints.end(), blue_endpoints.begin(), blue_endpoints.end());
        endpoints.insert(endpoints.end(), green_endpoints.begin(), green_endpoints.end());

        return endpoints;
    }

    std::string GetConnectedServerIp(SQLHDBC dbc) {
        SQLTCHAR buf[SQL_MAX_MESSAGE_LENGTH] = { 0 };
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
        while (connect_count++ < max_connect_count) {
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

    void PrintMetrics() {

    }

    std::chrono::steady_clock::time_point GetBgTriggerTime() {
        for (const auto& [_, result] : results) {
            if (result.bg_trigger_time != empty_time_point) {
                return result.bg_trigger_time;
            }
        }

        return empty_time_point;
    }

    size_t GetSwitchoverCompleteTime() {
        size_t max_green_node_change_time = GetMaxGreenNodeChangeTime();
        size_t switchover_complete_time_from_status_table = GetSwitchoverCompleteTimeFromStatusTable();
        return max_green_node_change_time > switchover_complete_time_from_status_table
            ? max_green_node_change_time
            : switchover_complete_time_from_status_table;
    }

    size_t GetMaxGreenNodeChangeTime() {
        std::chrono::steady_clock::time_point bg_trigger_time = GetBgTriggerTime();
        size_t largest_offset = 0;
        for (const auto& [_, result] : results) {
            std::chrono::steady_clock::time_point green_node_changed_name_time{};
            if (result.green_node_changed_name_time_first_success != empty_time_point) {
                green_node_changed_name_time = result.green_node_changed_name_time_first_success;
            } else if (result.green_node_changed_name_time_first_error != empty_time_point) {
                green_node_changed_name_time = result.green_node_changed_name_time_first_error;
            } else {
                continue;
            }

            size_t offset = GetTimeOffsetMs(green_node_changed_name_time, bg_trigger_time);
            largest_offset = offset > largest_offset
                ? offset
                : largest_offset;
        }
        return largest_offset;
    }

    size_t GetSwitchoverCompleteTimeFromStatusTable() {
        std::chrono::steady_clock::time_point bg_trigger_time = GetBgTriggerTime();
        size_t largest_offset = 0;
        for (const auto& [_, result] : results) {
            const auto& itr = result.green_status_time.find("SWITCHOVER_COMPLETED");
            if (itr != result.green_status_time.end()) {
                size_t result_offset = GetTimeOffsetMs(itr->second, bg_trigger_time);
                largest_offset = result_offset > largest_offset
                    ? result_offset
                    : largest_offset;
            }
        }
        return largest_offset;
    }

    std::chrono::steady_clock::time_point GetSwitchoverInitiatedTime(
        std::chrono::steady_clock::time_point bg_trigger_time)
    {
        std::chrono::steady_clock::time_point earliest_time = empty_time_point;
        for (const auto& [_, result] : results) {
            const auto& blue_itr = result.blue_status_time.find("SWITCHOVER_INITIATED");
            if (blue_itr != result.blue_status_time.end() && blue_itr->second != empty_time_point) {
                earliest_time = earliest_time == empty_time_point
                    ? blue_itr->second
                    : earliest_time < blue_itr->second
                        ? earliest_time
                        : blue_itr->second;
            }
            const auto& green_itr = result.green_status_time.find("SWITCHOVER_INITIATED");
            if (green_itr != result.green_status_time.end() && green_itr->second != empty_time_point) {
                earliest_time = earliest_time == empty_time_point
                    ? green_itr->second
                    : earliest_time < green_itr->second
                        ? earliest_time
                        : green_itr->second;
            }
        }
        return earliest_time;
    }

    std::chrono::steady_clock::time_point GetSwitchoverInProgressTime(
        std::chrono::steady_clock::time_point bg_trigger_time)
    {
        std::chrono::steady_clock::time_point latest_time = empty_time_point;
        for (const auto& [_, result] : results) {
            const auto& blue_itr = result.blue_status_time.find("SWITCHOVER_IN_PROGRESS");
            if (blue_itr != result.blue_status_time.end() && blue_itr->second != empty_time_point) {
                latest_time = blue_itr->second > latest_time
                    ? blue_itr->second
                    : latest_time;
            }
            const auto& green_itr = result.green_status_time.find("SWITCHOVER_IN_PROGRESS");
            if (green_itr != result.green_status_time.end() && green_itr->second != empty_time_point) {
                latest_time = green_itr->second > latest_time
                    ? green_itr->second
                    : latest_time;
            }
        }
        return latest_time;
    }

    size_t CountSuccessfulConnectsAfterSwitchover(
        std::chrono::steady_clock::time_point bg_trigger_time,
        size_t switchover_complete_time)
    {
        size_t count = 0;
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_connect_times) {
                count += GetTimeOffsetMs(x.start_time, bg_trigger_time) > switchover_complete_time
                         && !x.error.has_value()
                    ? 1
                    : 0;
            }
        }
        return count;
    }

    size_t CountUnsuccessfulConnectsAfterSwitchover(
        std::chrono::steady_clock::time_point bg_trigger_time,
        size_t switchover_complete_time)
    {
        size_t count = 0;
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_connect_times) {
                count += GetTimeOffsetMs(x.start_time, bg_trigger_time) > switchover_complete_time
                         && x.error.has_value()
                    ? 1
                    : 0;
            }
        }
        return count;
    }

    size_t CountSuccessfulExecutesAfterSwitchover() {
        size_t count = 0;
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_post_switchover_execute_times) {
                count += !x.error.has_value()
                    ? 1
                    : 0;
            }
        }
        return count;
    }

    size_t CountUnsuccessfulExecutesAfterSwitchover() {
        size_t count = 0;
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_post_switchover_execute_times) {
                count += x.error.has_value()
                    ? 1
                    : 0;
            }
        }
        return count;
    }

    void LogUnsuccessfulConnectionAfterSwitchover(
        std::chrono::steady_clock::time_point bg_trigger_time,
        size_t switchover_complete_time)
    {
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_connect_times) {
                size_t offset = GetTimeOffsetMs(x.start_time, bg_trigger_time);
                if (offset > switchover_complete_time
                    && x.error.has_value())
                {
                    ThreadSynchronization::Print("Unsuccessful connection at offset " + std::to_string(offset) +
                        "ms after switchover at (" + std::to_string(switchover_complete_time) + "ms): " +
                        x.error.value());
                }
            }
        }
    }

    void LogUnsuccessfulExecutionAfterSwitchover() {
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_post_switchover_execute_times) {
                if (x.error.has_value()) {
                    ThreadSynchronization::Print("Unsuccessful execution: " + x.error.value());
                }
            }
        }
    }

    size_t GetTimeOffsetMs(
        std::chrono::steady_clock::time_point time_stamp, std::chrono::steady_clock::time_point bg_trigger_time)
    {
        return bg_trigger_time > time_stamp
            ? 0
            : std::chrono::duration_cast<std::chrono::milliseconds>(time_stamp - bg_trigger_time).count();
    }

    void AssertTest() {
        AssertSwitchoverCompleted();

        std::chrono::steady_clock::time_point bg_trigger_time = GetBgTriggerTime();
        ASSERT_NE(empty_time_point, bg_trigger_time);
        AssertWrapperBehavior(bg_trigger_time);
    }

    void AssertSwitchoverCompleted() {
        size_t switchover_completed_time_from_status_table = GetSwitchoverCompleteTimeFromStatusTable();
        EXPECT_NE(0, switchover_completed_time_from_status_table);

        for (const auto& [host_id, result] : results) {
            // Is Green Instance
            if (host_id.find("-green-") != std::string::npos) {
                std::chrono::steady_clock::time_point green_node_changed_name_time =
                    result.green_node_changed_name_time_first_success != empty_time_point
                    ? result.green_node_changed_name_time_first_success
                    : result.green_node_changed_name_time_first_error;
                EXPECT_NE(empty_time_point, green_node_changed_name_time);
            }
        }
    }

    void AssertWrapperBehavior(std::chrono::steady_clock::time_point bg_trigger_time) {
        size_t switchover_complete_time = GetSwitchoverCompleteTime();

        // Log Timing
        ThreadSynchronization::Print("BG Trigger Time: " + std::to_string(bg_trigger_time.time_since_epoch().count()));
        ThreadSynchronization::Print("\tOffset from global start: " + std::to_string(GetTimeOffsetMs(bg_trigger_time, global_start_time)) + "ms");
        ThreadSynchronization::Print("Switchover Complete Time (ms): " + std::to_string(switchover_complete_time));

        // Gather Metrics
        size_t successful_connections = CountSuccessfulConnectsAfterSwitchover(
            bg_trigger_time, switchover_complete_time);
        size_t successful_executions = CountSuccessfulExecutesAfterSwitchover();

        size_t unsuccessful_connections = CountUnsuccessfulConnectsAfterSwitchover(
            bg_trigger_time, switchover_complete_time);
        size_t unsuccessful_executions = CountUnsuccessfulExecutesAfterSwitchover();

        // Log Metrics
        ThreadSynchronization::Print("Successful Wrapper Connection after Switchover: " + std::to_string(successful_connections));
        ThreadSynchronization::Print("Successful Wrapper Execution after Switchover: " + std::to_string(successful_executions));
        ThreadSynchronization::Print("Unsuccessful Wrapper Connection after Switchover: " + std::to_string(unsuccessful_connections));
        ThreadSynchronization::Print("Unsuccessful Wrapper Execution after Switchover: " + std::to_string(unsuccessful_executions));

        // Log Details of Unsuccessful Operations
        if (unsuccessful_connections > 0) {
            LogUnsuccessfulConnectionAfterSwitchover(bg_trigger_time, switchover_complete_time);
        }
        if (unsuccessful_executions > 0) {
            LogUnsuccessfulExecutionAfterSwitchover();
        }

        // Assert Metrics
        EXPECT_EQ(0, unsuccessful_connections);
        EXPECT_EQ(0, unsuccessful_executions);
        EXPECT_TRUE(successful_connections > 0);
        EXPECT_TRUE(successful_executions > 0);

        AssertNoConnectionsToOldBlueCluster(bg_trigger_time);
    }

    void AssertNoConnectionsToOldBlueCluster(std::chrono::steady_clock::time_point bg_trigger_time) {
        // Earliest timepoint which switchover initiated
        std::chrono::steady_clock::time_point switchover_initiated_time = GetSwitchoverInitiatedTime(bg_trigger_time);
        // Latest timepoint where switchover still in progress
        std::chrono::steady_clock::time_point switchover_in_process_time = GetSwitchoverInProgressTime(bg_trigger_time);

        ThreadSynchronization::Print("Earliest Status for Switchover Initiated: " + std::to_string(switchover_initiated_time.time_since_epoch().count()) + "ms");
        ThreadSynchronization::Print("\tOffset from global start: " + std::to_string(GetTimeOffsetMs(switchover_initiated_time, global_start_time)) + "ms");
        ThreadSynchronization::Print("Latest Status for Switchover Inprogress: " + std::to_string(switchover_in_process_time.time_since_epoch().count()) + "ms");
        ThreadSynchronization::Print("\tOffset from global start: " + std::to_string(GetTimeOffsetMs(switchover_in_process_time, global_start_time)) + "ms");

        EXPECT_NE(empty_time_point, switchover_initiated_time);
        EXPECT_NE(empty_time_point, switchover_in_process_time);

        // Verify connections go to blue (none to green) before switchover initiated
        size_t connections_before_switchover = 0;
        size_t connections_to_blue_before_switchover = 0;
        size_t connections_to_green_before_switchover = 0;

        size_t initiated_time_offset = GetTimeOffsetMs(bg_trigger_time, global_start_time);
        for (const auto& [_, result] : results) {
            for (const auto& host_verification_result : result.host_verification_results) {
                size_t offset = GetTimeOffsetMs(host_verification_result.timestamp, bg_trigger_time);
                if (initiated_time_offset > offset) {
                    continue;
                }

                if (host_verification_result.error.empty()) {
                    connections_before_switchover += 1;
                    if (host_verification_result.connected_to_blue) {
                        connections_to_blue_before_switchover += 1;
                    } else {
                        connections_to_green_before_switchover += 1;
                    }
                }
            }
        }

        EXPECT_EQ(connections_before_switchover, connections_to_blue_before_switchover);
        EXPECT_EQ(0, connections_to_green_before_switchover);

        // Verify no connections go to old-blue after switchover in progress
        size_t total_verifications_after_switchover_initiated = 0;
        size_t connections_to_blue_after_switchover_initiated = 0;
        size_t connections_to_green_after_switchover_initiated = 0;

        size_t in_progress_time_offset = GetTimeOffsetMs(switchover_in_process_time, global_start_time);
        for (const auto& [_, result] : results) {
            for (const auto& host_verification_result : result.host_verification_results) {
                size_t offset = GetTimeOffsetMs(host_verification_result.timestamp, bg_trigger_time);
                if (in_progress_time_offset > offset) {
                    continue;
                }

                if (host_verification_result.error.empty()) {
                    total_verifications_after_switchover_initiated += 1;
                    if (host_verification_result.connected_to_blue) {
                        connections_to_blue_after_switchover_initiated += 1;
                        ThreadSynchronization::Print("Connected to old blue cluster at offset: " + std::to_string(offset) +
                             "ms after switchover in progress at: " + std::to_string(in_progress_time_offset) +
                             "ms connected to: " + host_verification_result.connected_host +
                             ", original blue: " + host_verification_result.original_blue_ip);
                    } else {
                        connections_to_green_after_switchover_initiated += 1;
                    }
                }
            }
        }

        ThreadSynchronization::Print("After switchover in progress: " + std::to_string(switchover_in_process_time.time_since_epoch().count()));
        ThreadSynchronization::Print("\tOffset from global: " + std::to_string(GetTimeOffsetMs(switchover_in_process_time, global_start_time)));
        ThreadSynchronization::Print("Total Connections: " + std::to_string(total_verifications_after_switchover_initiated));
        ThreadSynchronization::Print("\tto old host: " + std::to_string(connections_to_blue_after_switchover_initiated));
        ThreadSynchronization::Print("\tto new host: " + std::to_string(connections_to_green_after_switchover_initiated));

        EXPECT_EQ(0, connections_to_blue_after_switchover_initiated);
        EXPECT_TRUE(total_verifications_after_switchover_initiated > 0);
    }

    std::thread CreateDirectBlueIdleConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, std::chrono::steady_clock::time_point& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetDirectConnStr(host);
            SQLHDBC hdbc;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            ThreadSynchronization::Print("[DirectBlueIdleConnectivityMonitoringThread: " + host_id + "] started monitoring idle connection.");
            while (!ThreadSynchronization::stop_flag) {
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    ThreadSynchronization::Print("[DirectBlueIdleConnectivityMonitoringThread: " + host_id + "] lost connection.");
                    results = std::chrono::steady_clock::now();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateDirectBlueIdleConnectivityMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateDirectBlueConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, std::chrono::steady_clock::time_point& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetDirectConnStr(host);
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            ThreadSynchronization::Print("[DirectBlueConnectivityMonitoringThread: " + host_id + "] started monitoring connection.");
            SQLHSTMT hstmt = SQL_NULL_HSTMT;
            SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
            while (!ThreadSynchronization::stop_flag) {
                if (!SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, simple_select_query))) {
                    ThreadSynchronization::Print("[DirectBlueConnectivityMonitoringThread: " + host_id + "] failed to execute.");
                    results = std::chrono::steady_clock::now();
                    break;
                }
                SQLCloseCursor(hstmt);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, hstmt);

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateDirectBlueConnectivityMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateWrapperBlueIdleConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, std::chrono::steady_clock::time_point& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Actual Work
            ThreadSynchronization::Print("[WrapperBlueIdleConnectivityMonitoringThread: " + host_id + "] started monitoring idle connection.");
            while (!ThreadSynchronization::stop_flag) {
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    ThreadSynchronization::Print("[WrapperBlueIdleConnectivityMonitoringThread: " + host_id + "] lost connection.");
                    results = std::chrono::steady_clock::now();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateWrapperBlueIdleConnectivityMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateWrapperBlueExecutingConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host,
        std::deque<TimeHolder>& pre_switch_results, std::deque<TimeHolder>& post_switch_results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            ThreadSynchronization::Print("[WrapperBlueExecutingConnectivityMonitoringThread: " + host_id + "] started monitoring connection.");

            // Phase 1 - Execute until connection closes during switchover
            SQLHSTMT hstmt = SQL_NULL_HSTMT;
            SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
            while (!ThreadSynchronization::stop_flag) {
                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time = empty_time_point;
                if (SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, sleep_query))) {
                    end_time = std::chrono::steady_clock::now();
                    SQLCloseCursor(hstmt);
                    pre_switch_results.push_back(
                        TimeHolder(start_time, end_time)
                    );
                } else {
                    end_time = std::chrono::steady_clock::now();
                    SQLCloseCursor(hstmt);
                    std::string err = ODBC_HELPER::PrintHandleError(hstmt, SQL_HANDLE_STMT);
                    ThreadSynchronization::Print("[WrapperBlueExecutingConnectivityMonitoringThread: " + host_id + "] Failed to connect: " + err);
                    pre_switch_results.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                    if (ODBC_HELPER::IsClosed(hdbc)) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);
            hstmt = SQL_NULL_HSTMT;

            // Phase 2 - Post-Switchover, reconnect and continue executing
            while (!ThreadSynchronization::stop_flag) {
                // Reconnect if needed
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);
                    hstmt = SQL_NULL_HSTMT;
                    SQLDisconnect(hdbc);
                    OpenConnectionWithRetry(hdbc, conn_str);
                    SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
                }

                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time = empty_time_point;
                if (SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, sleep_query))) {
                    end_time = std::chrono::steady_clock::now();
                    SQLCloseCursor(hstmt);
                    post_switch_results.push_back(
                        TimeHolder(start_time, end_time)
                    );
                } else {
                    end_time = std::chrono::steady_clock::now();
                    std::string err = ODBC_HELPER::PrintHandleError(hstmt, SQL_HANDLE_STMT);
                    ThreadSynchronization::Print("[WrapperBlueExecutingConnectivityMonitoringThread: " + host_id + "] Failed to connect: " + err);
                    post_switch_results.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                    SQLCloseCursor(hstmt);
                    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);
                    hstmt = SQL_NULL_HSTMT;
                    SQLDisconnect(hdbc);
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, hstmt);

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateWrapperBlueExecutingConnectivityMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateWrapperBlueNewConnectionMonitoringThread(
        const std::string& host_id, const std::string& host, std::deque<TimeHolder>& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            while (!ThreadSynchronization::stop_flag) {
                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time = empty_time_point;

                if (SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                    end_time = std::chrono::steady_clock::now();
                    results.push_back(
                        TimeHolder(start_time, end_time)
                    );
                } else {
                    end_time = std::chrono::steady_clock::now();
                    std::string err = ODBC_HELPER::PrintHandleError(hdbc, SQL_HANDLE_DBC);
                    ThreadSynchronization::Print("[WrapperBlueNewConnectionMonitoringThread: " + host_id + "] Failed to connect: " + err);
                    results.push_back(
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
            ThreadSynchronization::Print("[CreateWrapperBlueNewConnectionMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateWrapperBlueHostVerificationThread(
        const std::string& host_id, const std::string& host, std::deque<HostVerificationResult>& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);
            const std::string original_ip = GetConnectedServerIp(hdbc);
            SQLDisconnect(hdbc);

            if (original_ip.empty()) {
                GTEST_FAIL() << "Unable to fetch original blue IP from initial connection.";
            }

            ThreadSynchronization::Print("[WrapperBlueHostVerificationThread: " + host_id + "] Original Blue IP: " + original_ip);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            while (!ThreadSynchronization::stop_flag) {
                std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
                if (SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                    std::string latest_ip = GetConnectedServerIp(hdbc);
                    HostVerificationResult result = HostVerificationResult::Success(timestamp, latest_ip, original_ip);
                    results.push_back(result);
                    if (result.connected_to_blue) {
                        ThreadSynchronization::Print("[WrapperBlueHostVerificationThread: " + host_id + "] Connected to Blue Cluster on IP: " + latest_ip);
                    } else {
                        ThreadSynchronization::Print("[WrapperBlueHostVerificationThread: " + host_id + "] Connected to Green Cluster on IP: " + latest_ip);
                    }
                } else {
                    std::string err = ODBC_HELPER::PrintHandleError(hdbc, SQL_HANDLE_DBC) + " - ERROR";
                    ThreadSynchronization::Print("[WrapperBlueHostVerificationThread: " + host_id + "] Failed to connect: " + err);
                    results.push_back(
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
            ThreadSynchronization::Print("[CreateWrapperBlueHostVerificationThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateGreenDnsMonitoringThread(
        const std::string& host_id, const std::string& host, std::chrono::steady_clock::time_point& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            const std::string original_ip_address = TEST_UTILS::HostToIp(host);
            ThreadSynchronization::Print("[GreenDnsMonitoringThread: " + host_id + "] IP Address: " + original_ip_address);

            while (!ThreadSynchronization::stop_flag) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                const std::string current_ip = TEST_UTILS::HostToIp(host);
                if (current_ip != original_ip_address) {
                    ThreadSynchronization::Print("[GreenDnsMonitoringThread: " + host_id + "] IP Address Changed: " + current_ip);
                    results = std::chrono::steady_clock::now();
                    break;
                }
            }

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateGreenDnsMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateBlueDnsMonitoringThread(
        const std::string& host_id, const std::string& host,
        std::chrono::steady_clock::time_point& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            const std::string original_ip_address = TEST_UTILS::HostToIp(host);
            ThreadSynchronization::Print("[BlueDnsMonitoringThread: " + host_id + "] IP Address: " + original_ip_address);

            while (!ThreadSynchronization::stop_flag) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                const std::string current_ip = TEST_UTILS::HostToIp(host);
                if (current_ip != original_ip_address) {
                    ThreadSynchronization::Print("[BlueDnsMonitoringThread: " + host_id + "] IP Address Changed: " + current_ip);
                    results = std::chrono::steady_clock::now();
                    break;
                } else if (current_ip.empty()) {
                    ThreadSynchronization::Print("[BlueDnsMonitoringThread: " + host_id + "] Unable to retrieve IP");
                    results = std::chrono::steady_clock::now();
                    break;
                }
            }

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateBlueDnsMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateDirectTopologyMonitoringThread(
        const std::string& host_id, const std::string& host, std::unordered_map<std::string, std::chrono::steady_clock::time_point>& results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetDirectConnStr(host);
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);
            const int BUFFER_SIZE = 512;

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();
            std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now()
                + std::chrono::minutes(15);

            ThreadSynchronization::Print("[DirectTopologyMonitoringThread: " + host_id + "] Starting BG Status Monitoring.");

            while (!ThreadSynchronization::stop_flag && std::chrono::steady_clock::now() < end_time) {
                if (ODBC_HELPER::IsClosed(hdbc)) {
                    SQLDisconnect(hdbc);
                    OpenConnectionWithRetry(hdbc, conn_str);
                    ThreadSynchronization::Print("[DirectTopologyMonitoringThread: " + host_id + "] Connection re-opened.");
                }

                SQLHSTMT hstmt = SQL_NULL_HSTMT;
                SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
                if (SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, bg_status_query))) {
                    SQLLEN len = 0;
                    SQLTCHAR role[BUFFER_SIZE] = { 0 };
                    SQLTCHAR status[BUFFER_SIZE] = { 0 };
                    SQLBindCol(hstmt, 5, SQL_C_TCHAR, &role, BUFFER_SIZE, &len);
                    SQLBindCol(hstmt, 6, SQL_C_TCHAR, &status, BUFFER_SIZE, &len);

                    if (SQL_SUCCEEDED(SQLFetch(hstmt))) {
                        std::string status_str = STRING_HELPER::SqltcharToAnsi(status);
                        bool is_green = STRING_HELPER::SqltcharToAnsi(role) == "BLUE_GREEN_DEPLOYMENT_TARGET";
                        if (is_green) {
                            ThreadSynchronization::Print("[DirectTopologyMonitoringThread: " + host_id + "] Green Status Changed to: " + status_str);
                            results.try_emplace(
                                status_str,
                                std::chrono::steady_clock::now()
                            );
                        } else {
                            ThreadSynchronization::Print("[DirectTopologyMonitoringThread: " + host_id + "] Blue Status Changed to: " + status_str);
                            results.try_emplace(
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
            ThreadSynchronization::Print("[CreateDirectTopologyMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateWrapperGreenConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host,
        std::deque<TimeHolder>& execution_results, std::chrono::steady_clock::time_point& connection_results)
    {
        return std::thread([&](const std::string& host_id, const std::string& host){
            // Setup
            std::string conn_str = GetBlueGreenEnabledConnStr(host);
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            ThreadSynchronization::Print("[WrapperGreenConnectivityMonitoringThread: " + host_id + "] started monitoring connection.");
            SQLHSTMT hstmt = SQL_NULL_HSTMT;
            SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
            std::chrono::steady_clock::time_point start_time = empty_time_point;
            std::chrono::steady_clock::time_point end_time = empty_time_point;
            while (!ThreadSynchronization::stop_flag) {
                start_time = std::chrono::steady_clock::now();
                if (!SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(hstmt, simple_select_query))) {
                    end_time = std::chrono::steady_clock::now();
                    std::string err = ODBC_HELPER::PrintHandleError(hstmt, SQL_HANDLE_STMT);
                    execution_results.push_back(TimeHolder(start_time, end_time, err));
                    if (ODBC_HELPER::IsClosed(hdbc)) {
                        ThreadSynchronization::Print("[WrapperGreenConnectivityMonitoringThread: " + host_id + "] failed to execute.");
                        connection_results = std::chrono::steady_clock::now();
                    }
                    break;
                }
                end_time = std::chrono::steady_clock::now();
                execution_results.push_back(TimeHolder(start_time, end_time));
                SQLCloseCursor(hstmt);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, hstmt);

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateWrapperGreenConnectivityMonitoringThread] Finished.");
        }, host_id, host);
    }

    std::thread CreateGreenIamConnectivityMonitoringThread(
        const std::string& host_id, const std::string& host, std::chrono::steady_clock::time_point& results,
        std::deque<TimeHolder>& result_queue, const std::string& iam_host, const std::string& thread_prefix,
        bool notify_on_first_error, bool exit_on_first_success)
    {
        return std::thread([&](const std::string& host_id, const std::string& host, const std::string& iam_host, const std::string& thread_prefix) {
            // Setup
            const std::string iam_conn_str_prefix = ConnectionStringBuilder(conn_str_builder.getString())
                .withServer(host)
                .withUID(test_iam_user)
                .getString();
            const std::string green_node_original_ip = TEST_UTILS::HostToIp(host);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] started monitoring connection.");

            // Work
            SQLHDBC hdbc = SQL_NULL_HDBC;
            SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            while (!ThreadSynchronization::stop_flag) {
                const std::string iam_token = rds_client.GenerateConnectAuthToken(
                    iam_host.c_str(), test_region.c_str(), test_port, test_iam_user.c_str());
                const std::string conn_str = ConnectionStringBuilder(iam_conn_str_prefix)
                    .withPWD(iam_token)
                    .getString();

                std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point end_time = empty_time_point;
                if (SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                    end_time = std::chrono::steady_clock::now();
                    result_queue.push_back(
                        TimeHolder(start_time, end_time)
                    );

                    if (exit_on_first_success) {
                        ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] Successfully connecting, exiting.");
                        results = std::chrono::steady_clock::now();
                        break;
                    }
                } else {
                    end_time = std::chrono::steady_clock::now();
                    std::string err = ODBC_HELPER::PrintHandleError(hdbc, SQL_HANDLE_DBC);
                    ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] Failed to connect: " + err);
                    result_queue.push_back(
                        TimeHolder(start_time, end_time, err)
                    );

                    if (notify_on_first_error
                        && (
                            err.find("Access denied") != std::string::npos
                            || err.find("PAM authentication failed")  != std::string::npos
                        ))
                    {
                        results = std::chrono::steady_clock::now();
                        ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] Exiting thread, first authentication failed: " + err);
                        break;
                    }
                }

                SQLDisconnect(hdbc);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Cleanup
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateGreenIamConnectivityMonitoringThread] Finished.");
        }, host_id, host, iam_host, thread_prefix);
    }

    std::thread CreateBlueGreenSwitchoverTriggerThread(
        const std::string& blue_green_deployment_id, std::unordered_map<std::string, BlueGreenResults>& results)
    {
        return std::thread([&](const std::string& blue_green_deployment_id) {
            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Actual Work
            std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
            for (auto& [_, result] : results) {
                result.threads_sync_time = start_time;
            }

            std::this_thread::sleep_for(std::chrono::seconds(30));

            Aws::RDS::Model::SwitchoverBlueGreenDeploymentRequest request;
            request.WithBlueGreenDeploymentIdentifier(blue_green_deployment_id);
            auto outcome = rds_client.SwitchoverBlueGreenDeployment(request);
            if (!outcome.IsSuccess()) {
                ThreadSynchronization::Print("Failed to send Blue Green Switchover request: " + outcome.GetError().GetMessage());
            } else {
                ThreadSynchronization::Print("Successfully sent Blue Green Switchover request.");
            }

            std::chrono::steady_clock::time_point trigger_time = std::chrono::steady_clock::now();
            for (auto& [_, result] : results) {
                result.bg_trigger_time = trigger_time;
            }

            // Finished
            ThreadSynchronization::ThreadFinished();
            ThreadSynchronization::Print("[CreateBlueGreenSwitchoverTriggerThread] Finished.");
        }, blue_green_deployment_id);
    }
};

TEST_F(BlueGreenIntegrationTest, SwitchoverTest) {
    const std::vector<std::string> topology_instances = GetBlueGreenEndpoints(blue_green_deployment_id);
    if (topology_instances.empty()) {
        GTEST_FAIL() << "Unable to describe Blue Green Endpoints for: " << blue_green_deployment_id;
    }

    auto start_time = std::chrono::steady_clock::now();
    global_start_time = start_time;

    std::vector<std::thread> threads;
    for (std::string host_id : topology_instances) {
        std::string host = host_id + db_conn_str_suffix;

        results.insert_or_assign(host_id, BlueGreenResults());
        BlueGreenResults& result = results.at(host_id);

        // Not Green & Not Old Prefixed Instances
        size_t green_pos = host.find("-green-");
        size_t old_pos = host.find("-old1");
        if (green_pos == std::string::npos && old_pos == std::string::npos) {
            threads.push_back(CreateDirectTopologyMonitoringThread(host_id, host, result.green_status_time));
            threads.push_back(CreateDirectBlueConnectivityMonitoringThread(host_id, host, result.direct_blue_lost_connection_time));
            threads.push_back(CreateDirectBlueIdleConnectivityMonitoringThread(host_id, host, result.direct_blue_idle_lost_connection_time));
            threads.push_back(CreateWrapperBlueIdleConnectivityMonitoringThread(host_id, host, result.wrapper_blue_idle_lost_connection_time));
            threads.push_back(CreateWrapperBlueExecutingConnectivityMonitoringThread(host_id, host, result.blue_wrapper_pre_switchover_execute_times, result.blue_wrapper_post_switchover_execute_times));
            threads.push_back(CreateWrapperBlueNewConnectionMonitoringThread(host_id, host, result.blue_wrapper_connect_times));
            threads.push_back(CreateWrapperBlueHostVerificationThread(host_id, host, result.host_verification_results));
            threads.push_back(CreateBlueDnsMonitoringThread(host_id, host, result.dns_blue_changed_time));
        }
        // Is Green Instance
        else if (green_pos != std::string::npos) {
            threads.push_back(CreateDirectTopologyMonitoringThread(host_id, host, result.green_status_time));
            threads.push_back(CreateWrapperGreenConnectivityMonitoringThread(host_id, host, result.green_wrapper_execute_times, result.wrapper_green_lost_connection_time));
            threads.push_back(CreateGreenDnsMonitoringThread(host_id, host, result.dns_green_removed_time));
            threads.push_back(CreateGreenIamConnectivityMonitoringThread(
                host_id, host, result.green_node_changed_name_time_first_success, result.green_direct_iam_ip_with_blue_node_connect_times,
                "IAM_HOST", "BlueHost", false, true));
            threads.push_back(CreateGreenIamConnectivityMonitoringThread(
                host_id, host, result.green_node_changed_name_time_first_error, result.green_direct_iam_ip_with_green_node_connect_times,
                "IAM_HOST", "GreenHost", true, false));
        }
    }

    threads.push_back(CreateBlueGreenSwitchoverTriggerThread(blue_green_deployment_id, results));

    for (auto& [_, v] : results) {
        v.start_time = start_time;
    }

    // Allow threads to finish immediate work
    ThreadSynchronization::Print("Allow threads setup time");
    std::this_thread::sleep_for(std::chrono::minutes(3));

    // Notify all threads are allocated and ready to start
    ThreadSynchronization::total_threads = threads.size();
    ThreadSynchronization::start_flag = true;
    ThreadSynchronization::start_latch.notify_all();
    ThreadSynchronization::Print("Threads started");

    ThreadSynchronization::WaitForFinish(std::chrono::minutes(6));
    ThreadSynchronization::Print("Threads finished");

    // Stop threads
    ThreadSynchronization::stop_flag = true;
    // Allow threads to finish immediate work
    std::this_thread::sleep_for(std::chrono::minutes(3));
    ThreadSynchronization::Print("Stopping threads");

    // Assume threads hung and detach
    ThreadSynchronization::Print("Detach threads");
    for (auto& thread : threads) {
        thread.detach();
    }

    ThreadSynchronization::Print("-------------------------------------------------------------------------------------");
    ThreadSynchronization::Print("Tests finished");
    ThreadSynchronization::Print("-------------------------------------------------------------------------------------");
    PrintMetrics();
    AssertTest();
}
