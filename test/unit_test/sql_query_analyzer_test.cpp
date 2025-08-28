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

#include "../../driver/util/sql_query_analyzer.h"

class SqlQueryAnalyzerTest : public testing::Test {
protected:
    DBC* dbc_manual_commit;
    DBC* dbc_auto_commit;
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}

    void SetUp() override {
        dbc_manual_commit = new DBC();
        dbc_manual_commit->auto_commit = false;
        dbc_auto_commit = new DBC();
        dbc_auto_commit->auto_commit = true;
    }
    void TearDown() override {
        if (dbc_manual_commit) delete dbc_manual_commit;
        if (dbc_auto_commit) delete dbc_auto_commit;
    }
};

TEST_F(SqlQueryAnalyzerTest, GetFirstSqlStatement) {
    EXPECT_STREQ("SELECT 1", SqlQueryAnalyzer::GetFirstSqlStatement("select 1; GARBAGE VALUE;").c_str());
    EXPECT_STREQ("SELECT 1", SqlQueryAnalyzer::GetFirstSqlStatement("SELECT 1; GARBAGE VALUE;").c_str());
    EXPECT_STREQ("SELECT 1", SqlQueryAnalyzer::GetFirstSqlStatement("     SELECT 1; GARBAGE VALUE;").c_str());
    EXPECT_STREQ("SELECT 1", SqlQueryAnalyzer::GetFirstSqlStatement("/* Comment */ SELECT /* Comment */ 1 /* Comment */; GARBAGE VALUE;").c_str());
    EXPECT_STREQ("SET AUTOCOMMIT = 1", SqlQueryAnalyzer::GetFirstSqlStatement("set autocommit = 1").c_str());
    EXPECT_STREQ("SET AUTOCOMMIT=1", SqlQueryAnalyzer::GetFirstSqlStatement("set autocommit=1").c_str());
    EXPECT_STREQ("SET AUTOCOMMIT=1", SqlQueryAnalyzer::GetFirstSqlStatement("set   /* Comment */   autocommit=1").c_str());
}

TEST_F(SqlQueryAnalyzerTest, DoesOpenTransaction) {
    // True
    EXPECT_TRUE(SqlQueryAnalyzer::DoesOpenTransaction("begin"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesOpenTransaction("/**/ begin"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesOpenTransaction("begin /**/ "));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesOpenTransaction("begin /* Comment */ transaction"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesOpenTransaction("start TRANSACTION"));
    // False
    EXPECT_FALSE(SqlQueryAnalyzer::DoesOpenTransaction("Rollback"));
    EXPECT_FALSE(SqlQueryAnalyzer::DoesOpenTransaction("SELECT 1234;"));
}

TEST_F(SqlQueryAnalyzerTest, DoesCloseTransaction) {
    // True
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_auto_commit, "rollback"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_auto_commit, "commit"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_auto_commit, "end"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_auto_commit, "abort"));
    // False
    EXPECT_FALSE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_auto_commit, "select 1"));
    EXPECT_FALSE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_auto_commit, "SELECT 1234"));

    // True | w/o Autocommit
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "set autocommit = 1"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "     set autocommit = 1     "));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "  set autocommit=1  "));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "set autocommit = true"));
    EXPECT_TRUE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "set autocommit = on"));
    // False | w/o Autocommit
    EXPECT_FALSE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "set autocommit = false"));
    EXPECT_FALSE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "set autocommit = 0"));
    EXPECT_FALSE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "set autocommit = off"));
    EXPECT_FALSE(SqlQueryAnalyzer::DoesCloseTransaction(dbc_manual_commit, "set autocommit = garbage"));
}

TEST_F(SqlQueryAnalyzerTest, IsStatementSettingAutoCommit) {
    // True
    EXPECT_TRUE(SqlQueryAnalyzer::IsStatementSettingAutoCommit("SET AUTOCOMMIT"));
    EXPECT_TRUE(SqlQueryAnalyzer::IsStatementSettingAutoCommit("              SET AUTOCOMMIT"));
    EXPECT_TRUE(SqlQueryAnalyzer::IsStatementSettingAutoCommit("/* Comment */ Set autocommit"));
    EXPECT_TRUE(SqlQueryAnalyzer::IsStatementSettingAutoCommit("/* Comment */ Set autocommit    "));
    EXPECT_TRUE(SqlQueryAnalyzer::IsStatementSettingAutoCommit("Set /* Comment */ autocommit"));
    // False
    EXPECT_FALSE(SqlQueryAnalyzer::IsStatementSettingAutoCommit("select 1"));
    EXPECT_FALSE(SqlQueryAnalyzer::IsStatementSettingAutoCommit("SELECT 1234"));
}
