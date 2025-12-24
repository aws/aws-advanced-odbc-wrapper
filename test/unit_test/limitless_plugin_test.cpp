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
#include "../../driver/plugin/limitless/limitless_plugin.h"
#include "../../driver/util/connection_string_keys.h"

class LimitlessPluginTest : public testing::Test {
protected:
    MOCK_BASE_PLUGIN* mock_base_plugin;
    DBC* dbc;

    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    // Runs per test
    void SetUp() override {
        mock_base_plugin = new MOCK_BASE_PLUGIN();
        dbc = new DBC();
    }
    void TearDown() override {
        // mock_base_plugin should be cleaned up by plugin chain
        if (dbc) delete dbc;
    }
};

TEST_F(LimitlessPluginTest, Connect_Success) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));
    dbc->conn_attr.insert_or_assign(KEY_ENABLE_LIMITLESS, VALUE_BOOL_TRUE);
    std::shared_ptr<DialectAuroraPostgresLimitless> dialect = std::make_shared<DialectAuroraPostgresLimitless>();
    std::shared_ptr<MockLimitlessRouterService> mock_router_service = std::make_shared<MockLimitlessRouterService>(dialect, dbc->conn_attr, std::make_shared<OdbcHelper>(nullptr));
    EXPECT_CALL(*mock_router_service, StartMonitoring(testing::_, testing::_)).Times(testing::Exactly(1));
    EXPECT_CALL(*mock_router_service, EstablishConnection(testing::_, testing::_)).Times(testing::Exactly(1)).WillOnce(testing::Return(SQL_SUCCESS));
    LimitlessPlugin plugin(dbc, mock_base_plugin, dialect, mock_router_service, nullptr);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
}

TEST_F(LimitlessPluginTest, Connect_LimitlessDisabled) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SQL_SUCCESS));
    dbc->conn_attr.insert_or_assign(KEY_ENABLE_LIMITLESS, VALUE_BOOL_FALSE);
    std::shared_ptr<DialectAuroraPostgresLimitless> dialect = std::make_shared<DialectAuroraPostgresLimitless>();
    std::shared_ptr<MockLimitlessRouterService> mock_router_service = std::make_shared<MockLimitlessRouterService>(dialect, dbc->conn_attr, std::make_shared<OdbcHelper>(nullptr));
    EXPECT_CALL(*mock_router_service, StartMonitoring(testing::_, testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*mock_router_service, EstablishConnection(testing::_, testing::_)).Times(testing::Exactly(0));
    LimitlessPlugin plugin(dbc, mock_base_plugin, dialect, mock_router_service, nullptr);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_SUCCESS, ret);
}

TEST_F(LimitlessPluginTest, IncorrectDialect) {
    EXPECT_CALL(
        *mock_base_plugin,
        Connect(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .Times(testing::Exactly(0));
    std::shared_ptr<DialectAuroraPostgres> dialect = std::make_shared<DialectAuroraPostgres>();
    LimitlessPlugin plugin(dbc, mock_base_plugin, dialect, nullptr, nullptr);
    SQLRETURN ret = plugin.Connect(dbc, nullptr, nullptr, 0, 0, SQL_DRIVER_NOPROMPT);
    EXPECT_EQ(SQL_ERROR, ret);
}
