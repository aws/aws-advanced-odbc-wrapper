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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "auth_mock_objects.h"
#include "common_mock_objects.h"

#include "../../driver/host_info.h"
#include "../../driver/plugin/aurora_initial_connection_strategy/aurora_initial_connection_strategy_plugin.h"
#include "../../driver/util/connection_string_keys.h"

class AuroraInitialConnectionStrategyPluginTest : public ::testing::Test {
protected:
    MOCK_BASE_PLUGIN* mock_base_plugin = nullptr;
    std::shared_ptr<MOCK_PLUGIN_SERVICE> mock_plugin_service;
    std::shared_ptr<MOCK_HOST_LIST_PROVIDER> mock_host_list_provider;
    std::shared_ptr<MOCK_ODBC_HELPER> mock_odbc_helper;
    std::shared_ptr<MOCK_DIALECT> mock_dialect;
    std::shared_ptr<MOCK_HOST_SELECTOR> mock_host_selector;
    std::shared_ptr<MOCK_TOPOLOGY_UTIL> mock_topology_util;
    DBC* dbc = nullptr;
    const std::string writer_cluster_dns = "database-test-name.cluster-XYZ.us-east-2.rds.amazonaws.com";
    const std::string reader_cluster_dns = "database-test-name.cluster-ro-XYZ.us-east-2.rds.amazonaws.com";

    std::shared_ptr<HostInfo> empty_host = std::make_shared<HostInfo>();
    std::shared_ptr<HostInfo> writer_host = std::make_shared<HostInfo>("instance-1.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, WRITER, HostInfo::DEFAULT_WEIGHT);
    std::shared_ptr<HostInfo> reader_host_a = std::make_shared<HostInfo>("instance-2.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, READER, 3);
    std::shared_ptr<HostInfo> reader_host_b = std::make_shared<HostInfo>("instance-3.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, READER, 2);
    std::shared_ptr<HostInfo> reader_host_c = std::make_shared<HostInfo>("instance-4.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, READER, 1);
    std::vector<HostInfo> all_hosts;

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    // Runs per test
    void SetUp() override {
        mock_base_plugin = new MOCK_BASE_PLUGIN();
        mock_plugin_service = std::make_shared<MOCK_PLUGIN_SERVICE>();
        mock_host_list_provider = std::make_shared<MOCK_HOST_LIST_PROVIDER>();
        mock_odbc_helper = std::make_shared<MOCK_ODBC_HELPER>();
        mock_dialect = std::make_shared<MOCK_DIALECT>();
        mock_host_selector = std::make_shared<MOCK_HOST_SELECTOR>();
        mock_topology_util = std::make_shared<MOCK_TOPOLOGY_UTIL>(mock_odbc_helper, mock_dialect);
        ON_CALL(*mock_topology_util, GetWriter).WillByDefault(testing::Return(*writer_host));

        dbc = new DBC();
        dbc->plugin_service = mock_plugin_service;

        all_hosts.push_back(*writer_host);
        all_hosts.push_back(*reader_host_a);
        all_hosts.push_back(*reader_host_b);
        all_hosts.push_back(*reader_host_c);
    }
    void TearDown() override {
        // mock_base_plugin should be cleaned up by plugin chain
        if (dbc) delete dbc;
        all_hosts.clear();
    }
};

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_NonRdsCluster_Fallback) {
    std::string non_rds_url = "someNonRdsUrl.com";
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(0));

    dbc->conn_attr.insert_or_assign(KEY_SERVER, non_rds_url);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), non_rds_url);
}


TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_DSN) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(all_hosts));

    EXPECT_CALL(*mock_topology_util, GetWriter).WillOnce(testing::Return(*writer_host));

    dbc->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), writer_host->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_Configured) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(all_hosts));

    EXPECT_CALL(*mock_topology_util, GetWriter).WillOnce(testing::Return(*writer_host));

    dbc->conn_attr.insert_or_assign(KEY_VERIFY_INITIAL_CONNECTION_TYPE, "WRITER");
    dbc->conn_attr.insert_or_assign(KEY_SERVER, reader_cluster_dns);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), writer_host->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_Cannot_Find_Writer_Retry) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(2))
    .WillRepeatedly(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(2))
    .WillRepeatedly(testing::Return(all_hosts));

    ON_CALL(*mock_topology_util, GetWriter).WillByDefault(testing::Return(*writer_host));
    EXPECT_CALL(*mock_topology_util, GetWriter(testing::_))
        .WillOnce(testing::Return(*empty_host))
        .WillOnce(testing::Return(*writer_host));

    EXPECT_CALL(*mock_host_list_provider, GetConnectionInfo)
    .WillOnce(testing::Return(*reader_host_a));

    EXPECT_CALL(*mock_odbc_helper, Disconnect(testing::_))
    .Times(testing::Exactly(1));

    EXPECT_CALL(
        *mock_plugin_service,
        ForceRefreshHosts(testing::_, testing::_))
    .Times(testing::Exactly(1));

    dbc->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns);
    dbc->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, "10");
    dbc->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, "100");
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), writer_host->GetHost());
}


TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_Network_Error_Retry) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(2))
    .WillOnce(testing::Return(SQL_ERROR))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_dialect,
        IsSqlStateNetworkError(testing::_))
    .Times(testing::Exactly(1))
    .WillRepeatedly(testing::Return(true));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(2))
    .WillRepeatedly(testing::Return(all_hosts));

    EXPECT_CALL(*mock_topology_util, GetWriter)
    .Times(testing::Exactly(2))
    .WillRepeatedly(testing::Return(*writer_host));

    EXPECT_CALL(*mock_odbc_helper, Disconnect(testing::_))
    .Times(testing::Exactly(1));

    dbc->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns);
    dbc->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, "10");
    dbc->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, "100");
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), writer_host->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Reader_DSN) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(all_hosts));

    EXPECT_CALL(*mock_host_list_provider,
        GetConnectionRole(testing::_))
    .WillOnce(testing::Return(READER));

    dbc->conn_attr.insert_or_assign(KEY_SERVER, reader_cluster_dns);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), reader_host_a->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Reader_Configured) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(all_hosts));

    EXPECT_CALL(*mock_host_list_provider,
        GetConnectionRole(testing::_))
    .WillOnce(testing::Return(READER));

    dbc->conn_attr.insert_or_assign(KEY_VERIFY_INITIAL_CONNECTION_TYPE, "READER");
    dbc->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), reader_host_a->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Reader_Network_Error_Retry) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .WillOnce(testing::Return(SQL_ERROR))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_dialect,
        IsSqlStateNetworkError(testing::_))
    .Times(testing::Exactly(1))
    .WillRepeatedly(testing::Return(true));

    EXPECT_CALL(*mock_plugin_service,
        GetHosts())
    .Times(testing::Exactly(2))
    .WillRepeatedly(testing::Return(all_hosts));

    EXPECT_CALL(*mock_odbc_helper, Disconnect(testing::_))
    .Times(testing::Exactly(1));

    EXPECT_CALL(*mock_host_list_provider, GetConnectionRole)
    .Times(testing::Exactly(1))
    .WillRepeatedly(testing::Return(READER));

    dbc->conn_attr.insert_or_assign(KEY_SERVER, reader_cluster_dns);
     dbc->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, "10");
     dbc->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, "100");
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc,
        mock_base_plugin,
        mock_plugin_service,
        mock_host_list_provider,
        mock_host_selector,
        mock_dialect,
        mock_odbc_helper,
        mock_topology_util);

    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc->conn_attr.at(KEY_SERVER), reader_host_a->GetHost());
}
