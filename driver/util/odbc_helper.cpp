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

#include "odbc_helper.h"

#include "../odbcapi_rds_helper.h"

OdbcHelper::OdbcHelper(const std::shared_ptr<RdsLibLoader> &lib_loader) {
    this->lib_loader_ = lib_loader;
}

void OdbcHelper::Disconnect(const DBC* dbc) {
    if (dbc && dbc->wrapped_dbc) {
        try {
            NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
                dbc->wrapped_dbc
            );
        } catch (const std::exception& ex) {
            LOG(ERROR) << "Exception while disconnecting: " << ex.what();
        }
    }
}

void OdbcHelper::Disconnect(SQLHDBC* hdbc) {
    const DBC* local_dbc = static_cast<DBC*>(*hdbc);
    Disconnect(local_dbc);
}

void OdbcHelper::DisconnectAndFree(SQLHDBC* hdbc) {
    Disconnect(hdbc);
    RDS_FreeConnect(*hdbc);
}

SQLRETURN OdbcHelper::AllocEnv(SQLHENV* henv) {
    return RDS_AllocEnv(henv);
}

// codechecker_suppress [readability-convert-member-functions-to-static]
SQLRETURN OdbcHelper::FreeEnv(SQLHENV* henv) {
    return RDS_FreeEnv(*henv);
}

// codechecker_suppress [readability-convert-member-functions-to-static]
SQLRETURN OdbcHelper::AllocDbc(SQLHENV &henv, SQLHDBC &hdbc) {
    return RDS_AllocDbc(henv, &hdbc);
}

RdsLibResult OdbcHelper::SQLSetEnvAttr(
    const ENV *henv,
    const SQLINTEGER attribute,
    SQLPOINTER pointer,
    const int length)
{
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
        henv->wrapped_env, attribute, pointer, length
    );
}

RdsLibResult OdbcHelper::SQLFetch(SQLHSTMT* stmt)
{
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLFetch, RDS_STR_SQLFetch,
        *stmt
    );
}

RdsLibResult OdbcHelper::SQLBindCol(
    const SQLHSTMT* stmt,
    const int column,
    const int type,
    void* value,
    const size_t size,
    SQLLEN* len)
{
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
        *stmt, column, type, value, size, len
    );
}

RdsLibResult OdbcHelper::SQLExecDirect(const SQLHSTMT* stmt, const std::string &query) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
        *stmt, AS_SQLTCHAR(query), SQL_NTS
    );
}

RdsLibResult OdbcHelper::BaseAllocEnv(ENV* env) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_ENV, nullptr, &env->wrapped_env
    );
}

RdsLibResult OdbcHelper::BaseAllocStmt(const SQLHDBC* wrapped_dbc, SQLHSTMT* stmt) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, *wrapped_dbc, stmt
    );
}

RdsLibResult OdbcHelper::BaseFreeStmt(SQLHSTMT* stmt) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
        SQL_HANDLE_STMT, *stmt
    );
}
