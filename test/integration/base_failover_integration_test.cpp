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

#include "../common/string_helper.h"

#include "integration_test_utils.h"

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

static Aws::SDKOptions options;

constexpr auto MAX_CONN_LENGTH = 4096;
constexpr auto MAX_SQLSTATE_LENGTH = 6;

class BaseFailoverIntegrationTest : public testing::Test {
protected:
    // Connection string parameters
    std::string test_dsn = std::getenv("TEST_DSN");
    std::string test_db = std::getenv("TEST_DATABASE");
    std::string test_uid = std::getenv("TEST_USERNAME");
    std::string test_pwd = std::getenv("TEST_PASSWORD");
    int test_port = INTEGRATION_TEST_UTILS::str_to_int(
        INTEGRATION_TEST_UTILS::get_env_var("TEST_PORT", (char*) "5432"));
    std::string test_region = INTEGRATION_TEST_UTILS::get_env_var("TEST_REGION", (char*) "us-west-1");
    std::string test_server = std::getenv("TEST_SERVER");

    std::string cluster_prefix = "cluster-";
    std::string cluster_id = test_server.substr(0, test_server.find('.'));
    std::string instance_endpoint =
        test_server.substr(test_server.find(cluster_prefix) + cluster_prefix.size(), test_server.size());
    std::string db_conn_str_suffix = "." + instance_endpoint;
    std::string cluster_ro_url = "cluster-ro-" + instance_endpoint;

    std::string default_connection_string;
    std::string connection_string;

    SQLTCHAR* conn_in;
    SQLTCHAR* conn_out, sqlstate, message;
    SQLINTEGER native_error = 0;
    SQLSMALLINT len = 0, length = 0;

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

    // Helper functions
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

    void AssertQuerySuccess(SQLHDBC dbc, SQLTCHAR* query) const {
        SQLHSTMT handle;
        EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
        EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
        EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    }

    void AssertQueryFail(const SQLHDBC dbc, SQLTCHAR* query, const std::string& expected_error) const {
        SQLHSTMT handle;
        SQLSMALLINT stmt_length;
        SQLINTEGER native_err;
        SQLTCHAR msg[MAX_CONN_LENGTH], state[MAX_SQLSTATE_LENGTH];

        EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
        EXPECT_EQ(SQL_ERROR, SQLExecDirect(handle, query, SQL_NTS));
        EXPECT_EQ(SQL_SUCCESS, SQLError(nullptr, nullptr, handle, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));
        EXPECT_STREQ(expected_error.c_str(), STRING_HELPER::SqltcharToAnsi(state));
        EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    }

    std::string QueryInstanceId(SQLHDBC dbc) const {
        SQLTCHAR buf[SQL_MAX_MESSAGE_LENGTH] = { 0 };
        SQLLEN buflen;
        SQLHSTMT handle;
        EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
        SQLTCHAR server_id_query[STRING_HELPER::MAX_SQLCHAR] = { 0 };
        STRING_HELPER::AnsiToUnicode("SELECT aurora_db_instance_identifier()", server_id_query);
        EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, server_id_query, SQL_NTS));
        EXPECT_EQ(SQL_SUCCESS, SQLFetch(handle));
        EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_C_TCHAR, buf, sizeof(buf), &buflen));
        EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
        std::string id(STRING_HELPER::SqltcharToAnsi(buf));
        return id;
    }

    // Helper functions from integration tests
    static std::vector<std::string> GetTopologyViaSdk(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
        std::vector<std::string> instances;

        std::string writer;
        std::vector<std::string> readers;

        Aws::RDS::Model::DescribeDBClustersRequest rds_req;
        rds_req.WithDBClusterIdentifier(cluster_id);
        auto outcome = client.DescribeDBClusters(rds_req);

        if (!outcome.IsSuccess()) {
            std::cerr << "Error with Aurora::GDescribeDBClusters. " << outcome.GetError().GetMessage() << std::endl;
            throw std::runtime_error("Unable to get cluster topology using SDK.");
        }

        const auto result = outcome.GetResult();
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
        auto outcome = client.DescribeDBClusters(rds_req);
        const auto result = outcome.GetResult();
        return result.GetDBClusters().at(0);
    }

    static void WaitForDbReady(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
        Aws::String status = GetDbCluster(client, cluster_id).GetStatus();

        while (status != "available") {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            status = GetDbCluster(client, cluster_id).GetStatus();
        }
    }

    static Aws::RDS::Model::DBClusterEndpoint GetCustomEndpointInfo(const Aws::RDS::RDSClient& client, const std::string& endpoint_id) {
        Aws::RDS::Model::DescribeDBClusterEndpointsRequest request;
        request.SetDBClusterEndpointIdentifier(endpoint_id);
        const auto response = client.DescribeDBClusterEndpoints(request);
        return response.GetResult().GetDBClusterEndpoints()[0];
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
        if (!target_instance_id.empty()) {
            rds_req.WithTargetDBInstanceIdentifier(target_instance_id);
        }
        auto outcome = client.FailoverDBCluster(rds_req);
    }

    static Aws::String GetRandomDbReaderId(std::vector<std::string> readers) {
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> distribution(0, readers.size() - 1); // define the range of random numbers
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

    static void FailoverClusterWaitDesiredWriter(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                                                               const Aws::String& initial_writer_id, const Aws::String& target_writer_id = "") {

        auto cluster_endpoint = GetDbCluster(client, cluster_id).GetEndpoint();
        std::string initial_writer_ip = INTEGRATION_TEST_UTILS::host_to_IP(cluster_endpoint);

        FailoverCluster(client, cluster_id, target_writer_id);

        std::chrono::nanoseconds timeout = std::chrono::minutes(3);
        int remaining_attempts = 3;
        while (!HasWriterChanged(client, cluster_id, initial_writer_id, timeout)) {
            // if writer is not changed, try triggering failover again
            remaining_attempts--;
            if (remaining_attempts == 0) {
                throw std::runtime_error("Failover cluster request was not successful.");
            }
            FailoverCluster(client, cluster_id, target_writer_id);
        }

        // Failover has finished, wait for DNS to be updated so cluster endpoint resolves to the correct writer instance.
        std::string current_writer_ip = INTEGRATION_TEST_UTILS::host_to_IP(cluster_endpoint);
        auto start = std::chrono::high_resolution_clock::now();
        while (initial_writer_ip == current_writer_ip) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() > timeout.count()) {
                throw std::runtime_error("Cluster writer did not resolve to the target instance after server failover.");
            }

            current_writer_ip = INTEGRATION_TEST_UTILS::host_to_IP(cluster_endpoint);
        }
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

    static int QueryCountTableRows(const SQLHSTMT handle) {
        SQLTCHAR SELECT_COUNT_QUERY[STRING_HELPER::MAX_SQLCHAR] = { 0 };
        STRING_HELPER::AnsiToUnicode("SELECT count(*) FROM failover_transaction", SELECT_COUNT_QUERY);
        EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, SELECT_COUNT_QUERY, SQL_NTS));
        const auto rc = SQLFetch(handle);

        SQLINTEGER buf = -1;
        SQLLEN buflen;
        if (rc != SQL_NO_DATA_FOUND && rc != SQL_ERROR) {
            EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_INTEGER, &buf, sizeof(buf), &buflen));
            SQLFetch(handle); // To get cursor in correct position
        }
        return buf;
    }

    void TestConnection(const SQLHDBC dbc, const std::string& conn_str) {
        SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
        STRING_HELPER::AnsiToUnicode(conn_str.c_str(), conn_str_in);
        EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_str_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));
        EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
    }

    static void AssertNewReader(const std::vector<std::string>& old_readers, const std::string& new_reader) {
        for (const auto& reader : old_readers) {
            EXPECT_NE(reader, new_reader);
        }
    }
};
