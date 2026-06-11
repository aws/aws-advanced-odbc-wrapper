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

#include <iostream>

#include <aws/rds/model/TargetHealth.h>
#include <aws/rds/model/TargetState.h>

#include <aws/rds/RDSClient.h>

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/rds/model/DBCluster.h>
#include <aws/rds/model/DBClusterEndpoint.h>
#include <aws/rds/model/DBClusterMember.h>
#include <aws/rds/model/DescribeDBClusterEndpointsRequest.h>
#include <aws/rds/model/DescribeDBClustersRequest.h>
#include <aws/rds/model/DescribeDBInstancesRequest.h>
#include <aws/rds/model/FailoverDBClusterRequest.h>

#include <gtest/gtest.h>

#include <random>

#if defined(__APPLE__) || defined(__linux__)
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/types.h>
#endif

#include <ostream>

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

static Aws::SDKOptions options;

constexpr auto MAX_CONN_LENGTH = 4096;
constexpr auto MAX_SQLSTATE_LENGTH = 6;

class BaseFailoverIntegrationTest : public BaseConnectionTest {
protected:
    std::string cluster_prefix = ".cluster-";
    std::string cluster_id;
    std::string instance_endpoint;
    std::string db_conn_str_suffix;
    std::string cluster_ro_url;

    std::vector<std::string> cluster_instances;
    std::string writer_id;
    std::string writer_endpoint;
    std::vector<std::string> readers;
    std::string reader_id;
    std::string reader_endpoint;
    std::string target_writer_id;

    // Error codes
    std::string ERROR_FAILOVER_FAILED = "08S01";
    std::string ERROR_FAILOVER_SUCCEEDED = "08S02";
    std::string ERROR_TRANSACTION_UNKNOWN = "08007";

    // Test Queries
    std::string DROP_TABLE_QUERY = "DROP TABLE IF EXISTS failover_transaction";
    std::string CREATE_TABLE_QUERY = "CREATE TABLE failover_transaction (id INT NOT NULL PRIMARY KEY, failover_transaction_field VARCHAR(255) NOT NULL)";
    std::string COUNT_FAILOVER_TRANSACTION_ROWS_QUERY = "SELECT count(*) FROM failover_transaction";
    // Based off of dialect
    std::string SERVER_ID_QUERY = "";

    static void SetUpTestSuite() {
        BaseConnectionTest::SetUpTestSuite();
    }

    static void TearDownTestSuite() {
        BaseConnectionTest::TearDownTestSuite();
    }

    void SetUp() override {
        BaseConnectionTest::SetUp();

        cluster_prefix = ".cluster-";
        size_t cluster_id_index = test_server.find('.');
        if (std::string::npos == cluster_id_index) {
            GTEST_FAIL() << "Invalid test_server, cannot find Cluster ID for: " << test_server;
        }
        ASSERT_FALSE(std::string::npos == cluster_id_index);
        cluster_id = test_server.substr(0, cluster_id_index);
        size_t cluster_id_prefix_index = test_server.find(cluster_prefix);
        if (std::string::npos == cluster_id_prefix_index) {
            GTEST_FAIL() << "Invalid test_server, cannot find Cluster ID for: " << test_server;
        }
        ASSERT_FALSE(std::string::npos == cluster_id_prefix_index);
        instance_endpoint =
            test_server.substr(cluster_id_prefix_index + cluster_prefix.size(), test_server.size());
        db_conn_str_suffix = "." + instance_endpoint;
        cluster_ro_url = cluster_id + ".cluster-ro-" + instance_endpoint;

        if (test_dialect == "AURORA_POSTGRESQL") {
            SERVER_ID_QUERY = "SELECT pg_catalog.aurora_db_instance_identifier();";
        } else if (test_dialect == "AURORA_MYSQL") {
            SERVER_ID_QUERY = "SELECT @@aurora_server_id";
        } else if (test_dialect == "MULTI_AZ_POSTGRESQL") {
            SERVER_ID_QUERY = "SELECT SUBSTRING(endpoint FROM 0 FOR POSITION('.' IN endpoint)) "
                "FROM rds_tools.show_topology() WHERE id OPERATOR(pg_catalog.=) "
                "rds_tools.dbi_resource_id()";
        } else if (test_dialect == "MULTI_AZ_MYSQL") {
            SERVER_ID_QUERY = "SELECT SUBSTRING_INDEX(endpoint, '.', 1) "
                "FROM mysql.rds_topology "
                "WHERE id = @@server_id";
        } else {
            GTEST_SKIP() << "Failover requires database dialect to know which query to call.";
        }
    }

    void TearDown() override {
        BaseConnectionTest::TearDown();
    }

    // Class Helper functions
    std::string GetEndpoint(const std::string& instance_id) const {
        return instance_id + db_conn_str_suffix;
    }

    static std::string GetWriterId(std::vector<std::string> instances) {
        if (instances.empty()) {
            throw std::runtime_error("The input cluster topology is empty.");
        }
        return instances[0];
    }

    static std::vector<std::string> GetReaders(std::vector<std::string> instances) {
        if (instances.size() < 2) {
            throw std::runtime_error("The input cluster topology does not contain a reader.");
        }
        const std::vector<std::string>::const_iterator first_reader = instances.begin() + 1;
        const std::vector<std::string>::const_iterator last_reader = instances.end();
        std::vector<std::string> readers_list(first_reader, last_reader);
        return readers_list;
    }

    static std::string GetFirstReaderId(std::vector<std::string> instances) {
        if (instances.size() < 2) {
            throw std::runtime_error("The input cluster topology does not contain a reader.");
        }
        return instances[1];
    }

    // ODBC Query Helpers
    void AssertQueryFail(const SQLHDBC dbc, std::string query, const std::string& expected_error) const {
        SQLHSTMT hstmt = SQL_NULL_HSTMT;
        SQLSMALLINT stmt_length;
        SQLINTEGER native_err;
        SQLTCHAR msg[MAX_CONN_LENGTH] = {0}, state[MAX_SQLSTATE_LENGTH] = {0};

        SQLRETURN alloc_rc = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
        if (alloc_rc != SQL_SUCCESS) {
            ADD_FAILURE() << "AssertQueryFail: SQLAllocHandle failed with rc=" << alloc_rc;
            return;
        }
        EXPECT_EQ(SQL_ERROR, ODBC_HELPER::ExecuteQuery(hstmt, query));
        EXPECT_EQ(SQL_SUCCESS, SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));
        EXPECT_STREQ(expected_error.c_str(), STRING_HELPER::SqltcharToAnsi(state));
        EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, hstmt));
    }

    std::string QueryInstanceId(SQLHDBC dbc) const {
        SQLTCHAR buf[SQL_MAX_MESSAGE_LENGTH] = {0};
        SQLLEN buflen;
        SQLHSTMT hstmt = SQL_NULL_HANDLE;
        std::string result;

        if (SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt) != SQL_SUCCESS) {
            ADD_FAILURE() << "QueryInstanceId: SQLAllocHandle failed";
            return result;
        }

        if (ODBC_HELPER::ExecuteQuery(hstmt, SERVER_ID_QUERY) == SQL_SUCCESS &&
            SQLFetch(hstmt) == SQL_SUCCESS &&
            SQLGetData(hstmt, 1, SQL_C_TCHAR, buf, sizeof(buf), &buflen) == SQL_SUCCESS)
        {
            result = STRING_HELPER::SqltcharToAnsi(buf);
        } else {
            ADD_FAILURE() << "QueryInstanceId: query failed";
        }

        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return result;
    }

    int QueryCountTableRows(const SQLHSTMT handle) {
        EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, COUNT_FAILOVER_TRANSACTION_ROWS_QUERY));
        const SQLRETURN rc = SQLFetch(handle);
        EXPECT_EQ(SQL_SUCCESS, rc);

        SQLINTEGER buf = -1;
        SQLLEN buflen;
        if (rc != SQL_NO_DATA_FOUND && rc != SQL_ERROR) {
            EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_INTEGER, &buf, sizeof(buf), &buflen));
            EXPECT_EQ(SQL_SUCCESS, rc);
            SQLCloseCursor(handle);
        }
        return buf;
    }

    // AWS SDK Helpers
    static std::vector<std::string> GetTopologyViaSdk(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
        std::vector<std::string> instances;

        std::string writer;
        std::vector<std::string> readers;

        Aws::RDS::Model::DescribeDBClustersRequest rds_req;
        rds_req.WithDBClusterIdentifier(cluster_id);
        auto outcome = client.DescribeDBClusters(rds_req);

        if (!outcome.IsSuccess()) {
            std::cerr << "Error with Aurora::DescribeDBClusters. " << outcome.GetError().GetMessage() << std::endl;
            throw std::runtime_error("Unable to get cluster topology using SDK.");
        }

        const auto result = outcome.GetResult();
        if (result.GetDBClusters().empty()) {
            throw std::runtime_error("GetTopologyViaSdk: No clusters returned");
        }
        const Aws::RDS::Model::DBCluster cluster = result.GetDBClusters()[0];

        for (const auto& instance : cluster.GetDBClusterMembers()) {
            std::string instance_id(instance.GetDBInstanceIdentifier());
            if (instance.GetIsClusterWriter()) {
                writer = instance_id;
            } else {
                readers.push_back(instance_id);
            }
        }

        instances.push_back(writer);
        for (const auto& reader : readers) {
            instances.push_back(reader);
        }
        return instances;
    }

    static Aws::RDS::Model::DBCluster GetDbCluster(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
        Aws::RDS::Model::DescribeDBClustersRequest rds_req;
        rds_req.WithDBClusterIdentifier(cluster_id);

        constexpr int max_retries = 2;
        for (int attempt = 0; attempt <= max_retries; attempt++) {
            auto outcome = client.DescribeDBClusters(rds_req);
            if (outcome.IsSuccess()) {
                const auto result = outcome.GetResult();
                if (result.GetDBClusters().empty()) {
                    throw std::runtime_error("GetDbCluster: No clusters found");
                }
                return result.GetDBClusters().at(0);
            }
            if (attempt < max_retries) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }
        throw std::runtime_error("GetDbCluster: DescribeDBClusters failed after retries");
    }

    static void WaitForDbReady(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
        Aws::String status = GetDbCluster(client, cluster_id).GetStatus();

        while (status != "available") {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            status = GetDbCluster(client, cluster_id).GetStatus();
        }
    }

    static void WaitForAllInstancesReady(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                                         std::chrono::seconds timeout = std::chrono::minutes(10)) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            auto cluster = GetDbCluster(client, cluster_id);
            auto members = cluster.GetDBClusterMembers();
            bool all_available = true;

            for (const auto& member : members) {
                Aws::RDS::Model::DescribeDBInstancesRequest req;
                req.SetDBInstanceIdentifier(member.GetDBInstanceIdentifier());
                auto outcome = client.DescribeDBInstances(req);
                if (!outcome.IsSuccess() || outcome.GetResult().GetDBInstances().empty()) {
                    all_available = false;
                    break;
                }
                if (outcome.GetResult().GetDBInstances()[0].GetDBInstanceStatus() != "available") {
                    all_available = false;
                    break;
                }
            }

            if (all_available) {
                // SDK reports all instances available. Verify TCP connectivity to the
                // cluster writer endpoint. Multi-AZ instances may not accept connections
                // immediately after reporting "available" status.
                const std::string endpoint = cluster.GetEndpoint();
                const int port = cluster.GetPort();
                if (!TEST_UTILS::CanTcpConnect(endpoint, port, 10)) {
                    std::cout << "[WaitForAllInstancesReady] Instances available but writer endpoint "
                              << endpoint << ":" << port << " not yet accepting connections." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    continue;
                }

                // Also verify TCP connectivity to each individual instance endpoint.
                // Tests connect to instance endpoints directly, and these may lag behind
                // the cluster endpoint after failover.
                bool all_instances_connectable = true;
                for (const auto& member : members) {
                    Aws::RDS::Model::DescribeDBInstancesRequest inst_req;
                    inst_req.SetDBInstanceIdentifier(member.GetDBInstanceIdentifier());
                    auto inst_outcome = client.DescribeDBInstances(inst_req);
                    if (inst_outcome.IsSuccess() && !inst_outcome.GetResult().GetDBInstances().empty()) {
                        const std::string instance_endpoint(inst_outcome.GetResult().GetDBInstances()[0].GetEndpoint().GetAddress());
                        const int instance_port = inst_outcome.GetResult().GetDBInstances()[0].GetEndpoint().GetPort();
                        if (!instance_endpoint.empty() && !TEST_UTILS::CanTcpConnect(instance_endpoint, instance_port, 10)) {
                            std::cout << "[WaitForAllInstancesReady] Instance endpoint "
                                      << instance_endpoint << ":" << instance_port
                                      << " not yet accepting connections." << std::endl;
                            all_instances_connectable = false;
                            break;
                        }
                    }
                }
                if (all_instances_connectable) {
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        throw std::runtime_error("WaitForAllInstancesReady: Instances not available.");
    }

    static Aws::RDS::Model::DBClusterEndpoint GetCustomEndpointInfo(const Aws::RDS::RDSClient& client, const std::string& endpoint_id) {
        Aws::RDS::Model::DescribeDBClusterEndpointsRequest request;
        request.SetDBClusterEndpointIdentifier(endpoint_id);
        const auto response = client.DescribeDBClusterEndpoints(request);
        if (!response.IsSuccess()) {
            throw std::runtime_error("GetCustomEndpointInfo: DescribeDBClusterEndpoints failed");
        }
        const auto& endpoints = response.GetResult().GetDBClusterEndpoints();
        if (endpoints.empty()) {
            throw std::runtime_error("Custom endpoint not found: " + endpoint_id);
        }
        return endpoints[0];
    }

    static Aws::RDS::Model::DBClusterMember GetDbClusterWriter(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
        Aws::RDS::Model::DBClusterMember instance;
        const Aws::RDS::Model::DBCluster cluster = GetDbCluster(client, cluster_id);
        for (const auto& member : cluster.GetDBClusterMembers()) {
            if (member.GetIsClusterWriter()) {
                return member;
            }
        }
        return instance;
    }

    static Aws::String GetDbClusterWriterId(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
        return GetDbClusterWriter(client, cluster_id).GetDBInstanceIdentifier();
    }

    static void WaitForNewDbWriter(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                                                   const Aws::String& initial_writer_instance_id) {
        Aws::String next_cluster_writer_id = GetDbClusterWriterId(client, cluster_id);
        while (initial_writer_instance_id == next_cluster_writer_id) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            next_cluster_writer_id = GetDbClusterWriterId(client, cluster_id);
        }
    }

    static void FailoverCluster(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, const Aws::String& target_instance_id = "") {
        WaitForDbReady(client, cluster_id);
        Aws::RDS::Model::FailoverDBClusterRequest rds_req;
        rds_req.WithDBClusterIdentifier(cluster_id);
        if (!target_instance_id.empty() && test_dialect != "MULTI_AZ_MYSQL" && test_dialect != "MULTI_AZ_POSTGRESQL") {
            rds_req.WithTargetDBInstanceIdentifier(target_instance_id);
        }
        auto outcome = client.FailoverDBCluster(rds_req);
    }

    static Aws::String GetRandomDbReaderId(std::vector<std::string> readers) {
        if (readers.empty()) {
            throw std::runtime_error("GetRandomDbReaderId: readers list is empty");
        }
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> distribution(0, static_cast<int>(readers.size()) - 1);
        return readers.at(distribution(generator));
    }

    static bool HasWriterChanged(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, std::string initial_writer_id,
                                   std::chrono::nanoseconds timeout) {
        auto start = std::chrono::high_resolution_clock::now();

        std::string current_writer_id = GetDbClusterWriterId(client, cluster_id);
        while (initial_writer_id == current_writer_id) {
            if (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() > timeout.count()) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::seconds(3));
            // Calling the RDS API to get writer Id.
            current_writer_id = GetDbClusterWriterId(client, cluster_id);
        }
        return true;
    }

    static void LogClusterTopology(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, const std::string& label) {
        auto cluster = GetDbCluster(client, cluster_id);
        std::cout << "[" << label << "] SDK cluster topology (via DescribeDBClusters):" << std::endl;
        for (const auto& member : cluster.GetDBClusterMembers()) {
            std::cout << "  instance: " << member.GetDBInstanceIdentifier()
                      << ", role: " << (member.GetIsClusterWriter() ? "WRITER" : "READER") << std::endl;
        }
    }

    static void FailoverClusterWaitDesiredWriter(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                                                 const Aws::String& initial_writer_id, const Aws::String& target_writer_id = "") {
        std::cout << "[FailoverClusterWaitDesiredWriter] Initial writer: " << initial_writer_id
                  << ", desired writer: " << (target_writer_id.empty() ? "<any>" : target_writer_id) << std::endl;
        LogClusterTopology(client, cluster_id, "Before failover");

        auto cluster_endpoint = GetDbCluster(client, cluster_id).GetEndpoint();
        std::string initial_writer_ip = TEST_UTILS::HostToIp(cluster_endpoint);
        ASSERT_FALSE(initial_writer_ip.empty()) << "Failed to resolve IP address for host: " << cluster_endpoint;

        FailoverCluster(client, cluster_id, target_writer_id);

        std::chrono::nanoseconds timeout = std::chrono::minutes(3);
        int remaining_attempts = 5;
        while (!HasWriterChanged(client, cluster_id, initial_writer_id, timeout)) {
            // if writer is not changed, try triggering failover again
            remaining_attempts--;
            if (remaining_attempts == 0) {
                GTEST_SKIP() << "Failover cluster request was not successful.";
            }
            FailoverCluster(client, cluster_id, target_writer_id);
        }

        std::cout << "[FailoverClusterWaitDesiredWriter] Failover complete. Initial writer: " << initial_writer_id
                  << ", desired writer: " << (target_writer_id.empty() ? "<any>" : target_writer_id) << std::endl;
        LogClusterTopology(client, cluster_id, "After failover");

        // Failover has finished, wait for DNS to be updated so cluster endpoint resolves to the correct writer instance.
        std::string current_writer_ip = TEST_UTILS::HostToIp(cluster_endpoint);
        ASSERT_FALSE(current_writer_ip.empty()) << "Failed to resolve IP address for host: " << cluster_endpoint;
        auto start = std::chrono::high_resolution_clock::now();
        while (initial_writer_ip == current_writer_ip) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() > timeout.count()) {
                GTEST_SKIP() << "Cluster writer did not resolve to the target instance after server failover.";
            }

            current_writer_ip = TEST_UTILS::HostToIp(cluster_endpoint);
            ASSERT_FALSE(current_writer_ip.empty()) << "Failed to resolve IP address for host: " << cluster_endpoint;
        }

        std::cout << "[FailoverClusterWaitDesiredWriter] DNS updated. Cluster endpoint now resolves to new writer." << std::endl;
    }

    static Aws::RDS::Model::DBClusterMember GetMatchedDbClusterMember(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                                                                        const Aws::String& instance_id) {
        Aws::RDS::Model::DBClusterMember instance;
        const Aws::RDS::Model::DBCluster cluster = GetDbCluster(client, cluster_id);
        for (const auto& member : cluster.GetDBClusterMembers()) {
            Aws::String member_id = member.GetDBInstanceIdentifier();
            if (member_id == instance_id) {
                return member;
            }
        }
        return instance;
    }

    static bool IsInstanceWriter(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, const Aws::String& instance_id) {
        return GetMatchedDbClusterMember(client, cluster_id, instance_id).GetIsClusterWriter();
    }

    static bool IsInstanceReader(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, const Aws::String& instance_id) {
        return !GetMatchedDbClusterMember(client, cluster_id, instance_id).GetIsClusterWriter();
    }

    static void AssertNewReader(const std::vector<std::string>& old_readers, const std::string& new_reader) {
        for (const auto& reader : old_readers) {
            EXPECT_NE(reader, new_reader);
        }
    }
};
