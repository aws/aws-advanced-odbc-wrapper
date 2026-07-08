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

#include <gtest/gtest.h>

#include "../../driver/driver.h"
#include "../../driver/odbcapi_rds_helper.h"
#include "../../driver/error.h"

class ErrorHandlingTest : public testing::Test {
protected:
    ENV* env = nullptr;
    DBC* dbc = nullptr;
    STMT* stmt = nullptr;
    DESC* desc = nullptr;

    void SetUp() override {
        env = new ENV();
        dbc = new DBC();
        dbc->env = env;
        stmt = new STMT();
        stmt->dbc = dbc;
        desc = new DESC();
        desc->dbc = dbc;
    }

    void TearDown() override {
        delete desc;
        delete stmt;
        delete dbc;
        delete env;
    }
};

// HasEnvAccess tests

TEST_F(ErrorHandlingTest, HasEnvAccess_NullEnv) {
    EXPECT_FALSE(HasEnvAccess<ENV>(nullptr));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_ValidEnv) {
    EXPECT_TRUE(HasEnvAccess<ENV>(env));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_NullDbc) {
    EXPECT_FALSE(HasEnvAccess<DBC>(nullptr));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_DbcNullEnv) {
    DBC local_dbc{};
    local_dbc.env = nullptr;
    EXPECT_FALSE(HasEnvAccess<DBC>(&local_dbc));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_ValidDbc) {
    EXPECT_TRUE(HasEnvAccess<DBC>(dbc));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_NullStmt) {
    EXPECT_FALSE(HasEnvAccess<STMT>(nullptr));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_StmtNullDbc) {
    STMT local_stmt{};
    local_stmt.dbc = nullptr;
    EXPECT_FALSE(HasEnvAccess<STMT>(&local_stmt));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_StmtDbcNullEnv) {
    DBC local_dbc{};
    local_dbc.env = nullptr;
    STMT local_stmt{};
    local_stmt.dbc = &local_dbc;
    EXPECT_FALSE(HasEnvAccess<STMT>(&local_stmt));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_ValidStmt) {
    EXPECT_TRUE(HasEnvAccess<STMT>(stmt));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_NullDesc) {
    EXPECT_FALSE(HasEnvAccess<DESC>(nullptr));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_ValidDesc) {
    EXPECT_TRUE(HasEnvAccess<DESC>(desc));
}

// HasWrappedHandle tests

TEST_F(ErrorHandlingTest, HasWrappedHandle_EnvNull) {
    EXPECT_FALSE(HasWrappedHandle(env));
}

TEST_F(ErrorHandlingTest, HasWrappedHandle_DbcNull) {
    EXPECT_FALSE(HasWrappedHandle(dbc));
}

TEST_F(ErrorHandlingTest, HasWrappedHandle_StmtNull) {
    EXPECT_FALSE(HasWrappedHandle(stmt));
}

TEST_F(ErrorHandlingTest, HasWrappedHandle_DescNull) {
    EXPECT_FALSE(HasWrappedHandle(desc));
}

// ClearError tests

TEST_F(ErrorHandlingTest, ClearError_NullSafe) {
    ClearError(static_cast<ENV*>(nullptr));
    ClearError(static_cast<DBC*>(nullptr));
    ClearError(static_cast<STMT*>(nullptr));
    ClearError(static_cast<DESC*>(nullptr));
}

TEST_F(ErrorHandlingTest, ClearError_ResetsEnv) {
    env->err = std::make_unique<ERR_INFO>("test", ERR_GENERAL_ERROR);
    env->sql_error_called = 1;
    ClearError(env);
    EXPECT_FALSE(env->err);
    EXPECT_EQ(0, env->sql_error_called);
}

TEST_F(ErrorHandlingTest, ClearError_ResetsDbc) {
    dbc->err = std::make_unique<ERR_INFO>("test", ERR_GENERAL_ERROR);
    dbc->sql_error_called = 1;
    ClearError(dbc);
    EXPECT_FALSE(dbc->err);
    EXPECT_EQ(0, dbc->sql_error_called);
}

TEST_F(ErrorHandlingTest, ClearError_ResetsStmt) {
    stmt->err = std::make_unique<ERR_INFO>("test", ERR_GENERAL_ERROR);
    stmt->sql_error_called = 1;
    ClearError(stmt);
    EXPECT_FALSE(stmt->err);
    EXPECT_EQ(0, stmt->sql_error_called);
}

TEST_F(ErrorHandlingTest, ClearError_ResetsDesc) {
    desc->err = std::make_unique<ERR_INFO>("test", ERR_GENERAL_ERROR);
    desc->sql_error_called = 1;
    ClearError(desc);
    EXPECT_FALSE(desc->err);
    EXPECT_EQ(0, desc->sql_error_called);
}

// NextErrorRecord tests

TEST_F(ErrorHandlingTest, NextErrorRecord_NullReturnsZero) {
    EXPECT_EQ(0, NextErrorRecord<ENV>(nullptr));
    EXPECT_EQ(0, NextErrorRecord<DBC>(nullptr));
    EXPECT_EQ(0, NextErrorRecord<STMT>(nullptr));
    EXPECT_EQ(0, NextErrorRecord<DESC>(nullptr));
}

TEST_F(ErrorHandlingTest, NextErrorRecord_FirstCallReturnsOne) {
    env->sql_error_called = 0;
    EXPECT_EQ(1, NextErrorRecord<ENV>(env));
    EXPECT_EQ(1, env->sql_error_called);
}

TEST_F(ErrorHandlingTest, NextErrorRecord_SecondCallReturnsZero) {
    env->sql_error_called = 0;
    NextErrorRecord<ENV>(env);
    EXPECT_EQ(0, NextErrorRecord<ENV>(env));
}

// Invalid handle return tests

TEST_F(ErrorHandlingTest, RdsAllocDbc_NullEnvReturnsInvalidHandle) {
    SQLHDBC out = nullptr;
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_AllocDbc(nullptr, &out));
}

TEST_F(ErrorHandlingTest, RdsFreeEnv_NullReturnsInvalidHandle) {
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_FreeEnv(nullptr));
}

// ERR_INFO unique_ptr ownership tests

TEST_F(ErrorHandlingTest, UniquePtr_OwnershipTransfer) {
    auto err = std::make_unique<ERR_INFO>("original", ERR_GENERAL_ERROR);
    stmt->err = std::move(err);
    EXPECT_TRUE(stmt->err);
    EXPECT_STREQ("original", stmt->err->error_msg);
    EXPECT_STREQ("HY000", stmt->err->sqlstate);
}

TEST_F(ErrorHandlingTest, UniquePtr_ReassignmentDeletesPrevious) {
    stmt->err = std::make_unique<ERR_INFO>("first", ERR_GENERAL_ERROR);
    stmt->err = std::make_unique<ERR_INFO>("second", ERR_MEMORY_ALLOCATION_ERROR);
    EXPECT_STREQ("second", stmt->err->error_msg);
    EXPECT_STREQ("HY001", stmt->err->sqlstate);
}

TEST_F(ErrorHandlingTest, UniquePtr_ResetCleansUp) {
    stmt->err = std::make_unique<ERR_INFO>("test", ERR_GENERAL_ERROR);
    stmt->err.reset();
    EXPECT_FALSE(stmt->err);
}

// SQLGetDiagRec returns correct messages

TEST_F(ErrorHandlingTest, DiagRec_ReturnsErrMessage) {
    dbc->err = std::make_unique<ERR_INFO>("Connection failed", ERR_COMMUNICATION_LINK_FAILURE);

    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = {0};
    SQLINTEGER native_error = 0;
    SQLTCHAR message[MAX_MSG_LEN] = {0};
    SQLSMALLINT text_length = 0;

    SQLRETURN ret = RDS_SQLGetDiagRec(
        SQL_HANDLE_DBC, dbc, 1,
        sql_state, &native_error, message, MAX_MSG_LEN, &text_length, false);

    EXPECT_EQ(SQL_SUCCESS, ret);
#ifndef UNICODE
    EXPECT_STREQ("08S01", reinterpret_cast<const char*>(sql_state));
    EXPECT_STREQ("Connection failed", reinterpret_cast<const char*>(message));
#endif
    EXPECT_EQ(static_cast<SQLINTEGER>(ERR_COMMUNICATION_LINK_FAILURE), native_error);
}

TEST_F(ErrorHandlingTest, DiagRec_RecordTwoReturnsNoData) {
    dbc->err = std::make_unique<ERR_INFO>("test", ERR_GENERAL_ERROR);

    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = {0};
    SQLINTEGER native_error = 0;
    SQLTCHAR message[MAX_MSG_LEN] = {0};
    SQLSMALLINT text_length = 0;

    SQLRETURN ret = RDS_SQLGetDiagRec(
        SQL_HANDLE_DBC, dbc, 2,
        sql_state, &native_error, message, MAX_MSG_LEN, &text_length, false);

    EXPECT_EQ(SQL_NO_DATA, ret);
}

TEST_F(ErrorHandlingTest, DiagRec_InvalidHandleType) {
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = {0};
    SQLINTEGER native_error = 0;
    SQLTCHAR message[MAX_MSG_LEN] = {0};
    SQLSMALLINT text_length = 0;

    SQLRETURN ret = RDS_SQLGetDiagRec(
        99, dbc, 1,
        sql_state, &native_error, message, MAX_MSG_LEN, &text_length, false);

    EXPECT_EQ(SQL_INVALID_HANDLE, ret);
}
