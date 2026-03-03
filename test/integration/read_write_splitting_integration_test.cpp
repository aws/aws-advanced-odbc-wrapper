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

#include "base_failover_integration_test.h"

#include "../common/base_connection_test.h"
#include "../common/connection_string_builder.h"
#include "../common/odbc_helper.h"
#include "../common/string_helper.h"
#include "../common/test_utils.h"

class ReadWriteSplittingIntegrationTest : public BaseFailoverIntegrationTest {
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
            .withEnableRwSplitting(true)
            .withDatabaseDialect(test_dialect)
            .getString();

        reader_conn_str = ConnectionStringBuilder(test_dsn, cluster_ro_url, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withEnableRwSplitting(true)
            .withDatabaseDialect(test_dialect)
            .getString();

        // Check to see if cluster is available.
        WaitForDbReady(rds_client, cluster_id);
    }

    void TearDown() override {
        BaseFailoverIntegrationTest::TearDown();
    }
};

TEST_F(ReadWriteSplittingIntegrationTest, WriterConnectSetReadOnly) {
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

TEST_F(ReadWriteSplittingIntegrationTest, ReaderConnectSetReadOnly) {
    std::string url = ConnectionStringBuilder(test_dsn, GetEndpoint(reader_id), test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withEnableRwSplitting(true)
        .withDatabaseDialect(test_dialect)
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, url);
    EXPECT_EQ(SQL_SUCCESS, rc);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a writer
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

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

TEST_F(ReadWriteSplittingIntegrationTest, ReaderClusterConnectSetReadOnly) {
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, reader_conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a writer
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

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

TEST_F(ReadWriteSplittingIntegrationTest, ReadOnlyFalseInTx) {
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    // Set read only to true
    Query(dbc, stmt_set_read_only_true);

    // Execute queries within the transaction
    // Setup tests
    SQLHSTMT handle;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

    std::string begin_query = "BEGIN;";
    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, begin_query));
    std::string transaction_insert_query = "INSERT INTO failover_transaction VALUES (1, 'test field string 1')";
    EXPECT_THROW(ODBC_HELPER::ExecuteQuery(handle, stmt_set_read_only_false), std::runtime_error);

    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, "COMMIT;"));

    // Set read only to false
    Query(dbc, stmt_set_read_only_false);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(ReadWriteSplittingIntegrationTest, ReadOnlyTrueInTx) {
    if (test_dialect == "AURORA_MYSQL") {
        SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
        EXPECT_EQ(SQL_SUCCESS, rc);

        // Execute queries within the transaction
        // Setup tests
        SQLHSTMT handle;
        EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
        // Execute setup query
        EXPECT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(handle, DROP_TABLE_QUERY))); // May return SQL_SUCCESS_WITH_INFO if table does not exist
        EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, CREATE_TABLE_QUERY));
        std::string begin_query = "set autocommit = 0;";
        EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, begin_query));

        std::string transaction_insert_query = "INSERT INTO failover_transaction VALUES (1, 'test field string 1')";
        EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, transaction_insert_query));

        ODBC_HELPER::ExecuteQuery(handle, stmt_set_read_only_true);

        std::string current_connection_id = QueryInstanceId(dbc);

        // Check if current connection is a writer
        EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

        EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, "COMMIT;"));

        EXPECT_EQ(1, QueryCountTableRows(handle));
        SQLFreeHandle(SQL_HANDLE_STMT, handle);

        // Set read only to true
        Query(dbc, stmt_set_read_only_true);

        current_connection_id = QueryInstanceId(dbc);

        // Check if current connection is a writer
        EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

        // Clean up test
        EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
    }
}

TEST_F(ReadWriteSplittingIntegrationTest, ReadOnlyFalseAutocommitFalse) {
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    // Set read only to true
    Query(dbc, stmt_set_read_only_true);

    std::string current_connection_id = QueryInstanceId(dbc);
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Execute queries within the transaction
    // Setup tests
    EXPECT_EQ(SQL_SUCCESS, SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0));
    // EXPECT_EQ(SQL_SUCCESS, SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, 0));

    // Set read only to false
    SQLHSTMT hstmt;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt));
    EXPECT_THROW(ODBC_HELPER::ExecuteQuery(hstmt, stmt_set_read_only_false), std::runtime_error);
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, hstmt));

    current_connection_id = QueryInstanceId(dbc);
    // Check if current connection is a reader
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    Query(dbc, "COMMIT;");

    Query(dbc, stmt_set_read_only_false);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a reader
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(ReadWriteSplittingIntegrationTest, WriterFailoverReadOnly) {
    std::string strict_reader_conn_str = ConnectionStringBuilder(conn_str)
        .withEnableClusterFailover(true)
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, strict_reader_conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    FailoverClusterWaitDesiredWriter(rds_client, cluster_id, writer_id, target_writer_id);
    AssertQueryFail(dbc, SERVER_ID_QUERY, ERROR_FAILOVER_SUCCEEDED);

    // Query new ID after failover
    current_connection_id = QueryInstanceId(dbc);

    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to true
    Query(dbc, stmt_set_read_only_true);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is the original writer
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to false
    Query(dbc, stmt_set_read_only_false);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is the original writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(ReadWriteSplittingIntegrationTest, FailoverToNewReaderReadOnly) {
    std::string strict_reader_conn_str = ConnectionStringBuilder(conn_str)
        .withEnableClusterFailover(true)
        .withFailoverMode("STRICT_READER")
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, strict_reader_conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    FailoverClusterWaitDesiredWriter(rds_client, cluster_id, writer_id, target_writer_id);
    AssertQueryFail(dbc, SERVER_ID_QUERY, ERROR_FAILOVER_SUCCEEDED);

    // Query new ID after failover
    current_connection_id = QueryInstanceId(dbc);

    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to true
    Query(dbc, stmt_set_read_only_false);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is the original writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Set read only to true
    Query(dbc, stmt_set_read_only_true);

    current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is the original writer
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
