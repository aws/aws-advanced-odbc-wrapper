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

#include<iostream>
#include "rds_lib_loader.h"
#include "rds_strings.h"

OdbcHelper::OdbcHelper(const std::shared_ptr<RdsLibLoader> &lib_loader) {
    this->lib_loader_ = lib_loader;
}

void OdbcHelper::Disconnect(DBC* dbc) {
    if (dbc != nullptr && dbc->wrapped_dbc != nullptr) {
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
    DBC* local_dbc = static_cast<DBC*>(*hdbc);
    Disconnect(local_dbc);
}

void OdbcHelper::DisconnectAndFree(SQLHDBC* hdbc, bool keep_dbc) {
    Disconnect(hdbc);
    RDS_FreeConnect(*hdbc, keep_dbc);
}

void OdbcHelper::DisconnectAndFreeDBC(DBC* dbc, bool keep_dbc) {
    Disconnect(dbc);
    RDS_FreeConnect(dbc, keep_dbc);
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

RdsLibResult OdbcHelper::SetEnvAttr(
    const ENV *henv,
    const SQLINTEGER attribute,
    SQLPOINTER pointer,
    const int length)
{
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
        henv->wrapped_env, attribute, pointer, length
    );
}

RdsLibResult OdbcHelper::Fetch(SQLHSTMT* stmt)
{
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLFetch, RDS_STR_SQLFetch,
        *stmt
    );
}

RdsLibResult OdbcHelper::BindCol(
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

RdsLibResult OdbcHelper::ExecDirect(const SQLHSTMT* stmt, const std::string &query) {
#if UNICODE
    if (this->GetUse4BytesBaseDriver()) {
        const std::wstring wide_conn(query.begin(), query.end());
        SQLTCHAR* conn_in_sqltchar = const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(wide_conn.c_str()));
        return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
            *stmt, conn_in_sqltchar, SQL_NTS
        );
    }
#endif
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

std::shared_ptr<RdsLibLoader> OdbcHelper::GetLibLoader() {
    return this->lib_loader_;
}

bool OdbcHelper::GetUse4BytesBaseDriver() const {
    return this->use_4_bytes_base_driver_;
}

bool OdbcHelper::GetUse4BytesUserApp() const {
    return this->use_4_bytes_user_app_;
}

void OdbcHelper::SetUse4BytesBaseDriver(const bool use_4_bytes) {
    this->use_4_bytes_base_driver_ = use_4_bytes;
}

void OdbcHelper::SetUse4BytesUserApp(const bool use_4_bytes) {
    this->use_4_bytes_user_app_ = use_4_bytes;
}

std::string OdbcHelper::GetSqlStateAndLogMessage(DBC* dbc) {
    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = { 0 };
    SQLTCHAR message[MAX_MSG_LEN] = { 0 };
    RDS_SQLError(nullptr, static_cast<SQLHDBC>(dbc), nullptr, sql_state, &native_error, message, MAX_MSG_LEN, &stmt_length, true);
    LOG(WARNING) << "SQL State: " << AS_UTF8_CSTR(sql_state) << ". Message: " << AS_UTF8_CSTR(message);
    return AS_UTF8_CSTR(sql_state);
}
