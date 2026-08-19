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

// Regression tests: NULL/dangling wrapped handles must never reach the
// underlying driver (no driver manager validates them in between).

#include <gtest/gtest.h>

#include "../../driver/driver.h"
#include "../../driver/odbcapi.h"
#include "../../driver/odbcapi_rds_helper.h"
#include "../../driver/util/odbc_helper.h"

class NullWrappedHandleTest : public testing::Test {
protected:
    ENV env_;
    DBC* dbc_ = nullptr;
    STMT* stmt_ = nullptr;

    void SetUp() override {
        dbc_ = new DBC();
        dbc_->env = &env_;
        env_.dbc_list.push_back(dbc_);

        // Simulate a statement invalidated by a failed failover.
        stmt_ = new STMT();
        stmt_->dbc = dbc_;
        stmt_->wrapped_stmt = SQL_NULL_HSTMT;
        stmt_->app_row_desc = new DESC();
        stmt_->app_row_desc->dbc = dbc_;
        stmt_->app_param_desc = new DESC();
        stmt_->app_param_desc->dbc = dbc_;
        stmt_->imp_row_desc = new DESC();
        stmt_->imp_row_desc->dbc = dbc_;
        stmt_->imp_param_desc = new DESC();
        stmt_->imp_param_desc->dbc = dbc_;
        dbc_->stmt_list.push_back(stmt_);
    }

    void TearDown() override {
        delete stmt_->app_row_desc;
        delete stmt_->app_param_desc;
        delete stmt_->imp_row_desc;
        delete stmt_->imp_param_desc;
        delete stmt_;
        delete dbc_;
        env_.dbc_list.clear();
    }
};

// SQL_ATTR_*_DESC lookups previously forwarded wrapped_stmt unguarded.
TEST_F(NullWrappedHandleTest, GetStmtAttrDescOnNulledStmtReturnsInvalidHandle) {
    SQLPOINTER value = nullptr;
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt_, SQL_ATTR_APP_ROW_DESC, &value, SQL_IS_POINTER, nullptr));
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt_, SQL_ATTR_APP_PARAM_DESC, &value, SQL_IS_POINTER, nullptr));
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt_, SQL_ATTR_IMP_ROW_DESC, &value, SQL_IS_POINTER, nullptr));
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt_, SQL_ATTR_IMP_PARAM_DESC, &value, SQL_IS_POINTER, nullptr));
}

TEST_F(NullWrappedHandleTest, GetStmtAttrNullHandleReturnsInvalidHandle) {
    SQLPOINTER value = nullptr;
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(nullptr, SQL_ATTR_APP_ROW_DESC, &value, SQL_IS_POINTER, nullptr));
}

// Disconnect previously passed already-nulled wrapped handles to SQLFreeHandle.
TEST_F(NullWrappedHandleTest, DisconnectWithNulledWrappedHandlesDoesNotCrash) {
    DESC* desc = new DESC();
    desc->dbc = dbc_;
    desc->wrapped_desc = SQL_NULL_HDESC;
    dbc_->desc_list.push_back(desc);

    OdbcHelper helper(nullptr, &env_);
    helper.Disconnect(dbc_);

    EXPECT_EQ(SQL_NULL_HSTMT, stmt_->wrapped_stmt);
    EXPECT_EQ(SQL_NULL_HDESC, desc->wrapped_desc);

    dbc_->desc_list.clear();
    delete desc;
}

// Implicit descriptors are not in dbc->desc_list; invalidation must null them.
TEST_F(NullWrappedHandleTest, InvalidateImplicitDescriptorsNullsAllFour) {
    const int fake = 1;
    stmt_->app_row_desc->wrapped_desc = const_cast<int*>(&fake);
    stmt_->app_param_desc->wrapped_desc = const_cast<int*>(&fake);
    stmt_->imp_row_desc->wrapped_desc = const_cast<int*>(&fake);
    stmt_->imp_param_desc->wrapped_desc = const_cast<int*>(&fake);

    OdbcHelper::InvalidateImplicitDescriptors(stmt_);

    EXPECT_EQ(SQL_NULL_HDESC, stmt_->app_row_desc->wrapped_desc);
    EXPECT_EQ(SQL_NULL_HDESC, stmt_->app_param_desc->wrapped_desc);
    EXPECT_EQ(SQL_NULL_HDESC, stmt_->imp_row_desc->wrapped_desc);
    EXPECT_EQ(SQL_NULL_HDESC, stmt_->imp_param_desc->wrapped_desc);
}

// SQLCopyDesc previously forwarded a missing/invalid target's NULL handle.
TEST_F(NullWrappedHandleTest, CopyDescToInvalidTargetReturnsInvalidHandle) {
    const int fake = 1;
    DESC src;
    src.dbc = dbc_;
    src.wrapped_desc = const_cast<int*>(&fake);

    DESC dst;
    dst.dbc = dbc_;
    dst.wrapped_desc = SQL_NULL_HDESC;

    EXPECT_EQ(SQL_INVALID_HANDLE, SQLCopyDesc(&src, &dst));
    EXPECT_EQ(SQL_INVALID_HANDLE, SQLCopyDesc(&src, nullptr));

    src.wrapped_desc = SQL_NULL_HDESC;
}
