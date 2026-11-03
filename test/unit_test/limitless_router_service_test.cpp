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
#include "limitless_mock_objects.h"

#include "../../driver/driver.h"
#include "../../driver/dialect/dialect_aurora_postgres.h"
#include "../../driver/plugin/limitless/limitless_router_service.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/util/plugin_service.h"

class LimitlessRouterServiceTest : public testing::Test {
protected:
    std::shared_ptr<MockBasePlugin> mock_base_plugin_;
    DBC* dbc_;
    std::shared_ptr<OdbcHelper> odbc_helper_;

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    // Runs per test
    void SetUp() override {
        mock_base_plugin_ = std::make_shared<MockBasePlugin>();
        dbc_ = new DBC();
        dbc_->plugin_head = mock_base_plugin_.get();
        dbc_->plugin_service = std::make_shared<PluginService>();
        dbc_->plugin_service->SetPluginChain(mock_base_plugin_);
        odbc_helper_ = std::make_shared<OdbcHelper>(nullptr, nullptr);
    }
    void TearDown() override {
        if (dbc_) delete dbc_;
    }
};

TEST_F(LimitlessRouterServiceTest, EstablishConnectionWithNullDbcReturnsInvalidHandle) {
    std::shared_ptr<DialectLimitless> dialect = std::make_shared<DialectAuroraPostgresLimitless>();
    TestLimitlessRouterService router_service(dialect, {}, odbc_helper_);

    EXPECT_EQ(SQL_INVALID_HANDLE, router_service.EstablishConnection(mock_base_plugin_, nullptr));
}

TEST_F(LimitlessRouterServiceTest, EstablishConnectionWithNullNextPluginReturnsErrorAndSetsDiagnostic) {
    std::shared_ptr<DialectLimitless> dialect = std::make_shared<DialectAuroraPostgresLimitless>();
    TestLimitlessRouterService router_service(dialect, {}, odbc_helper_);

    const SQLRETURN ret = router_service.EstablishConnection(nullptr, dbc_);

    EXPECT_EQ(SQL_ERROR, ret);
    ASSERT_NE(nullptr, dbc_->err);
    EXPECT_EQ("HY000", dbc_->err->sqlstate);
}

TEST_F(LimitlessRouterServiceTest, LimitlessRouterMonitorReferenceCountingTest) {
    const std::map<std::string, std::string> attr1 = {
        {KEY_SERVER, "server_1"},
        {KEY_CLUSTER_ID, "cluster_1"}
    };
    const std::map<std::string, std::string> attr2 = {
        {KEY_SERVER, "server_2"},
        {KEY_CLUSTER_ID, "cluster_2"}
    };

    std::shared_ptr<DialectLimitless> dialect = std::make_shared<DialectAuroraPostgresLimitless>();

    dbc_->conn_attr = attr1;
    TestLimitlessRouterService* router_service1 = new TestLimitlessRouterService(dialect, attr1, odbc_helper_);
    TestLimitlessRouterService* router_service2 = new TestLimitlessRouterService(dialect, attr1, odbc_helper_);
    router_service1->StartMonitoring(dbc_, dialect);
    router_service2->StartMonitoring(dbc_, dialect);

    dbc_->conn_attr = attr2;
    TestLimitlessRouterService* router_service3 = new TestLimitlessRouterService(dialect, attr2, odbc_helper_);
    router_service3->StartMonitoring(dbc_, dialect);

    std::pair<unsigned int, std::shared_ptr<LimitlessRouterMonitor>> pair = LimitlessRouterService::limitless_router_monitors_.at("cluster_1");
    EXPECT_EQ(2, pair.first);
    pair = LimitlessRouterService::limitless_router_monitors_.at("cluster_2");
    EXPECT_EQ(1, pair.first);

    delete router_service1;
    pair = LimitlessRouterService::limitless_router_monitors_.at("cluster_1");
    EXPECT_EQ(1, pair.first);
    pair = LimitlessRouterService::limitless_router_monitors_.at("cluster_2");
    EXPECT_EQ(1, pair.first);

    delete router_service2;
    auto itr = LimitlessRouterService::limitless_router_monitors_.find("cluster_1");
    EXPECT_EQ(LimitlessRouterService::limitless_router_monitors_.end(), itr);
    pair = LimitlessRouterService::limitless_router_monitors_.at("cluster_2");
    EXPECT_EQ(1, pair.first);

    delete router_service3;
    itr = LimitlessRouterService::limitless_router_monitors_.find("cluster_2");
    EXPECT_EQ(LimitlessRouterService::limitless_router_monitors_.end(), itr);
}

TEST_F(LimitlessRouterServiceTest, LimitlessRouterMonitorKeyedByClusterId) {
    std::shared_ptr<DialectLimitless> dialect = std::make_shared<DialectAuroraPostgresLimitless>();

    // Different hosts of one cluster share a monitor
    const std::map<std::string, std::string> shard_group_attr = {
        {KEY_SERVER, "my-db.shardgrp-xyz.us-east-1.rds.amazonaws.com"},
        {KEY_CLUSTER_ID, "shared_cluster"}
    };
    const std::map<std::string, std::string> ip_attr = {
        {KEY_SERVER, "10.0.0.1"},
        {KEY_CLUSTER_ID, "shared_cluster"}
    };
    dbc_->conn_attr = shard_group_attr;
    TestLimitlessRouterService* router_service1 = new TestLimitlessRouterService(dialect, shard_group_attr, odbc_helper_);
    router_service1->StartMonitoring(dbc_, dialect);
    dbc_->conn_attr = ip_attr;
    TestLimitlessRouterService* router_service2 = new TestLimitlessRouterService(dialect, ip_attr, odbc_helper_);
    router_service2->StartMonitoring(dbc_, dialect);
    EXPECT_EQ(2, LimitlessRouterService::limitless_router_monitors_.at("shared_cluster").first);

    // Without a cluster ID the default is used
    const std::map<std::string, std::string> no_cluster_id_attr = {
        {KEY_SERVER, "my-db.shardgrp-xyz.us-east-1.rds.amazonaws.com"}
    };
    dbc_->conn_attr = no_cluster_id_attr;
    TestLimitlessRouterService* router_service3 = new TestLimitlessRouterService(dialect, no_cluster_id_attr, odbc_helper_);
    router_service3->StartMonitoring(dbc_, dialect);
    EXPECT_EQ(1, LimitlessRouterService::limitless_router_monitors_.at(DEFAULT_CLUSTER_ID).first);

    delete router_service1;
    delete router_service2;
    delete router_service3;
    EXPECT_FALSE(LimitlessRouterService::limitless_router_monitors_.contains("shared_cluster"));
    EXPECT_FALSE(LimitlessRouterService::limitless_router_monitors_.contains(DEFAULT_CLUSTER_ID));
}
