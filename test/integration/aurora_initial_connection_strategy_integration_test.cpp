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

class AuroraInitialConnectionStrategyIntegrationTest : public BaseFailoverIntegrationTest {
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
    }

    void TearDown() override {
        BaseFailoverIntegrationTest::TearDown();
    }
};

TEST_F(AuroraInitialConnectionStrategyIntegrationTest, AuroraInitialConnectionStrategyWriterDns) {
    const std::string writer_id = GetWriterId(cluster_instances);
    writer_endpoint = GetEndpoint(writer_id);

    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withAuroraInitialConnectionStrategy(true)
        .withDatabaseDialect(test_dialect)
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    const std::string connection_id = QueryInstanceId(dbc);

    // Ensure connected to writer instance
    EXPECT_EQ(connection_id, writer_id);

    // Cleanup
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(AuroraInitialConnectionStrategyIntegrationTest, AuroraInitialConnectionStrategyReaderDns) {
    conn_str = ConnectionStringBuilder(test_dsn, cluster_id + cluster_ro_url, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withAuroraInitialConnectionStrategy(true)
        .withDatabaseDialect(test_dialect)
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    const std::string connection_id = QueryInstanceId(dbc);

    // Ensure connected to reader instance
    EXPECT_TRUE(IsInstanceReader(rds_client, cluster_id, connection_id));

    // Cleanup
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(AuroraInitialConnectionStrategyIntegrationTest, AuroraInitialConnectionStrategyWriterDnsReaderOverride) {
    conn_str = ConnectionStringBuilder(test_dsn, cluster_id + cluster_ro_url, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect(test_dialect)
        .withAuroraInitialConnectionStrategy(true)
        .withVerifyInitialConnectionType("READER")
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    const std::string connection_id = QueryInstanceId(dbc);

    // Ensure connected to reader instance
    EXPECT_TRUE(IsInstanceReader(rds_client, cluster_id, connection_id));

    // Cleanup
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(AuroraInitialConnectionStrategyIntegrationTest, AuroraInitialConnectionStrategyReaderDnsWriterOverride) {
    const std::string writer_id = GetWriterId(cluster_instances);
    writer_endpoint = GetEndpoint(writer_id);

    conn_str = ConnectionStringBuilder(test_dsn, test_server, test_port)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withDatabaseDialect(test_dialect)
        .withAuroraInitialConnectionStrategy(true)
        .withVerifyInitialConnectionType("WRITER")
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    const std::string connection_id = QueryInstanceId(dbc);

    // Ensure connected to writer instance
    EXPECT_EQ(connection_id, writer_id);

    // Cleanup
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
