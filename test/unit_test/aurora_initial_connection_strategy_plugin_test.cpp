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

using ::testing::NiceMock;

class AuroraInitialConnectionStrategyPluginTest : public ::testing::Test {
protected:
    std::shared_ptr<NiceMock<MockBasePlugin>> mock_base_plugin_ = nullptr;
    std::shared_ptr<NiceMock<MockPluginService>> mock_plugin_service_;
    std::shared_ptr<NiceMock<MockHostListProvider>> mock_host_list_provider_;
    std::shared_ptr<NiceMock<MockOdbcHelper>> mock_odbc_helper_;
    std::shared_ptr<NiceMock<MockDialect>> mock_dialect_;
    std::shared_ptr<NiceMock<MockHighestWeightHostSelector>> mock_host_selector_;
    std::shared_ptr<NiceMock<MockTopologyUtil>> mock_topology_util_;
    DBC* dbc_ = nullptr;
    const std::string writer_cluster_dns_ = "database-test-name.cluster-XYZ.us-east-2.rds.amazonaws.com";
    const std::string reader_cluster_dns_ = "database-test-name.cluster-ro-XYZ.us-east-2.rds.amazonaws.com";

    std::shared_ptr<HostInfo> empty_host_ = std::make_shared<HostInfo>();
    std::shared_ptr<HostInfo> writer_host_ = std::make_shared<HostInfo>("instance-1.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, WRITER, HostInfo::DEFAULT_WEIGHT);
    std::shared_ptr<HostInfo> reader_host_a_ = std::make_shared<HostInfo>("instance-2.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, READER, 3);
    std::shared_ptr<HostInfo> reader_host_b_ = std::make_shared<HostInfo>("instance-3.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, READER, 2);
    std::shared_ptr<HostInfo> reader_host_c_ = std::make_shared<HostInfo>("instance-4.XYZ.us-east-2.rds.amazonaws.com", 1234, UP, READER, 1);
    std::vector<HostInfo> all_hosts_;

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    // Runs per test
    void SetUp() override {
        mock_base_plugin_ = std::make_shared<NiceMock<MockBasePlugin>>();
        mock_plugin_service_ = std::make_shared<NiceMock<MockPluginService>>();
        mock_host_list_provider_ = std::make_shared<NiceMock<MockHostListProvider>>();
        mock_odbc_helper_ = std::make_shared<NiceMock<MockOdbcHelper>>();
        mock_dialect_ = std::make_shared<NiceMock<MockDialect>>();
        mock_host_selector_ = std::make_shared<NiceMock<MockHighestWeightHostSelector>>();
        mock_topology_util_ = std::make_shared<NiceMock<MockTopologyUtil>>(mock_odbc_helper_, mock_dialect_);
        ON_CALL(*mock_topology_util_, GetWriter).WillByDefault(testing::Return(*writer_host_));
        ON_CALL(*mock_plugin_service_, GetHostListProvider).WillByDefault(testing::Return(mock_host_list_provider_));
        ON_CALL(*mock_plugin_service_, GetTopologyUtil).WillByDefault(testing::Return(mock_topology_util_));

        dbc_ = new DBC();
        dbc_->plugin_service = mock_plugin_service_;

        all_hosts_.push_back(*writer_host_);
        all_hosts_.push_back(*reader_host_a_);
        all_hosts_.push_back(*reader_host_b_);
        all_hosts_.push_back(*reader_host_c_);
    }
    void TearDown() override {
        if (dbc_) delete dbc_;
        all_hosts_.clear();
    }
};

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_NonRdsCluster_Fallback) {
    std::string non_rds_url = "someNonRdsUrl.com";
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .Times(testing::Exactly(0));

    dbc_->conn_attr.insert_or_assign(KEY_SERVER, non_rds_url);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), non_rds_url);
}


TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_DSN) {
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .WillRepeatedly(testing::Return(all_hosts_));

    EXPECT_CALL(*mock_topology_util_, GetWriter).WillOnce(testing::Return(*writer_host_));

    dbc_->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns_);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), writer_host_->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_Configured) {
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .WillRepeatedly(testing::Return(all_hosts_));

    EXPECT_CALL(*mock_topology_util_, GetWriter).WillOnce(testing::Return(*writer_host_));

    dbc_->conn_attr.insert_or_assign(KEY_VERIFY_INITIAL_CONNECTION_TYPE, "WRITER");
    dbc_->conn_attr.insert_or_assign(KEY_SERVER, reader_cluster_dns_);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), writer_host_->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_Cannot_Find_Writer_Retry) {
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(2))
    .WillRepeatedly(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .WillRepeatedly(testing::Return(all_hosts_));

    ON_CALL(*mock_topology_util_, GetWriter).WillByDefault(testing::Return(*writer_host_));
    EXPECT_CALL(*mock_topology_util_, GetWriter(testing::_))
        .WillOnce(testing::Return(*empty_host_))
        .WillOnce(testing::Return(*writer_host_));

    EXPECT_CALL(*mock_host_list_provider_, GetConnectionInfo)
    .WillOnce(testing::Return(*reader_host_a_));

    EXPECT_CALL(*mock_odbc_helper_, Disconnect(testing::A<DBC*>()))
    .Times(testing::Exactly(1));

    EXPECT_CALL(
        *mock_plugin_service_,
        ForceRefreshHosts(testing::_, testing::_))
    .Times(testing::Exactly(1));

    dbc_->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns_);
    dbc_->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, "10");
    dbc_->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, "5000");
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), writer_host_->GetHost());
}


TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Writer_Network_Error_Retry) {
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(2))
    .WillOnce(testing::Return(SQL_ERROR))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_dialect_,
        IsSqlStateNetworkError(testing::_))
    .Times(testing::Exactly(1))
    .WillRepeatedly(testing::Return(true));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .WillRepeatedly(testing::Return(all_hosts_));

    EXPECT_CALL(*mock_topology_util_, GetWriter)
    .Times(testing::Exactly(2))
    .WillRepeatedly(testing::Return(*writer_host_));

    EXPECT_CALL(*mock_odbc_helper_, Disconnect(testing::A<DBC*>()))
    .Times(testing::Exactly(1));

    dbc_->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns_);
    dbc_->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, "10");
    dbc_->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, "5000");
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), writer_host_->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Reader_DSN) {
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .WillRepeatedly(testing::Return(all_hosts_));

    EXPECT_CALL(*mock_host_list_provider_,
        GetConnectionRole(testing::_))
    .WillOnce(testing::Return(READER));

    dbc_->conn_attr.insert_or_assign(KEY_SERVER, reader_cluster_dns_);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), reader_host_a_->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Reader_Configured) {
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .Times(testing::Exactly(1))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .WillRepeatedly(testing::Return(all_hosts_));

    EXPECT_CALL(*mock_host_list_provider_,
        GetConnectionRole(testing::_))
    .WillOnce(testing::Return(READER));

    dbc_->conn_attr.insert_or_assign(KEY_VERIFY_INITIAL_CONNECTION_TYPE, "READER");
    dbc_->conn_attr.insert_or_assign(KEY_SERVER, writer_cluster_dns_);
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), reader_host_a_->GetHost());
}

TEST_F(AuroraInitialConnectionStrategyPluginTest, Connect_Success_Reader_Network_Error_Retry) {
    EXPECT_CALL(
        *mock_base_plugin_,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
    .WillOnce(testing::Return(SQL_ERROR))
    .WillOnce(testing::Return(SQL_SUCCESS));

    EXPECT_CALL(
        *mock_dialect_,
        IsSqlStateNetworkError(testing::_))
    .Times(testing::Exactly(1))
    .WillRepeatedly(testing::Return(true));

    EXPECT_CALL(*mock_plugin_service_,
        GetHosts())
    .WillRepeatedly(testing::Return(all_hosts_));

    EXPECT_CALL(*mock_odbc_helper_, Disconnect(testing::A<DBC*>()))
    .Times(testing::Exactly(1));

    EXPECT_CALL(*mock_host_list_provider_, GetConnectionRole)
    .Times(testing::Exactly(1))
    .WillRepeatedly(testing::Return(READER));

    dbc_->conn_attr.insert_or_assign(KEY_SERVER, reader_cluster_dns_);
    dbc_->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_INTERVAL_MS, "10");
    dbc_->conn_attr.insert_or_assign(KEY_INITIAL_CONNECTION_RETRY_TIMEOUT_MS, "5000");
    AuroraInitialConnectionStrategyPlugin plugin(
        dbc_,
        mock_base_plugin_,
        mock_plugin_service_,
        mock_host_selector_,
        mock_dialect_,
        mock_odbc_helper_);

    SQLRETURN ret = plugin.Connect(dbc_, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
    EXPECT_EQ(dbc_->conn_attr.at(KEY_SERVER), reader_host_a_->GetHost());
}
