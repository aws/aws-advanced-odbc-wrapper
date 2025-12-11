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

// codechecker_suppress [readability-convert-member-functions-to-static]
void OdbcHelper::Disconnect(const DBC* &dbc, const std::shared_ptr<RdsLibLoader> &lib_loader) {
    if (dbc->wrapped_dbc) {
        try {
            NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
            dbc->wrapped_dbc
            );
        } catch (const std::exception& ex) {
            LOG(ERROR) << "Exception while disconnecting: " << ex.what();
        }
    }
}

void OdbcHelper::Disconnect(const SQLHDBC hdbc, const std::shared_ptr<RdsLibLoader> &lib_loader) {
    const DBC* local_dbc = static_cast<DBC*>(hdbc);
    Disconnect(local_dbc, lib_loader);
}

void OdbcHelper::DisconnectAndFree(const SQLHDBC &hdbc, const std::shared_ptr<RdsLibLoader> &lib_loader) {
    Disconnect(hdbc, lib_loader);
    RDS_FreeConnect(hdbc);
}

RdsLibResult OdbcHelper::AllocEnv(SQLHENV &henv, ENV* &env, const std::shared_ptr<RdsLibLoader> &lib_loader) {
    RDS_AllocEnv(&henv);
    env = static_cast<ENV*>(henv);
    return NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_ENV, nullptr, &env->wrapped_env
    );
}

void OdbcHelper::FreeEnv(const SQLHENV &henv) {
    RDS_FreeEnv(henv);
}

RdsLibResult OdbcHelper::AllocStmt(const SQLHDBC &wrapped_dbc, const std::shared_ptr<RdsLibLoader> &lib_loader, SQLHSTMT &stmt) {
    return NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, wrapped_dbc, &stmt
    );
}

RdsLibResult OdbcHelper::FreeStmt(const std::shared_ptr<RdsLibLoader> &lib_loader, SQLHSTMT &stmt) {
    return NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
        SQL_HANDLE_STMT, stmt
    );
}

DBC* OdbcHelper::AllocDbc(SQLHENV &henv, SQLHDBC &hdbc) {
    RDS_AllocDbc(henv, &hdbc);
    DBC* local_dbc = static_cast<DBC*>(hdbc);
    return local_dbc;
}

void OdbcHelper::SQLSetEnvAttr(
    const std::shared_ptr<RdsLibLoader> &lib_loader,
    const ENV* henv,
    const SQLINTEGER attribute,
    SQLPOINTER pointer,
    const int length)
{
    NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
        henv->wrapped_env, attribute, pointer, length
    );
}

RdsLibResult OdbcHelper::SQLFetch(const std::shared_ptr<RdsLibLoader> &lib_loader, SQLHSTMT &stmt)
{
    return NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLFetch, RDS_STR_SQLFetch,
        stmt
    );
}

RdsLibResult OdbcHelper::SQLBindCol(
    const std::shared_ptr<RdsLibLoader> &lib_loader,
    const SQLHSTMT &stmt,
    const int column,
    const int type,
    void* value,
    size_t size,
    SQLLEN len)
{
    return NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
        stmt, column, type, value, size, &len
    );
}

RdsLibResult OdbcHelper::SQLExecDirect(const std::shared_ptr<RdsLibLoader> &lib_loader, const SQLHSTMT &stmt, const std::string &query) {
    return NULL_CHECK_CALL_LIB_FUNC(lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
        stmt, AS_SQLTCHAR(query), SQL_NTS
    );
}
