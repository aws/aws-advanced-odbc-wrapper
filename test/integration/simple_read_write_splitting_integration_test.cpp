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

#include <unistd.h>

#include "base_failover_integration_test.h"

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

class SimpleReadWriteSplittingIntegrationTest : public BaseFailoverIntegrationTest {
protected:
    std::string access_key = TEST_UTILS::GetEnvVar("AWS_ACCESS_KEY_ID");
    std::string secret_access_key = TEST_UTILS::GetEnvVar("AWS_SECRET_ACCESS_KEY");
    std::string session_token = TEST_UTILS::GetEnvVar("AWS_SESSION_TOKEN");
    std::string rds_endpoint = TEST_UTILS::GetEnvVar("RDS_ENDPOINT");
    std::string rds_region = TEST_UTILS::GetEnvVar("TEST_REGION", "us-west-1");
    Aws::RDS::RDSClientConfiguration client_config;
    Aws::RDS::RDSClient rds_client;
    std::string stmt_set_read_only_true;
    std::string stmt_set_read_only_false;
    std::string reader_conn_str;

    static void SetUpTestSuite() {
        BaseFailoverIntegrationTest::SetUpTestSuite();
        Aws::InitAPI(options);
    }

    static void TearDownTestSuite() {
        Aws::ShutdownAPI(options);
        BaseFailoverIntegrationTest::TearDownTestSuite();
    }

    void SetUp() override {
        BaseFailoverIntegrationTest::SetUp();
        Aws::Auth::AWSCredentials credentials =
            session_token.empty() ? Aws::Auth::AWSCredentials(access_key, secret_access_key)
                                  : Aws::Auth::AWSCredentials(access_key, secret_access_key, session_token);
        client_config.region = rds_region;
        if (!rds_endpoint.empty()) {
            client_config.endpointOverride = rds_endpoint;
        }
        rds_client = Aws::RDS::RDSClient(credentials, client_config);

        if (test_dialect == "AURORA_MYSQL") {
            stmt_set_read_only_true = "set session transaction read only";
            stmt_set_read_only_false = "set session transaction read write";
        } else if (test_dialect == "AURORA_POSTGRESQL") {
            stmt_set_read_only_true = "set session characteristics as transaction read only";
            stmt_set_read_only_false = "set session characteristics as transaction read write";
        }

        cluster_instances = GetTopologyViaSdk(rds_client, cluster_id);
        writer_id = GetWriterId(cluster_instances);
        writer_endpoint = GetEndpoint(writer_id);
        readers = GetReaders(cluster_instances);
        reader_id = GetFirstReaderId(cluster_instances);
        target_writer_id = GetRandomDbReaderId(readers);

        conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withEnableSrwSplitting(true, test_server, cluster_ro_url)
            .withDatabaseDialect(test_dialect)
            .getString();

        reader_conn_str = ConnectionStringBuilder(test_dsn, cluster_ro_url, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withEnableSrwSplitting(true, test_server, cluster_ro_url)
            .withDatabaseDialect(test_dialect)
            .getString();

        // Check to see if cluster is available.
        WaitForDbReady(rds_client, cluster_id);
    }

    void TearDown() override {
        BaseFailoverIntegrationTest::TearDown();
    }
};

TEST_F(SimpleReadWriteSplittingIntegrationTest, WriterConnectSetReadOnly) {
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to true
    Query(dbc, stmt_set_read_only_true);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to false
    Query(dbc, stmt_set_read_only_false);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(SimpleReadWriteSplittingIntegrationTest, IncorrectURLs) {
    std::string incorrect_urls_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withEnableSrwSplitting(true, test_server, test_server)
            .withDatabaseDialect(test_dialect)
            .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, incorrect_urls_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to true
    Query(dbc, stmt_set_read_only_true);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to false
    Query(dbc, stmt_set_read_only_false);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(SimpleReadWriteSplittingIntegrationTest, WriterConnectSetReadOnlyVerifyConns) {
    conn_str += "SRW_VERIFY_CONNS=1;";
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to true
    Query(dbc, stmt_set_read_only_true);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to false
    Query(dbc, stmt_set_read_only_false);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
