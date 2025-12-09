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

class FailoverIntegrationTest : public BaseFailoverIntegrationTest {
protected:
    std::string access_key = TEST_UTILS::GetEnvVar("AWS_ACCESS_KEY_ID");
    std::string secret_access_key = TEST_UTILS::GetEnvVar("AWS_SECRET_ACCESS_KEY");
    std::string session_token = TEST_UTILS::GetEnvVar("AWS_SESSION_TOKEN");
    std::string rds_endpoint = TEST_UTILS::GetEnvVar("RDS_ENDPOINT");
    std::string rds_region = TEST_UTILS::GetEnvVar("TEST_REGION", "us-west-1");
    Aws::RDS::RDSClientConfiguration client_config;
    Aws::RDS::RDSClient rds_client;

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

        cluster_instances = GetTopologyViaSdk(rds_client, cluster_id);
        writer_id = GetWriterId(cluster_instances);
        writer_endpoint = GetEndpoint(writer_id);
        readers = GetReaders(cluster_instances);
        reader_id = GetFirstReaderId(cluster_instances);
        target_writer_id = GetRandomDbReaderId(readers);

        conn_str = ConnectionStringBuilder(test_dsn, writer_endpoint, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withEnableClusterFailover(true)
            .withDatabaseDialect(test_dialect)
            .getString();

        // Check to see if cluster is available.
        WaitForDbReady(rds_client, cluster_id);
    }

    void TearDown() override {
        BaseFailoverIntegrationTest::TearDown();
    }
};

/** Writer fails to a reader **/
TEST_F(FailoverIntegrationTest, WriterFailToReader) {
    std::string strict_reader_conn_str = ConnectionStringBuilder(conn_str)
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

    // Check if current connection is a reader
    EXPECT_FALSE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));

    // Clean up test
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

/** Writer fails within a transaction. Open transaction by explicitly calling BEGIN */
TEST_F(FailoverIntegrationTest, WriterFailWithinTransaction_DisableAutocommit) {
    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::DriverConnect(dbc, conn_str));

    // Setup tests
    SQLHSTMT handle;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

    // Execute setup query
    EXPECT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(handle, DROP_TABLE_QUERY))); // May return SQL_SUCCESS_WITH_INFO if table does not exist
    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, CREATE_TABLE_QUERY));

    // Execute queries within the transaction
    std::string transaction_insert_query = "BEGIN; INSERT INTO failover_transaction VALUES (1, 'test field string 1')";
    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, transaction_insert_query));

    FailoverClusterWaitDesiredWriter(rds_client, cluster_id, writer_id, target_writer_id);

    // If there is an active transaction, roll it back and return an error 08007.
    AssertQueryFail(dbc, SERVER_ID_QUERY, ERROR_TRANSACTION_UNKNOWN);
    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a new writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));
    EXPECT_NE(current_connection_id, writer_id);

    // No rows should have been inserted to the table
    EXPECT_EQ(0, QueryCountTableRows(handle));
    SQLFreeHandle(SQL_HANDLE_STMT, handle);

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

    // Clean up test
    EXPECT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(handle, DROP_TABLE_QUERY))); // May return SQL_SUCCESS_WITH_INFO if table does not exist

    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

/** Writer fails within a transaction. Open transaction with SQLSetConnectAttr */
TEST_F(FailoverIntegrationTest, WriterFailWithinTransaction_setAutoCommitFalse) {
    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::DriverConnect(dbc, conn_str));

    // Setup tests
    SQLHSTMT handle;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

    // Execute setup query
    EXPECT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(handle, DROP_TABLE_QUERY))); // May return SQL_SUCCESS_WITH_INFO if table does not exist
    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, CREATE_TABLE_QUERY));

    // Set autocommit = false
    EXPECT_EQ(SQL_SUCCESS, SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0));

    // Run insert query in a new transaction
    std::string insert_query = "INSERT INTO failover_transaction VALUES (1, 'test field string 1')";
    EXPECT_EQ(SQL_SUCCESS, ODBC_HELPER::ExecuteQuery(handle, insert_query));

    FailoverClusterWaitDesiredWriter(rds_client, cluster_id, writer_id, target_writer_id);

    // If there is an active transaction, roll it back and return an error 08007.
    AssertQueryFail(dbc, SERVER_ID_QUERY, ERROR_TRANSACTION_UNKNOWN);

    // Query new ID after failover
    std::string current_connection_id = QueryInstanceId(dbc);

    // Check if current connection is a new writer
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, current_connection_id));
    EXPECT_NE(current_connection_id, writer_id);

    // No rows should have been inserted to the table
    EXPECT_EQ(0, QueryCountTableRows(handle));
    SQLFreeHandle(SQL_HANDLE_STMT, handle);

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

    // Set autocommit = true
    EXPECT_EQ(SQL_SUCCESS, SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, 0));

    // Clean up test
    EXPECT_TRUE(SQL_SUCCEEDED(ODBC_HELPER::ExecuteQuery(handle, DROP_TABLE_QUERY))); // May return SQL_SUCCESS_WITH_INFO if table does not exist

    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
