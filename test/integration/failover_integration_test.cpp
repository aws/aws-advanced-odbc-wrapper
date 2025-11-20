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

#include "base_failover_integration_test.cpp"

#include "../common/connection_string_builder.h"
#include "../common/string_helper.h"

#include "integration_test_utils.h"

class FailoverIntegrationTest : public BaseFailoverIntegrationTest {
protected:
    std::string access_key = std::getenv("AWS_ACCESS_KEY_ID");
    std::string secret_access_key = std::getenv("AWS_SECRET_ACCESS_KEY");
    std::string session_token = std::getenv("AWS_SESSION_TOKEN") ? std::getenv("AWS_SESSION_TOKEN") : "";
    std::string rds_endpoint = std::getenv("RDS_ENDPOINT") ? std::getenv("RDS_ENDPOINT") : "";
    std::string rds_region = std::getenv("TEST_REGION") ? std::getenv("TEST_REGION") : "";
    Aws::RDS::RDSClientConfiguration client_config;
    Aws::RDS::RDSClient rds_client;
    SQLHENV env = nullptr;
    SQLHDBC dbc = nullptr;

    // Test Queries
    SQLTCHAR DROP_TABLE_QUERY[STRING_HELPER::MAX_SQLCHAR] = { 0 }; // DROP TABLE IF EXISTS failover_transaction
    SQLTCHAR CREATE_TABLE_QUERY[STRING_HELPER::MAX_SQLCHAR] = { 0 }; // CREATE TABLE failover_transaction (id INT NOT NULL PRIMARY KEY, failover_transaction_field VARCHAR(255) NOT NULL)
    SQLTCHAR SERVER_ID_QUERY[STRING_HELPER::MAX_SQLCHAR] = { 0 }; // SELECT aurora_db_instance_identifier()

    static void SetUpTestSuite() { Aws::InitAPI(options); }
    static void TearDownTestSuite() { Aws::ShutdownAPI(options); }

    void SetUp() override {
        SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
        SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
        SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

        STRING_HELPER::AnsiToUnicode("DROP TABLE IF EXISTS failover_transaction", DROP_TABLE_QUERY);
        STRING_HELPER::AnsiToUnicode("CREATE TABLE failover_transaction (id INT NOT NULL PRIMARY KEY, failover_transaction_field VARCHAR(255) NOT NULL)", CREATE_TABLE_QUERY);
        STRING_HELPER::AnsiToUnicode("SELECT aurora_db_instance_identifier()", SERVER_ID_QUERY);

        Aws::Auth::AWSCredentials credentials =
            session_token.empty() ? Aws::Auth::AWSCredentials(Aws::String(access_key), Aws::String(secret_access_key))
                                  : Aws::Auth::AWSCredentials(Aws::String(access_key), Aws::String(secret_access_key), Aws::String(session_token));
        client_config.region = rds_region.empty() ? "us-west-1" : rds_region;
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

        default_connection_string = ConnectionStringBuilder(test_dsn, writer_endpoint, test_port)
            .withUID(test_uid)
            .withPWD(test_pwd)
            .withDatabase(test_db)
            .withEnableClusterFailover(true)
            .getString();
        // Simple check to see if cluster is available.
        WaitForDbReady(rds_client, cluster_id);
    }

    void TearDown() override {
        if (nullptr != dbc) {
            SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        }
        if (nullptr != env) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
        }
    }
};

/** Writer fails to a reader **/
TEST_F(FailoverIntegrationTest, WriterFailToReader) {
    auto conn_str = ConnectionStringBuilder(default_connection_string)
        .withFailoverMode("STRICT_READER")
        .getString();
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(conn_str.c_str(), conn_str_in);
    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_str_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    // Query new ID after failover
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
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(default_connection_string.c_str(), conn_str_in);
    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_str_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    // Setup tests
    SQLHSTMT handle;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

    // Execute setup query
    EXPECT_TRUE(SQL_SUCCEEDED(SQLExecDirect(handle, DROP_TABLE_QUERY, SQL_NTS)));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, CREATE_TABLE_QUERY, SQL_NTS));

    // Execute queries within the transaction
    SQLTCHAR insert_query[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode("BEGIN; INSERT INTO failover_transaction VALUES (1, 'test field string 1')", insert_query);
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, insert_query, SQL_NTS));

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
    EXPECT_TRUE(SQL_SUCCEEDED(SQLExecDirect(handle, DROP_TABLE_QUERY, SQL_NTS)));

    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

/** Writer fails within a transaction. Open transaction with SQLSetConnectAttr */
TEST_F(FailoverIntegrationTest, WriterFailWithinTransaction_setAutoCommitFalse) {
    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(default_connection_string.c_str(), conn_str_in);
    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_str_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT));

    // Setup tests
    SQLHSTMT handle;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

    // Execute setup query
    EXPECT_TRUE(SQL_SUCCEEDED(SQLExecDirect(handle, DROP_TABLE_QUERY, SQL_NTS)));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, CREATE_TABLE_QUERY, SQL_NTS));

    // Set autocommit = false
    EXPECT_EQ(SQL_SUCCESS, SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, 0));

    // Run insert query in a new transaction
    SQLTCHAR insert_query[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode("INSERT INTO failover_transaction VALUES (1, 'test field string 1')", insert_query);
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, insert_query, SQL_NTS));

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
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, DROP_TABLE_QUERY, SQL_NTS));

    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
