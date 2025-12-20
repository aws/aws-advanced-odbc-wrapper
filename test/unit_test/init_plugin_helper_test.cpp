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

#include <map>

#include "auth_mock_objects.h"

#include "../../driver/dialect/dialect.h"
#include "../../driver/util/init_plugin_helper.h"

#include "../../driver/dialect/dialect_aurora_mysql.h"
#include "../../driver/dialect/dialect_aurora_postgres.h"

class InitPluginHelperTest : public testing::Test {};

TEST_F(InitPluginHelperTest, TestInitDialect_PG) {
    const std::map<std::string, std::string> attr = {
        {KEY_DATABASE_DIALECT, VALUE_DB_DIALECT_AURORA_POSTGRESQL}
    };
    std::shared_ptr<Dialect> dialect = InitDialect(attr);
    DialectAuroraPostgres* res = dynamic_cast<DialectAuroraPostgres*>(dialect.get());
    if (!res) {
        GTEST_FAIL() << "Dialect is not the expected type 'DialectAuroraPostgres'";
    }
}

TEST_F(InitPluginHelperTest, TestInitDialect_PG_Limitless) {
    const std::map<std::string, std::string> attr = {
        {KEY_DATABASE_DIALECT, VALUE_DB_DIALECT_AURORA_POSTGRESQL_LIMITLESS}
    };
    std::shared_ptr<Dialect> dialect = InitDialect(attr);
    DialectAuroraPostgresLimitless* res = dynamic_cast<DialectAuroraPostgresLimitless*>(dialect.get());
    if (!res) {
        GTEST_FAIL() << "Dialect is not the expected type 'DialectAuroraPostgresLimitless'";
    }
}

TEST_F(InitPluginHelperTest, TestInitDialect_PG_Server) {
    const std::map<std::string, std::string> attr = {
        {KEY_DATABASE_DIALECT, ""},
        {KEY_SERVER, "database-test-name.cluster-XYZ.us-east-2.rds.amazonaws.com"},

    };
    std::shared_ptr<Dialect> dialect = InitDialect(attr);
    DialectAuroraPostgres* res = dynamic_cast<DialectAuroraPostgres*>(dialect.get());
    if (!res) {
        GTEST_FAIL() << "Dialect is not the expected type 'DialectAuroraPostgres'";
    }
}

TEST_F(InitPluginHelperTest, TestInitDialect_PG_LIMITLESS_Server) {
    const std::map<std::string, std::string> attr = {
        {KEY_DATABASE_DIALECT, ""},
        {KEY_SERVER, "database-test-name.shardgrp-XYZ.us-east-2.rds.amazonaws.com"},

    };
    std::shared_ptr<Dialect> dialect = InitDialect(attr);
    DialectAuroraPostgresLimitless* res = dynamic_cast<DialectAuroraPostgresLimitless*>(dialect.get());
    if (!res) {
        GTEST_FAIL() << "Dialect is not the expected type 'DialectAuroraPostgresLimitless'";
    }
}

TEST_F(InitPluginHelperTest, TestInitDialect_MySQL) {
    const std::map<std::string, std::string> attr = {
        {KEY_DATABASE_DIALECT, VALUE_DB_DIALECT_AURORA_MYSQL}
    };
    std::shared_ptr<Dialect> dialect = InitDialect(attr);
    DialectAuroraMySql* res = dynamic_cast<DialectAuroraMySql*>(dialect.get());
    if (!res) {
        GTEST_FAIL() << "Dialect is not the expected type 'DialectAuroraMySql'";
    }
}

TEST_F(InitPluginHelperTest, TestInitDialect_Unknown) {
    const std::map<std::string, std::string> attr = {
        {KEY_DATABASE_DIALECT, ""},
        {KEY_SERVER, ""}
    };
    std::shared_ptr<Dialect> dialect = InitDialect(attr);
    Dialect* res = dialect.get();
    if (!res) {
        GTEST_FAIL() << "Dialect is not the expected type 'Dialect'";
    }
}
