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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

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

namespace {
    const std::regex BG_GREEN_HOST_PATTERN(R"#(.*(-green-[0-9a-z]{6})\..*)#", std::regex_constants::icase);
    const std::regex BG_GREEN_HOSTID_PATTERN(R"#((.*)-green-[0-9a-z]{6})#", std::regex_constants::icase);

    std::string RemoveGreenInstancePrefix(const std::string& host) {
        if (host.empty()) {
            return host;
        }
        std::smatch match;
        if (!std::regex_match(host, match, BG_GREEN_HOST_PATTERN)) {
            std::smatch host_id_match;
            if (!std::regex_match(host, host_id_match, BG_GREEN_HOSTID_PATTERN)) {
                return host;
            }
            return host_id_match.size() > 1 ? host_id_match[1].str() : host;
        }
        std::string prefix = match.size() > 1 ? match[1].str() : "";
        std::string converted_host = host;
        if (!prefix.empty()) {
            size_t begin_idx = host.find(prefix);
            if (begin_idx != std::string::npos) {
                converted_host = converted_host.replace(begin_idx, begin_idx + prefix.length(), ".");
            }
        }
        return converted_host;
    }
} // namespace

namespace ThreadSynchronization {
    int total_threads = 0;

    std::condition_variable start_latch;
    std::atomic<int> ready_count = 0;
    std::atomic<bool> start_flag = false;
    std::mutex start_mutex;
    std::mutex finish_mutex;

    void ReadyAndWait() {
        ThreadSynchronization::ready_count++;
        std::unique_lock<std::mutex> lock(start_mutex);
        ThreadSynchronization::start_latch.wait_for(lock, std::chrono::minutes(5), [] {
            return ThreadSynchronization::start_flag
                && ThreadSynchronization::ready_count == ThreadSynchronization::total_threads;
        });
        ThreadSynchronization::start_latch.notify_one();
    };

    std::condition_variable finish_latch;
    std::atomic<int> finish_count = 0;
    std::atomic<bool> stop_flag = false;

    void ThreadFinished() {
        ThreadSynchronization::finish_count++;
        finish_latch.notify_all();
    }

    void WaitForFinish(std::chrono::milliseconds wait_ms) {
        std::unique_lock<std::mutex> lock(finish_mutex);
        ThreadSynchronization::finish_latch.wait_for(lock, wait_ms, [] {
            return finish_count == total_threads;
        });
        ThreadSynchronization::finish_latch.notify_one();
    };

    std::mutex cout_mutex;

    std::mutex env_mutex;

    void Print(const std::string& message) {
        // std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << message << std::endl;
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
    Aws::RDS::RDSClient rds_client;

    std::chrono::steady_clock::time_point empty_time_point{};
    std::chrono::steady_clock::time_point global_start_time;

    ConnectionStringBuilder conn_str_builder;
    std::string test_iam_user = TEST_UTILS::GetEnvVar("TEST_IAM_USER", "");
    std::string test_base_driver = TEST_UTILS::GetEnvVar("TEST_BASE_DRIVER", "");

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
        Aws::RDS::RDSClientConfiguration client_config;
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
            .withSslMode("require")
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
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::string GetDirectConnStr(const std::string& host, const bool& use_iam = true) {
        return use_iam
            ? ConnectionStringBuilder(conn_str_builder.getString())
                .withUID(test_iam_user)
                .withAuthMode("IAM")
                .withSslMode("require")
                .withExtraUrlEncode(true)
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
                .withSslMode("require")
                .withExtraUrlEncode(true)
                .withBlueGreenEnabled(true)
                .withEnableClusterFailover(false)
                .withExtraUrlEncode(true)
                .withServer(host)
                .getString()
            : ConnectionStringBuilder(conn_str_builder.getString())
                .withUID(test_uid)
                .withPWD(test_pwd)
                .withBlueGreenEnabled(true)
                .withEnableClusterFailover(false)
                .withServer(host)
                .getString();
    }

    std::string FormatTimeOffset(std::chrono::steady_clock::time_point tp, std::chrono::steady_clock::time_point bg_trigger_time) {
        if (tp == empty_time_point) {
            return "-";
        }
        return std::to_string(GetTimeOffsetMs(tp, bg_trigger_time)) + " ms";
    }

    std::string TruncateError(const std::optional<std::string>& error, size_t max_len = 100) {
        if (!error.has_value()) {
            return "";
        }
        std::string e = error.value();
        // Replace newlines with spaces
        for (auto& c : e) {
            if (c == '\n') c = ' ';
        }
        if (e.length() > max_len) {
            e = e.substr(0, max_len) + "...";
        }
        return e;
    }

    long long GetPercentile(const std::deque<TimeHolder>& times, double percentile) {
        if (times.empty()) {
            return 0;
        }
        std::vector<long long> durations;
        durations.reserve(times.size());
        for (const auto& t : times) {
            durations.push_back(
                std::chrono::duration_cast<std::chrono::milliseconds>(t.end_time - t.start_time).count());
        }
        std::sort(durations.begin(), durations.end());
        int rank = percentile == 0.0 ? 1 : static_cast<int>(std::ceil(percentile / 100.0 * durations.size()));
        return durations[rank - 1];
    }

    // Sort entries: blue instances first, then green, alphabetically within each group
    std::vector<std::pair<std::string, BlueGreenResults*>> GetSortedEntries() {
        std::vector<std::pair<std::string, BlueGreenResults*>> entries;
        for (auto& [host_id, result] : results) {
            entries.push_back({host_id, &result});
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            bool a_green = a.first.find("-green-") != std::string::npos;
            bool b_green = b.first.find("-green-") != std::string::npos;
            if (a_green != b_green) {
                return !a_green; // blue first
            }
            return RemoveGreenInstancePrefix(a.first) < RemoveGreenInstancePrefix(b.first);
        });
        return entries;
    }

    void PrintNodeStatusTimes(const std::string& node, const BlueGreenResults& result,
        std::chrono::steady_clock::time_point bg_trigger_time)
    {
        // Merge blue and green status maps, sort by time
        std::map<std::chrono::steady_clock::time_point, std::string> sorted_statuses;
        for (const auto& [status, tp] : result.blue_status_time) {
            sorted_statuses[tp] = status;
        }
        for (const auto& [status, tp] : result.green_status_time) {
            sorted_statuses.try_emplace(tp, status);
        }

        // Collect unique status names in time order
        std::vector<std::string> ordered_statuses;
        for (const auto& [_, status] : sorted_statuses) {
            if (std::find(ordered_statuses.begin(), ordered_statuses.end(), status) == ordered_statuses.end()) {
                ordered_statuses.push_back(status);
            }
        }

        std::ostringstream oss;
        oss << "\n" << node << ":\n";
        oss << "+------------------------------+---------------------+---------------------+\n";
        oss << "| Status                       | SOURCE              | TARGET              |\n";
        oss << "+------------------------------+---------------------+---------------------+\n";

        for (const auto& status : ordered_statuses) {
            std::string source_time;
            auto blue_itr = result.blue_status_time.find(status);
            if (blue_itr != result.blue_status_time.end()) {
                source_time = std::to_string(GetTimeOffsetMs(blue_itr->second, bg_trigger_time)) + " ms";
            }

            std::string target_time;
            auto green_itr = result.green_status_time.find(status);
            if (green_itr != result.green_status_time.end()) {
                target_time = std::to_string(GetTimeOffsetMs(green_itr->second, bg_trigger_time)) + " ms";
            }

            oss << "| " << std::left << std::setw(29) << status
                << "| " << std::setw(20) << source_time
                << "| " << std::setw(20) << target_time << "|\n";
        }
        oss << "+------------------------------+---------------------+---------------------+";
        ThreadSynchronization::Print(oss.str());
    }

    void PrintDurationTimes(const std::string& node, const std::string& title,
        const std::deque<TimeHolder>& times, std::chrono::steady_clock::time_point bg_trigger_time)
    {
        if (times.empty()) {
            return;
        }

        long long p99 = GetPercentile(times, 99.0);

        std::ostringstream oss;
        oss << "\n" << node << ": " << title << "\n";
        oss << "+---------------------+------------------------------+----------------------------------------------+\n";
        oss << "| Connect at (ms)     | Connect time/duration (ms)   | Error                                        |\n";
        oss << "+---------------------+------------------------------+----------------------------------------------+\n";
        oss << "| " << std::left << std::setw(20) << "p99"
            << "| " << std::setw(29) << p99
            << "| " << std::setw(45) << "" << "|\n";
        oss << "+---------------------+------------------------------+----------------------------------------------+\n";

        auto print_row = [&](const TimeHolder& th) {
            long long offset = GetTimeOffsetMs(th.start_time, bg_trigger_time);
            long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(th.end_time - th.start_time).count();
            oss << "| " << std::left << std::setw(20) << offset
                << "| " << std::setw(29) << duration
                << "| " << std::setw(45) << TruncateError(th.error) << "|\n";
        };

        // First entry
        print_row(times.front());

        // Entries exceeding p99
        for (const auto& th : times) {
            long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(th.end_time - th.start_time).count();
            if (duration > p99) {
                print_row(th);
            }
        }

        // Last entry (if different from first)
        if (times.size() > 1) {
            print_row(times.back());
        }

        oss << "+---------------------+------------------------------+----------------------------------------------+";
        ThreadSynchronization::Print(oss.str());
    }

    void PrintHostVerificationResults(const std::string& node,
        const std::deque<HostVerificationResult>& host_results,
        std::chrono::steady_clock::time_point bg_trigger_time)
    {
        long long total_attempts = static_cast<long long>(host_results.size());
        long long total_successful = 0;
        long long total_unsuccessful = 0;
        long long connections_to_blue = 0;
        long long connections_to_green = 0;

        for (const auto& r : host_results) {
            if (r.error.empty()) {
                total_successful++;
                if (r.connected_to_blue) {
                    connections_to_blue++;
                } else {
                    connections_to_green++;
                }
            } else {
                total_unsuccessful++;
            }
        }

        std::chrono::steady_clock::time_point switchover_in_progress_time = GetSwitchoverInProgressTime();
        long long in_progress_offset = GetTimeOffsetMs(switchover_in_progress_time, bg_trigger_time);

        long long connections_to_blue_after_switchover = 0;
        for (const auto& r : host_results) {
            long long offset = GetTimeOffsetMs(r.timestamp, bg_trigger_time);
            if (offset > in_progress_offset && r.connected_to_blue) {
                connections_to_blue_after_switchover++;
            }
        }

        std::ostringstream oss;
        oss << "\n" << node << ": Host Verification Results\n";
        oss << "+------------------------------------------------------------------------+---------------------+\n";
        oss << "| Metric                                                                 | Value               |\n";
        oss << "+------------------------------------------------------------------------+---------------------+\n";
        oss << "| " << std::left << std::setw(71) << "Total verification attempts"
            << "| " << std::setw(20) << total_attempts << "|\n";
        oss << "| " << std::setw(71) << "Total successful connection and verification attempts"
            << "| " << std::setw(20) << total_successful << "|\n";
        oss << "| " << std::setw(71) << "Total unsuccessful/dropped attempts (expected during switchover)"
            << "| " << std::setw(20) << total_unsuccessful << "|\n";
        oss << "| " << std::setw(71) << "Total successful connections to blue"
            << "| " << std::setw(20) << connections_to_blue << "|\n";
        oss << "| " << std::setw(71) << "Total successful connections to green"
            << "| " << std::setw(20) << connections_to_green << "|\n";
        oss << "| " << std::setw(71) << "Connections to old blue after switchover in progress (ERROR if not 0)"
            << "| " << std::setw(20) << connections_to_blue_after_switchover << "|\n";
        oss << "+------------------------------------------------------------------------+---------------------+";

        if (connections_to_blue_after_switchover > 0) {
            oss << "\n| " << std::left << std::setw(20) << "Time (ms)"
                << "| Connection to blue after switchover                                    |\n";
            oss << "+------------------------------------------------------------------------+---------------------+\n";
            for (const auto& r : host_results) {
                long long offset = GetTimeOffsetMs(r.timestamp, bg_trigger_time);
                if (offset > in_progress_offset && r.connected_to_blue) {
                    oss << "| " << std::left << std::setw(20) << offset
                        << "| " << std::setw(71)
                        << (r.connected_host + " (original: " + r.original_blue_ip + ")") << "|\n";
                }
            }
            oss << "+------------------------------------------------------------------------+---------------------+";
        }

        ThreadSynchronization::Print(oss.str());
    }

    void PrintMetrics() {
        std::chrono::steady_clock::time_point bg_trigger_time = GetBgTriggerTime();
        if (bg_trigger_time == empty_time_point) {
            ThreadSynchronization::Print("PrintMetrics: No BG trigger time available.");
            return;
        }

        auto sorted_entries = GetSortedEntries();

        // Main metrics table
        {
            std::ostringstream oss;
            oss << "\n===================== Blue/Green Switchover Metrics =====================\n";
            oss << std::left
                << std::setw(40) << "Instance"
                << std::setw(14) << "startTime"
                << std::setw(14) << "threadsSync"
                << std::setw(16) << "Blue idle drop"
                << std::setw(16) << "Blue SEL drop"
                << std::setw(16) << "Wrap idle drop"
                << std::setw(16) << "Green SEL drop"
                << std::setw(16) << "Blue DNS"
                << std::setw(16) << "Green DNS"
                << std::setw(16) << "Green cert"
                << "\n";

            for (const auto& [host_id, result] : sorted_entries) {
                long long start_offset = GetTimeOffsetMs(result->start_time, bg_trigger_time);
                long long sync_offset = GetTimeOffsetMs(result->threads_sync_time, bg_trigger_time);

                std::chrono::steady_clock::time_point green_node_time =
                    result->green_node_changed_name_time_first_success != empty_time_point
                        ? result->green_node_changed_name_time_first_success
                        : result->green_node_changed_name_time_first_error;

                oss << std::left
                    << std::setw(40) << host_id
                    << std::setw(14) << (std::to_string(start_offset) + " ms")
                    << std::setw(14) << (std::to_string(sync_offset) + " ms")
                    << std::setw(16) << FormatTimeOffset(result->direct_blue_idle_lost_connection_time, bg_trigger_time)
                    << std::setw(16) << FormatTimeOffset(result->direct_blue_lost_connection_time, bg_trigger_time)
                    << std::setw(16) << FormatTimeOffset(result->wrapper_blue_idle_lost_connection_time, bg_trigger_time)
                    << std::setw(16) << FormatTimeOffset(result->wrapper_green_lost_connection_time, bg_trigger_time)
                    << std::setw(16) << FormatTimeOffset(result->dns_blue_changed_time, bg_trigger_time)
                    << std::setw(16) << FormatTimeOffset(result->dns_green_removed_time, bg_trigger_time)
                    << std::setw(16) << FormatTimeOffset(green_node_time, bg_trigger_time)
                    << "\n";
            }
            oss << "=========================================================================";
            ThreadSynchronization::Print(oss.str());
        }

        // Node status times
        for (const auto& [host_id, result] : sorted_entries) {
            if (result->blue_status_time.empty() && result->green_status_time.empty()) {
                continue;
            }
            PrintNodeStatusTimes(host_id, *result, bg_trigger_time);
        }

        // Wrapper connection times to Blue
        for (const auto& [host_id, result] : sorted_entries) {
            if (result->blue_wrapper_connect_times.empty()) {
                continue;
            }
            PrintDurationTimes(host_id, "Wrapper connection time (ms) to Blue",
                result->blue_wrapper_connect_times, bg_trigger_time);
        }

        // IAM (green token) connection times to Green
        for (const auto& [host_id, result] : sorted_entries) {
            if (result->green_direct_iam_ip_with_green_node_connect_times.empty()) {
                continue;
            }
            PrintDurationTimes(host_id, "Wrapper IAM (green token) connection time (ms) to Green",
                result->green_direct_iam_ip_with_green_node_connect_times, bg_trigger_time);
        }

        // Wrapper execution times to Blue
        for (const auto& [host_id, result] : sorted_entries) {
            if (result->blue_wrapper_pre_switchover_execute_times.empty()) {
                continue;
            }
            PrintDurationTimes(host_id, "Wrapper execution time (ms) to Blue",
                result->blue_wrapper_pre_switchover_execute_times, bg_trigger_time);
        }

        // Wrapper execution times to Green
        for (const auto& [host_id, result] : sorted_entries) {
            if (result->green_wrapper_execute_times.empty()) {
                continue;
            }
            PrintDurationTimes(host_id, "Wrapper execution time (ms) to Green",
                result->green_wrapper_execute_times, bg_trigger_time);
        }

        // Host verification results
        for (const auto& [host_id, result] : sorted_entries) {
            if (result->host_verification_results.empty()) {
                continue;
            }
            PrintHostVerificationResults(host_id, result->host_verification_results, bg_trigger_time);
        }
    }

    std::chrono::steady_clock::time_point GetBgTriggerTime() {
        for (const auto& [_, result] : results) {
            if (result.bg_trigger_time != empty_time_point) {
                return result.bg_trigger_time;
            }
        }

        return empty_time_point;
    }

    long long GetSwitchoverCompleteTime() {
        long long max_green_node_change_time = GetMaxGreenNodeChangeTime();
        long long switchover_complete_time_from_status_table = GetSwitchoverCompleteTimeFromStatusTable();
        return max_green_node_change_time > switchover_complete_time_from_status_table
            ? max_green_node_change_time
            : switchover_complete_time_from_status_table;
    }

    long long GetMaxGreenNodeChangeTime() {
        std::chrono::steady_clock::time_point bg_trigger_time = GetBgTriggerTime();
        long long largest_offset = 0;
        for (const auto& [_, result] : results) {
            std::chrono::steady_clock::time_point green_node_changed_name_time{};
            if (result.green_node_changed_name_time_first_success != empty_time_point) {
                green_node_changed_name_time = result.green_node_changed_name_time_first_success;
            } else if (result.green_node_changed_name_time_first_error != empty_time_point) {
                green_node_changed_name_time = result.green_node_changed_name_time_first_error;
            } else {
                continue;
            }

            long long offset = GetTimeOffsetMs(green_node_changed_name_time, bg_trigger_time);
            largest_offset = offset > largest_offset
                ? offset
                : largest_offset;
        }
        return largest_offset;
    }

    long long GetSwitchoverCompleteTimeFromStatusTable() {
        std::chrono::steady_clock::time_point bg_trigger_time = GetBgTriggerTime();
        long long largest_offset = 0;
        for (const auto& [_, result] : results) {
            const auto& itr = result.green_status_time.find("SWITCHOVER_COMPLETED");
            if (itr != result.green_status_time.end()) {
                long long result_offset = GetTimeOffsetMs(itr->second, bg_trigger_time);
                largest_offset = result_offset > largest_offset
                    ? result_offset
                    : largest_offset;
            }
        }
        return largest_offset;
    }

    std::chrono::steady_clock::time_point GetSwitchoverInitiatedTime()
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

    std::chrono::steady_clock::time_point GetSwitchoverInProgressTime()
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

    long long CountSuccessfulConnectsAfterSwitchover(
        std::chrono::steady_clock::time_point bg_trigger_time,
        long long switchover_complete_time)
    {
        long long count = 0;
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

    long long CountUnsuccessfulConnectsAfterSwitchover(
        std::chrono::steady_clock::time_point bg_trigger_time,
        long long switchover_complete_time)
    {
        long long count = 0;
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

    long long CountSuccessfulExecutesAfterSwitchover() {
        long long count = 0;
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_post_switchover_execute_times) {
                count += !x.error.has_value()
                    ? 1
                    : 0;
            }
        }
        return count;
    }

    long long CountUnsuccessfulExecutesAfterSwitchover() {
        long long count = 0;
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
        long long switchover_complete_time)
    {
        for (const auto& [_, v] : results) {
            for (const auto& x : v.blue_wrapper_connect_times) {
                long long offset = GetTimeOffsetMs(x.start_time, bg_trigger_time);
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

    long long GetTimeOffsetMs(
        std::chrono::steady_clock::time_point time_stamp, std::chrono::steady_clock::time_point bg_trigger_time)
    {
        return time_stamp == empty_time_point
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
        long long switchover_completed_time_from_status_table = GetSwitchoverCompleteTimeFromStatusTable();
        EXPECT_NE(0, switchover_completed_time_from_status_table);

        for (const auto& [host_id, result] : results) {
            // Is Green Instance
            if (host_id.find("-green-") != std::string::npos) {
                std::chrono::steady_clock::time_point green_node_changed_name_time{};
                if (result.green_node_changed_name_time_first_success != empty_time_point) {
                    green_node_changed_name_time = result.green_node_changed_name_time_first_success;
                } else if (result.green_node_changed_name_time_first_error != empty_time_point) {
                    green_node_changed_name_time = result.green_node_changed_name_time_first_error;
                } else {
                    ThreadSynchronization::Print("Unable to find green node name change time: " + host_id);
                }
                EXPECT_NE(empty_time_point, green_node_changed_name_time);
            }
        }
    }

    void AssertWrapperBehavior(std::chrono::steady_clock::time_point bg_trigger_time) {
        long long switchover_complete_time = GetSwitchoverCompleteTime();

        // Log Timing
        std::cout << ("BG Trigger Time: " + std::to_string(bg_trigger_time.time_since_epoch().count())) << std::endl;
        std::cout << ("\tOffset from global start: " + std::to_string(GetTimeOffsetMs(bg_trigger_time, global_start_time)) + "ms") << std::endl;
        std::cout << ("Switchover Complete Time (ms): " + std::to_string(switchover_complete_time)) << std::endl;

        // Gather Metrics
        long long successful_connections = CountSuccessfulConnectsAfterSwitchover(
            bg_trigger_time, switchover_complete_time);
        long long successful_executions = CountSuccessfulExecutesAfterSwitchover();

        long long unsuccessful_connections = CountUnsuccessfulConnectsAfterSwitchover(
            bg_trigger_time, switchover_complete_time);
        long long unsuccessful_executions = CountUnsuccessfulExecutesAfterSwitchover();

        // Log Metrics
        std::cout << ("Successful Wrapper Connection after Switchover: " + std::to_string(successful_connections)) << std::endl;
        std::cout << ("Successful Wrapper Execution after Switchover: " + std::to_string(successful_executions)) << std::endl;
        std::cout << ("Unsuccessful Wrapper Connection after Switchover: " + std::to_string(unsuccessful_connections)) << std::endl;
        std::cout << ("Unsuccessful Wrapper Execution after Switchover: " + std::to_string(unsuccessful_executions)) << std::endl;

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
        std::chrono::steady_clock::time_point switchover_initiated_time = GetSwitchoverInitiatedTime();
        long long initiated_time_offset = GetTimeOffsetMs(switchover_initiated_time, bg_trigger_time);
        // Latest timepoint where switchover still in progress
        std::chrono::steady_clock::time_point switchover_in_process_time = GetSwitchoverInProgressTime();
        long long in_progress_time_offset = GetTimeOffsetMs(switchover_in_process_time, bg_trigger_time);

        std::cout << ("Earliest Status for Switchover Initiated: " + std::to_string(switchover_initiated_time.time_since_epoch().count()) + "ms") << std::endl;
        std::cout << ("\tOffset from bg start: " + std::to_string(initiated_time_offset) + "ms") << std::endl;
        std::cout << ("Latest Status for Switchover Inprogress: " + std::to_string(switchover_in_process_time.time_since_epoch().count()) + "ms") << std::endl;
        std::cout << ("\tOffset from bg start: " + std::to_string(in_progress_time_offset) + "ms") << std::endl;

        EXPECT_NE(empty_time_point, switchover_initiated_time);
        EXPECT_NE(empty_time_point, switchover_in_process_time);

        // Verify connections go to blue (none to green) before switchover initiated
        long long connections_before_switchover = 0;
        long long connections_to_blue_before_switchover = 0;
        long long connections_to_green_before_switchover = 0;

        for (const auto& [_, result] : results) {
            for (const auto& host_verification_result : result.host_verification_results) {
                long long offset = GetTimeOffsetMs(host_verification_result.timestamp, bg_trigger_time);
                if (offset > initiated_time_offset) {
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
        long long total_verifications_after_switchover_initiated = 0;
        long long connections_to_blue_after_switchover_initiated = 0;
        long long connections_to_green_after_switchover_initiated = 0;

        for (const auto& [_, result] : results) {
            for (const auto& host_verification_result : result.host_verification_results) {
                long long offset = GetTimeOffsetMs(host_verification_result.timestamp, bg_trigger_time);
                if (offset < in_progress_time_offset) {
                    continue;
                }

                if (host_verification_result.error.empty()) {
                    total_verifications_after_switchover_initiated += 1;
                    if (host_verification_result.connected_to_blue) {
                        connections_to_blue_after_switchover_initiated += 1;
                        std::cout << ("Connected to old blue cluster at offset: " + std::to_string(offset) +
                             "ms after switchover in progress at: " + std::to_string(in_progress_time_offset) +
                             "ms connected to: " + host_verification_result.connected_host +
                             ", original blue: " + host_verification_result.original_blue_ip)
                              << std::endl;
                    } else {
                        connections_to_green_after_switchover_initiated += 1;
                    }
                }
            }
        }

        std::cout << ("After switchover in progress: " + std::to_string(switchover_in_process_time.time_since_epoch().count())) << std::endl;
        std::cout << ("\tOffset from global: " + std::to_string(GetTimeOffsetMs(switchover_in_process_time, global_start_time))) << std::endl;
        std::cout << ("Total Connections: " + std::to_string(total_verifications_after_switchover_initiated)) << std::endl;
        std::cout << ("\tto old host: " + std::to_string(connections_to_blue_after_switchover_initiated)) << std::endl;
        std::cout << ("\tto new host: " + std::to_string(connections_to_green_after_switchover_initiated)) << std::endl;

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
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            ThreadSynchronization::Print("[DirectBlueIdleConnectivityMonitoringThread: " + host_id + "] started monitoring idle connection.");
            while (!ThreadSynchronization::stop_flag) {
                if (!ODBC_HELPER::TestSimpleQuery(hdbc)) {
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
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
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
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
            OpenConnectionWithRetry(hdbc, conn_str);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Actual Work
            ThreadSynchronization::Print("[WrapperBlueIdleConnectivityMonitoringThread: " + host_id + "] started monitoring idle connection.");
            while (!ThreadSynchronization::stop_flag) {
                if (!ODBC_HELPER::TestSimpleQuery(hdbc)) {
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
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
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
                    ThreadSynchronization::Print("[WrapperBlueExecutingConnectivityMonitoringThread: " + host_id + "] Phase 1 failed at offset "
                        + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - global_start_time).count()) + "ms: " + err);
                    pre_switch_results.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                    if (!ODBC_HELPER::TestSimpleQuery(hdbc)) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Phase 2 - Post-Switchover, reconnect and continue executing
            // Matches Java behavior: each iteration tries a single connect attempt
            // with a fresh DBC handle. If connect fails, record the error and try again next iteration
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, hstmt);
            hstmt = SQL_NULL_HSTMT;
            hdbc = SQL_NULL_HDBC;

            while (!ThreadSynchronization::stop_flag) {
                // Reconnect if needed with fresh DBC
                if (hdbc == SQL_NULL_HDBC || !ODBC_HELPER::TestSimpleQuery(hdbc)) {
                    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);
                    hstmt = SQL_NULL_HSTMT;
                    if (hdbc != SQL_NULL_HDBC) {
                        ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
                        hdbc = SQL_NULL_HDBC;
                    }
                    {
                        std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                        SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
                    }
                    SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
                    if (!SQL_SUCCEEDED(ODBC_HELPER::DriverConnect(hdbc, conn_str))) {
                        std::string err = ODBC_HELPER::PrintHandleError(hdbc, SQL_HANDLE_DBC);
                        ThreadSynchronization::Print("[WrapperBlueExecutingConnectivityMonitoringThread: " + host_id + "] Phase 2 connect failed at offset "
                            + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - global_start_time).count()) + "ms: " + err);
                        ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
                        hdbc = SQL_NULL_HDBC;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }
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
                    ThreadSynchronization::Print("[WrapperBlueExecutingConnectivityMonitoringThread: " + host_id + "] Phase 2 execute failed at offset "
                        + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - global_start_time).count()) + "ms: " + err);
                    post_switch_results.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                    SQLCloseCursor(hstmt);
                    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt);
                    hstmt = SQL_NULL_HSTMT;
                    ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
                    hdbc = SQL_NULL_HDBC;
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

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            while (!ThreadSynchronization::stop_flag) {
                SQLHDBC hdbc = SQL_NULL_HDBC;
                {
                    std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                    SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
                }
                SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);

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
                    ThreadSynchronization::Print("[WrapperBlueNewConnectionMonitoringThread: " + host_id + "] Failed to connect at offset "
                        + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - global_start_time).count()) + "ms: " + err);
                    results.push_back(
                        TimeHolder(start_time, end_time, err)
                    );
                }

                ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

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
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
            OpenConnectionWithRetry(hdbc, conn_str);
            const std::string original_ip = GetConnectedServerIp(hdbc);
            ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
            hdbc = SQL_NULL_HDBC;

            ThreadSynchronization::Print("[WrapperBlueHostVerificationThread: " + host_id + "] Original Blue IP: " + original_ip);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            // Work
            while (!ThreadSynchronization::stop_flag) {
                {
                    std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                    SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
                }
                SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);

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
                    long long offset = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - global_start_time).count();
                    ThreadSynchronization::Print("[WrapperBlueHostVerificationThread: " + host_id + "] Failed to connect at offset " + std::to_string(offset) + "ms: " + err);
                    results.push_back(
                        HostVerificationResult::Failure(timestamp, original_ip, err)
                    );
                }

                ODBC_HELPER::CleanUpHandles(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT);
                hdbc = SQL_NULL_HDBC;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

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
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            // Set login timeout to 10 seconds preventing long OS-level TCP timeouts from blocking reconnection during switchover.
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
            OpenConnectionWithRetry(hdbc, conn_str);
            const int BUFFER_SIZE = 512;

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();
            std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now()
                + std::chrono::minutes(15);

            ThreadSynchronization::Print("[DirectTopologyMonitoringThread: " + host_id + "] Starting BG Status Monitoring.");

            while (!ThreadSynchronization::stop_flag && std::chrono::steady_clock::now() < end_time) {
                if (!ODBC_HELPER::TestSimpleQuery(hdbc)) {
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

                    while (SQL_SUCCEEDED(SQLFetch(hstmt))) {
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
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
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
                    if (!ODBC_HELPER::TestSimpleQuery(hdbc)) {
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
            const std::string green_node_connect_ip = TEST_UTILS::HostToIp(host);
            ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] resolved IP: " + green_node_connect_ip + " for host: " + host);
            const std::string iam_conn_str_prefix = ConnectionStringBuilder(conn_str_builder.getString())
                .withServer(green_node_connect_ip)
                .withUID(test_iam_user)
                .getString();
            ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] iam_host: " + iam_host + " conn_str_prefix: " + iam_conn_str_prefix);

            // Ready for work, wait for other threads
            ThreadSynchronization::ReadyAndWait();

            ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] started monitoring connection.");

            // Work
            SQLHDBC hdbc = SQL_NULL_HDBC;
            {
                std::lock_guard<std::mutex> lock(ThreadSynchronization::env_mutex);
                SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
            }
            SQLSetConnectAttr(hdbc, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
            while (!ThreadSynchronization::stop_flag) {
                const std::string raw_iam_token = rds_client.GenerateConnectAuthToken(
                    iam_host.c_str(), test_region.c_str(), test_port, test_iam_user.c_str());
                // Extra URL encode: replace '%' with '%25'
                std::string iam_token = raw_iam_token;
                size_t pos = 0;
                while ((pos = iam_token.find('%', pos)) != std::string::npos) {
                    iam_token.replace(pos, 1, "%25");
                    pos += 3;
                }
                const std::string conn_str = ConnectionStringBuilder(iam_conn_str_prefix)
                    .withPWD(iam_token)
                    .getString();

                ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] attempting connection with iam_host: " + iam_host);
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
                    ThreadSynchronization::Print("[GreenIamConnectivityMonitoringThread_" + thread_prefix + ": " + host_id + "] Failed to connect at offset "
                        + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - global_start_time).count()) + "ms: " + err);
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
            SQLDisconnect(hdbc);
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
                ThreadSynchronization::Print("Failed to send Blue Green Switchover request at offset "
                    + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - global_start_time).count()) + "ms: "
                    + outcome.GetError().GetMessage());
            } else {
                ThreadSynchronization::Print("Successfully sent Blue Green Switchover request at offset "
                    + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - global_start_time).count()) + "ms.");
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
    ASSERT_FALSE(topology_instances.empty());

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

            size_t cluster_pos = host.find(".cluster");
            std::string blue_iam_host = host;
            if (green_pos != std::string::npos && cluster_pos != std::string::npos) {
                blue_iam_host = host.substr(0, green_pos) + host.substr(cluster_pos);
            }

            threads.push_back(CreateGreenIamConnectivityMonitoringThread(
                host_id, host, result.green_node_changed_name_time_first_success, result.green_direct_iam_ip_with_blue_node_connect_times,
                blue_iam_host, "BlueHost", false, true));
            threads.push_back(CreateGreenIamConnectivityMonitoringThread(
                host_id, host, result.green_node_changed_name_time_first_error, result.green_direct_iam_ip_with_green_node_connect_times,
                host, "GreenHost", true, false));
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
    ThreadSynchronization::Print("Threads started at offset " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - global_start_time).count()) + "ms");

    ThreadSynchronization::WaitForFinish(std::chrono::minutes(6));
    ThreadSynchronization::Print("Threads finished at offset " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - global_start_time).count()) + "ms");

    // Stop threads
    ThreadSynchronization::Print("Stopping threads at offset " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - global_start_time).count()) + "ms");
    ThreadSynchronization::stop_flag = true;
    // Allow threads to finish immediate work
    std::this_thread::sleep_for(std::chrono::minutes(3));

    ThreadSynchronization::Print("Join threads at offset " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - global_start_time).count()) + "ms");
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << ("-------------------------------------------------------------------------------------") << std::endl;
    std::cout << ("Tests finished") << std::endl;
    std::cout << ("-------------------------------------------------------------------------------------") << std::endl;
    PrintMetrics();
    AssertTest();
}
