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
#include "../../driver/util/connection_string_keys.h"

class ErrorHandlingTest : public testing::Test {
protected:
    ENV* env_ = nullptr;
    DBC* dbc_ = nullptr;
    STMT* stmt_ = nullptr;
    DESC* desc_ = nullptr;

    void SetUp() override {
        env_ = new ENV();
        dbc_ = new DBC();
        dbc_->env = env_;
        stmt_ = new STMT();
        stmt_->dbc = dbc_;
        desc_ = new DESC();
        desc_->dbc = dbc_;
    }

    void TearDown() override {
        delete desc_;
        delete stmt_;
        delete dbc_;
        delete env_;
    }
};

// HasEnvAccess tests

TEST_F(ErrorHandlingTest, HasEnvAccess_NullEnv) {
    EXPECT_FALSE(HasEnvAccess<ENV>(nullptr));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_ValidEnv) {
    EXPECT_TRUE(HasEnvAccess<ENV>(env_));
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
    EXPECT_TRUE(HasEnvAccess<DBC>(dbc_));
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
    EXPECT_TRUE(HasEnvAccess<STMT>(stmt_));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_NullDesc) {
    EXPECT_FALSE(HasEnvAccess<DESC>(nullptr));
}

TEST_F(ErrorHandlingTest, HasEnvAccess_ValidDesc) {
    EXPECT_TRUE(HasEnvAccess<DESC>(desc_));
}

// HasWrappedHandle tests

TEST_F(ErrorHandlingTest, HasWrappedHandle_EnvNull) {
    EXPECT_FALSE(HasWrappedHandle(env_));
}

TEST_F(ErrorHandlingTest, HasWrappedHandle_DbcNull) {
    EXPECT_FALSE(HasWrappedHandle(dbc_));
}

TEST_F(ErrorHandlingTest, HasWrappedHandle_StmtNull) {
    EXPECT_FALSE(HasWrappedHandle(stmt_));
}

TEST_F(ErrorHandlingTest, HasWrappedHandle_DescNull) {
    EXPECT_FALSE(HasWrappedHandle(desc_));
}

// ClearError tests

TEST_F(ErrorHandlingTest, ClearError_NullSafe) {
    ClearError(static_cast<ENV*>(nullptr));
    ClearError(static_cast<DBC*>(nullptr));
    ClearError(static_cast<STMT*>(nullptr));
    ClearError(static_cast<DESC*>(nullptr));
}

TEST_F(ErrorHandlingTest, ClearError_ResetsEnv) {
    env_->err = std::make_unique<ErrInfo>("test", ERR_GENERAL_ERROR);
    env_->sql_error_called = 1;
    ClearError(env_);
    EXPECT_FALSE(env_->err);
    EXPECT_EQ(0, env_->sql_error_called);
}

TEST_F(ErrorHandlingTest, ClearError_ResetsDbc) {
    dbc_->err = std::make_unique<ErrInfo>("test", ERR_GENERAL_ERROR);
    dbc_->sql_error_called = 1;
    ClearError(dbc_);
    EXPECT_FALSE(dbc_->err);
    EXPECT_EQ(0, dbc_->sql_error_called);
}

TEST_F(ErrorHandlingTest, ClearError_ResetsStmt) {
    stmt_->err = std::make_unique<ErrInfo>("test", ERR_GENERAL_ERROR);
    stmt_->sql_error_called = 1;
    ClearError(stmt_);
    EXPECT_FALSE(stmt_->err);
    EXPECT_EQ(0, stmt_->sql_error_called);
}

TEST_F(ErrorHandlingTest, ClearError_ResetsDesc) {
    desc_->err = std::make_unique<ErrInfo>("test", ERR_GENERAL_ERROR);
    desc_->sql_error_called = 1;
    ClearError(desc_);
    EXPECT_FALSE(desc_->err);
    EXPECT_EQ(0, desc_->sql_error_called);
}

// NextErrorRecord tests

TEST_F(ErrorHandlingTest, NextErrorRecord_NullReturnsZero) {
    EXPECT_EQ(0, NextErrorRecord<ENV>(nullptr));
    EXPECT_EQ(0, NextErrorRecord<DBC>(nullptr));
    EXPECT_EQ(0, NextErrorRecord<STMT>(nullptr));
    EXPECT_EQ(0, NextErrorRecord<DESC>(nullptr));
}

TEST_F(ErrorHandlingTest, NextErrorRecord_FirstCallReturnsOne) {
    env_->sql_error_called = 0;
    EXPECT_EQ(1, NextErrorRecord<ENV>(env_));
    EXPECT_EQ(1, env_->sql_error_called);
}

TEST_F(ErrorHandlingTest, NextErrorRecord_SecondCallReturnsZero) {
    env_->sql_error_called = 0;
    NextErrorRecord<ENV>(env_);
    EXPECT_EQ(0, NextErrorRecord<ENV>(env_));
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
    auto err = std::make_unique<ErrInfo>("original", ERR_GENERAL_ERROR);
    stmt_->err = std::move(err);
    EXPECT_TRUE(stmt_->err);
    EXPECT_STREQ("original", stmt_->err->error_msg);
    EXPECT_STREQ("HY000", stmt_->err->sqlstate);
}

TEST_F(ErrorHandlingTest, UniquePtr_ReassignmentDeletesPrevious) {
    stmt_->err = std::make_unique<ErrInfo>("first", ERR_GENERAL_ERROR);
    stmt_->err = std::make_unique<ErrInfo>("second", ERR_MEMORY_ALLOCATION_ERROR);
    EXPECT_STREQ("second", stmt_->err->error_msg);
    EXPECT_STREQ("HY001", stmt_->err->sqlstate);
}

TEST_F(ErrorHandlingTest, UniquePtr_ResetCleansUp) {
    stmt_->err = std::make_unique<ErrInfo>("test", ERR_GENERAL_ERROR);
    stmt_->err.reset();
    EXPECT_FALSE(stmt_->err);
}

// SQLGetDiagRec returns correct messages

TEST_F(ErrorHandlingTest, DiagRec_ReturnsErrMessage) {
    dbc_->err = std::make_unique<ErrInfo>("Connection failed", ERR_COMMUNICATION_LINK_FAILURE);

    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = {0};
    SQLINTEGER native_error = 0;
    SQLTCHAR message[MAX_MSG_LEN] = {0};
    SQLSMALLINT text_length = 0;

    SQLRETURN ret = RDS_SQLGetDiagRec(
        SQL_HANDLE_DBC, dbc_, 1,
        sql_state, &native_error, message, MAX_MSG_LEN, &text_length, false);

    EXPECT_EQ(SQL_SUCCESS, ret);
#ifndef UNICODE
    EXPECT_STREQ("08S01", reinterpret_cast<const char*>(sql_state));
    EXPECT_STREQ("Connection failed", reinterpret_cast<const char*>(message));
#endif
    EXPECT_EQ(static_cast<SQLINTEGER>(ERR_COMMUNICATION_LINK_FAILURE), native_error);
}

TEST_F(ErrorHandlingTest, DiagRec_RecordTwoReturnsNoData) {
    dbc_->err = std::make_unique<ErrInfo>("test", ERR_GENERAL_ERROR);

    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = {0};
    SQLINTEGER native_error = 0;
    SQLTCHAR message[MAX_MSG_LEN] = {0};
    SQLSMALLINT text_length = 0;

    SQLRETURN ret = RDS_SQLGetDiagRec(
        SQL_HANDLE_DBC, dbc_, 2,
        sql_state, &native_error, message, MAX_MSG_LEN, &text_length, false);

    EXPECT_EQ(SQL_NO_DATA, ret);
}

TEST_F(ErrorHandlingTest, InitializeConnection_UnloadableDriverReturnsErrorDiagnostic) {
    dbc_->conn_attr[KEY_BASE_DRIVER] = "/nonexistent/path/no-such-driver.so";

    EXPECT_EQ(SQL_ERROR, RDS_InitializeConnection(dbc_, ""));

    ASSERT_TRUE(dbc_->err);
    EXPECT_STREQ("IM003", dbc_->err->sqlstate);
    const std::string message(dbc_->err->error_msg);
    EXPECT_NE(std::string::npos, message.find("/nonexistent/path/no-such-driver.so"));
    EXPECT_FALSE(env_->driver_lib_loader);
}

TEST_F(ErrorHandlingTest, InitializeConnection_UnregisteredDriverNameReturnsErrorDiagnostic) {
    dbc_->conn_attr[KEY_BASE_DRIVER] = "DefinitelyNotARegisteredOdbcDriver123";

    EXPECT_EQ(SQL_ERROR, RDS_InitializeConnection(dbc_, ""));

    ASSERT_TRUE(dbc_->err);
    EXPECT_STREQ("IM003", dbc_->err->sqlstate);
    const std::string message(dbc_->err->error_msg);
    EXPECT_NE(std::string::npos, message.find("DefinitelyNotARegisteredOdbcDriver123"));
    EXPECT_FALSE(env_->driver_lib_loader);
}

TEST_F(ErrorHandlingTest, InitializeConnection_NoDriverKeepsExistingDiagnostic) {
    EXPECT_EQ(SQL_ERROR, RDS_InitializeConnection(dbc_, ""));

    ASSERT_TRUE(dbc_->err);
    EXPECT_EQ(static_cast<SQLINTEGER>(ERR_NO_UNDER_LYING_DRIVER), dbc_->err->native_err);
}

TEST_F(ErrorHandlingTest, DiagRec_InvalidHandleType) {
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = {0};
    SQLINTEGER native_error = 0;
    SQLTCHAR message[MAX_MSG_LEN] = {0};
    SQLSMALLINT text_length = 0;
    
    const SQLSMALLINT invalid_handle_type = 999;
    SQLRETURN ret = RDS_SQLGetDiagRec(invalid_handle_type, dbc_, 1, sql_state, &native_error, message, MAX_MSG_LEN, &text_length, false);

    EXPECT_EQ(SQL_INVALID_HANDLE, ret);
}
