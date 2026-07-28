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
    ENV env;
    DBC* dbc = nullptr;
    STMT* stmt = nullptr;

    void SetUp() override {
        dbc = new DBC();
        dbc->env = &env;
        env.dbc_list.push_back(dbc);

        // Simulate a statement invalidated by a failed failover.
        stmt = new STMT();
        stmt->dbc = dbc;
        stmt->wrapped_stmt = SQL_NULL_HSTMT;
        stmt->app_row_desc = new DESC();
        stmt->app_row_desc->dbc = dbc;
        stmt->app_param_desc = new DESC();
        stmt->app_param_desc->dbc = dbc;
        stmt->imp_row_desc = new DESC();
        stmt->imp_row_desc->dbc = dbc;
        stmt->imp_param_desc = new DESC();
        stmt->imp_param_desc->dbc = dbc;
        dbc->stmt_list.push_back(stmt);
    }

    void TearDown() override {
        delete stmt->app_row_desc;
        delete stmt->app_param_desc;
        delete stmt->imp_row_desc;
        delete stmt->imp_param_desc;
        delete stmt;
        delete dbc;
        env.dbc_list.clear();
    }
};

// SQL_ATTR_*_DESC lookups previously forwarded wrapped_stmt unguarded.
TEST_F(NullWrappedHandleTest, GetStmtAttrDescOnNulledStmtReturnsInvalidHandle) {
    SQLPOINTER value = nullptr;
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &value, SQL_IS_POINTER, nullptr));
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt, SQL_ATTR_APP_PARAM_DESC, &value, SQL_IS_POINTER, nullptr));
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt, SQL_ATTR_IMP_ROW_DESC, &value, SQL_IS_POINTER, nullptr));
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(stmt, SQL_ATTR_IMP_PARAM_DESC, &value, SQL_IS_POINTER, nullptr));
}

TEST_F(NullWrappedHandleTest, GetStmtAttrNullHandleReturnsInvalidHandle) {
    SQLPOINTER value = nullptr;
    EXPECT_EQ(SQL_INVALID_HANDLE, RDS_SQLGetStmtAttr(nullptr, SQL_ATTR_APP_ROW_DESC, &value, SQL_IS_POINTER, nullptr));
}

// Disconnect previously passed already-nulled wrapped handles to SQLFreeHandle.
TEST_F(NullWrappedHandleTest, DisconnectWithNulledWrappedHandlesDoesNotCrash) {
    DESC* desc = new DESC();
    desc->dbc = dbc;
    desc->wrapped_desc = SQL_NULL_HDESC;
    dbc->desc_list.push_back(desc);

    OdbcHelper helper(nullptr, &env);
    helper.Disconnect(dbc);

    EXPECT_EQ(SQL_NULL_HSTMT, stmt->wrapped_stmt);
    EXPECT_EQ(SQL_NULL_HDESC, desc->wrapped_desc);

    dbc->desc_list.clear();
    delete desc;
}

// Implicit descriptors are not in dbc->desc_list; invalidation must null them.
TEST_F(NullWrappedHandleTest, InvalidateImplicitDescriptorsNullsAllFour) {
    const int fake = 1;
    stmt->app_row_desc->wrapped_desc = const_cast<int*>(&fake);
    stmt->app_param_desc->wrapped_desc = const_cast<int*>(&fake);
    stmt->imp_row_desc->wrapped_desc = const_cast<int*>(&fake);
    stmt->imp_param_desc->wrapped_desc = const_cast<int*>(&fake);

    OdbcHelper::InvalidateImplicitDescriptors(stmt);

    EXPECT_EQ(SQL_NULL_HDESC, stmt->app_row_desc->wrapped_desc);
    EXPECT_EQ(SQL_NULL_HDESC, stmt->app_param_desc->wrapped_desc);
    EXPECT_EQ(SQL_NULL_HDESC, stmt->imp_row_desc->wrapped_desc);
    EXPECT_EQ(SQL_NULL_HDESC, stmt->imp_param_desc->wrapped_desc);
}

// SQLCopyDesc previously forwarded a missing/invalid target's NULL handle.
TEST_F(NullWrappedHandleTest, CopyDescToInvalidTargetReturnsInvalidHandle) {
    const int fake = 1;
    DESC src;
    src.dbc = dbc;
    src.wrapped_desc = const_cast<int*>(&fake);

    DESC dst;
    dst.dbc = dbc;
    dst.wrapped_desc = SQL_NULL_HDESC;

    EXPECT_EQ(SQL_INVALID_HANDLE, SQLCopyDesc(&src, &dst));
    EXPECT_EQ(SQL_INVALID_HANDLE, SQLCopyDesc(&src, nullptr));

    src.wrapped_desc = SQL_NULL_HDESC;
}
