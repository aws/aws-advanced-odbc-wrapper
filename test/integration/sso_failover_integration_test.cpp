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

// Not built as part of standard integration test, requires manual CMake addition to compile
class SsoFailoverIntegrationTest : public BaseFailoverIntegrationTest {
protected:
    std::string access_key = TEST_UTILS::GetEnvVar("AWS_ACCESS_KEY_ID");
    std::string secret_access_key = TEST_UTILS::GetEnvVar("AWS_SECRET_ACCESS_KEY");
    std::string session_token = TEST_UTILS::GetEnvVar("AWS_SESSION_TOKEN");
    std::string rds_endpoint = TEST_UTILS::GetEnvVar("RDS_ENDPOINT");
    std::string rds_region = TEST_UTILS::GetEnvVar("TEST_REGION", "us-west-2");

    // SSO configuration
    std::string sso_start_url = TEST_UTILS::GetEnvVar("SSO_START_URL");
    std::string sso_account_id = TEST_UTILS::GetEnvVar("SSO_ACCOUNT_ID");
    std::string sso_role_name = TEST_UTILS::GetEnvVar("SSO_ROLE_NAME");
    std::string sso_region = TEST_UTILS::GetEnvVar("SSO_REGION", TEST_UTILS::GetEnvVar("TEST_REGION", "us-west-2").c_str());
    std::string sso_session_name = TEST_UTILS::GetEnvVar("SSO_SESSION_NAME");

    Aws::RDS::RDSClientConfiguration client_config;
    Aws::RDS::RDSClient rds_client;

    std::string SELECT_USER_QUERY = "";

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

        if (sso_start_url.empty() || sso_account_id.empty() || sso_role_name.empty()) {
            GTEST_SKIP() << "SSO_START_URL / SSO_ACCOUNT_ID / SSO_ROLE_NAME must be set.";
        }

        Aws::Auth::AWSCredentials credentials =
            session_token.empty() ? Aws::Auth::AWSCredentials(access_key, secret_access_key)
                                  : Aws::Auth::AWSCredentials(access_key, secret_access_key, session_token);
        client_config.region = rds_region;
        if (!rds_endpoint.empty()) {
            client_config.endpointOverride = rds_endpoint;
        }
        rds_client = Aws::RDS::RDSClient(credentials, client_config);

        WaitForDbReady(rds_client, cluster_id);
        WaitForAllInstancesReady(rds_client, cluster_id);

        cluster_instances = GetTopologyViaSdk(rds_client, cluster_id);
        if (cluster_instances.empty()) {
            GTEST_SKIP() << "No cluster instances found";
        }
        writer_id = GetWriterId(cluster_instances);
        writer_endpoint = GetEndpoint(writer_id);
        if (cluster_instances.size() < 2) {
            GTEST_SKIP() << "Failover tests require at least 2 instances (1 writer + 1 reader)";
        }
        readers = GetReaders(cluster_instances);
        reader_id = GetFirstReaderId(cluster_instances);
        target_writer_id = GetRandomDbReaderId(readers);

        SELECT_USER_QUERY = test_dialect == "AURORA_POSTGRESQL"
            ? "SELECT current_user;"
            : "SELECT CURRENT_USER();";

        bool extra_url_encode = test_dialect == "AURORA_POSTGRESQL";
        ConnectionStringBuilder builder(test_dsn, writer_endpoint, test_port);
        builder.withUID(test_uid)
            .withDatabase(test_db)
            .withAuthMode("AWS_SSO")
            .withSsoStartUrl(sso_start_url)
            .withSsoAccountId(sso_account_id)
            .withSsoRoleName(sso_role_name)
            .withSsoRegion(sso_region)
            .withExtraUrlEncode(extra_url_encode)
            .withEnableClusterFailover(true)
            .withDatabaseDialect(test_dialect)
            .withFailoverMode("STRICT_WRITER")
            .withFailoverTimeout(120000);
        if (!sso_session_name.empty()) {
            builder.withSsoSessionName(sso_session_name);
        }
        conn_str = builder.getString();
    }

    void TearDown() override {
        BaseFailoverIntegrationTest::TearDown();
    }

    // Returns the result of `SELECT current_user` over the existing connection.
    std::string QueryCurrentUser(SQLHDBC connection) const {
        SQLTCHAR buf[SQL_MAX_MESSAGE_LENGTH] = {0};
        SQLLEN buflen;
        SQLHSTMT hstmt = SQL_NULL_HANDLE;
        std::string result;

        if (SQLAllocHandle(SQL_HANDLE_STMT, connection, &hstmt) != SQL_SUCCESS) {
            ADD_FAILURE() << "QueryCurrentUser: SQLAllocHandle failed";
            return result;
        }
        if (ODBC_HELPER::ExecuteQuery(hstmt, SELECT_USER_QUERY) == SQL_SUCCESS &&
            SQLFetch(hstmt) == SQL_SUCCESS &&
            SQLGetData(hstmt, 1, SQL_C_TCHAR, buf, sizeof(buf), &buflen) == SQL_SUCCESS) {
            result = STRING_HELPER::SqltcharToAnsi(buf);
        } else {
            ADD_FAILURE() << "QueryCurrentUser: query failed";
        }
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return result;
    }
};

TEST_F(SsoFailoverIntegrationTest, WriterFailoverReusesCachedSsoToken) {
    conn_str = ConnectionStringBuilder(conn_str)
        .withClusterId("SsoWriterFailover")
        .getString();

    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    ASSERT_EQ(SQL_SUCCESS, rc) << "Initial SSO connect failed; ensure a valid SSO token is cached.";

    const std::string before_id = QueryInstanceId(dbc);
    const std::string before_user = QueryCurrentUser(dbc);
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, before_id))
        << "Expected initial connection to be the writer, got " << before_id;
    EXPECT_EQ(test_uid, before_user) << "Initial session should be the SSO DB user.";

    FailoverClusterWaitDesiredWriter(rds_client, cluster_id, writer_id, target_writer_id);
    AssertQueryFail(dbc, SERVER_ID_QUERY, ERROR_FAILOVER_SUCCEEDED);

    const std::string after_id = QueryInstanceId(dbc);
    const std::string after_user = QueryCurrentUser(dbc);

    EXPECT_NE(before_id, after_id) << "Writer should have changed after failover.";
    EXPECT_TRUE(IsInstanceWriter(rds_client, cluster_id, after_id))
        << "Expected post-failover connection to be the new writer, got " << after_id;
    EXPECT_EQ(test_uid, after_user)
        << "Post-failover session must still be the SSO DB user (SSO re-auth via cached token).";

    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
