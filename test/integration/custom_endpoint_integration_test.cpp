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

class CustomEndpointIntegrationTest : public BaseFailoverIntegrationTest {
protected:
    std::string access_key = TEST_UTILS::GetEnvVar("AWS_ACCESS_KEY_ID");
    std::string secret_access_key = TEST_UTILS::GetEnvVar("AWS_SECRET_ACCESS_KEY");
    std::string session_token = TEST_UTILS::GetEnvVar("AWS_SESSION_TOKEN");
    std::string rds_endpoint = TEST_UTILS::GetEnvVar("RDS_ENDPOINT");
    std::string rds_region = TEST_UTILS::GetEnvVar("TEST_REGION", "us-west-1");
    Aws::RDS::RDSClientConfiguration client_config;
    Aws::RDS::RDSClient rds_client;

    std::string custom_endpoint_id = TEST_UTILS::GetEnvVar("TEST_CUSTOM_ENDPOINT_ID");

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

        // Check to see if cluster is available.
        WaitForDbReady(rds_client, cluster_id);
    }

    void TearDown() override {
        BaseFailoverIntegrationTest::TearDown();
    }
};

TEST_F(CustomEndpointIntegrationTest, CustomEndpointFailover) {
    // Connect using custom endpoint
    const auto endpoint_info = GetCustomEndpointInfo(rds_client, custom_endpoint_id);
    const std::vector<std::string>& endpoint_members = endpoint_info.GetStaticMembers();
    conn_str = ConnectionStringBuilder(test_dsn, endpoint_info.GetEndpoint(), 5432)
        .withUID(test_uid)
        .withPWD(test_pwd)
        .withDatabase(test_db)
        .withCustomEndpoint(true)
        .withEnableClusterFailover(true)
        .withDatabaseDialect(test_dialect)
        .getString();
    SQLRETURN rc = ODBC_HELPER::DriverConnect(dbc, conn_str);
    EXPECT_EQ(SQL_SUCCESS, rc);

    // Failover cluster to ensure connection swaps
    FailoverClusterWaitDesiredWriter(rds_client, cluster_id, writer_id);

    // Ensure failover is successful
    AssertQueryFail(dbc, SERVER_ID_QUERY, ERROR_FAILOVER_SUCCEEDED);

    // Query new ID after failover
    const std::string connection_id = QueryInstanceId(dbc);
    // Ensure connected to an instance within the static list
    EXPECT_NE(std::find(endpoint_members.begin(), endpoint_members.end(), connection_id), endpoint_members.end());

    // Cleanup
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
