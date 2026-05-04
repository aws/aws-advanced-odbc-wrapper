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

#include "rds_lib_loader.h"
#include "rds_strings.h"

#include <mutex>

OdbcHelper::OdbcHelper(const std::shared_ptr<RdsLibLoader> &lib_loader) {
    this->lib_loader_ = lib_loader;
}

void OdbcHelper::Disconnect(DBC* dbc) {
    if (dbc) {
        const std::lock_guard<std::recursive_mutex> lock_guard(dbc->lock);
        // Cleanup tracked underlying statements
        const std::list<STMT*> stmt_list = dbc->stmt_list;
        for (STMT* stmt : stmt_list) {
            const std::lock_guard<std::recursive_mutex> stmt_lock(stmt->lock);
            try {
                NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
                    SQL_HANDLE_STMT, stmt->wrapped_stmt
                );
                stmt->wrapped_stmt = SQL_NULL_HSTMT;
            } catch (const std::exception& ex) {
                LOG(ERROR) << "Exception while cleaning up statements for disconnects: " << ex.what();
            }
        }
        // and underlying descriptors
        const std::list<DESC*> desc_list = dbc->desc_list;
        for (DESC* desc : desc_list) {
            const std::lock_guard<std::recursive_mutex> desc_lock(desc->lock);
            try {
                NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
                    SQL_HANDLE_DESC, desc->wrapped_desc
                );
                desc->wrapped_desc = SQL_NULL_HDESC;
            } catch (const std::exception& ex) {
                LOG(ERROR) << "Exception while cleaning up descriptors for disconnects: " << ex.what();
            }
        }
        if (dbc->wrapped_dbc) {
            try {
                NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
                    dbc->wrapped_dbc
                );
                dbc->wrapped_dbc = SQL_NULL_HDBC;
            } catch (const std::exception& ex) {
                LOG(ERROR) << "Exception while disconnecting: " << ex.what();
            }
        }
    }
}

void OdbcHelper::Disconnect(SQLHDBC* hdbc) {
    DBC* local_dbc = static_cast<DBC*>(*hdbc);
    Disconnect(local_dbc);
}

void OdbcHelper::DisconnectAndFree(SQLHDBC* hdbc) {
    Disconnect(hdbc);
    RDS_FreeConnect(*hdbc);
    *hdbc = SQL_NULL_HDBC;
}

bool OdbcHelper::IsClosed(SQLHDBC hdbc) {
    const DBC* local_dbc = static_cast<DBC*>(hdbc);
    if (hdbc == SQL_NULL_HDBC || local_dbc->wrapped_dbc == SQL_NULL_HDBC) {
        return true;
    }
    SQLUINTEGER connection_state = SQL_CD_FALSE;
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLGetConnectAttr, RDS_STR_SQLGetConnectAttr,
        local_dbc->wrapped_dbc, SQL_ATTR_CONNECTION_DEAD, &connection_state, sizeof(SQLUINTEGER), nullptr
    );

    if (SQL_SUCCEEDED(res.fn_result)) {
        return connection_state == SQL_CD_TRUE;
    }

    return true;
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
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLSetEnvAttr, RDS_STR_SQLSetEnvAttr,
        henv->wrapped_env, attribute, pointer, length
    );
}

RdsLibResult OdbcHelper::Fetch(SQLHSTMT* stmt)
{
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLFetch, RDS_STR_SQLFetch,
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
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
        *stmt, column, type, value, size, len
    );
}

RdsLibResult OdbcHelper::ExecDirect(const SQLHSTMT* stmt, const std::string &query) {
#if UNICODE
    if (this->GetUse4BytesBaseDriver()) {
        const std::wstring wide_conn = ConvertUTF8ToWString(query);
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

RdsLibResult OdbcHelper::CloseCursor(SQLHSTMT stmt) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLCloseCursor, RDS_STR_SQLCloseCursor,
        stmt
    );
}

RdsLibResult OdbcHelper::BaseAllocEnv(ENV* env) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_ENV, nullptr, &env->wrapped_env
    );
}

RdsLibResult OdbcHelper::BaseAllocStmt(const SQLHDBC* wrapped_dbc, SQLHSTMT* stmt) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, *wrapped_dbc, stmt
    );
}

RdsLibResult OdbcHelper::BaseFreeStmt(SQLHSTMT* stmt) {
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
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
    std::string unused;
    return GetSqlStateAndLogMessage(dbc, unused);
}

std::string OdbcHelper::GetSqlStateAndLogMessage(DBC* dbc, std::string& out_message) {
    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = { 0 };
    SQLTCHAR message[MAX_MSG_LEN] = { 0 };
    // Use RDS_SQLGetDiagRec directly instead of RDS_SQLError to avoid consuming the
    // sql_error_called flag. RDS_SQLError sets this flag, which prevents subsequent
    // external SQLError calls from retrieving the diagnostic record.
    RDS_SQLGetDiagRec(SQL_HANDLE_DBC, static_cast<SQLHDBC>(dbc), 1, sql_state, &native_error, message, MAX_MSG_LEN, &stmt_length, true);
    LOG(WARNING) << "SQL State: " << AS_UTF8_CSTR(sql_state) << ". Message: " << AS_UTF8_CSTR(message);
    out_message = AS_UTF8_CSTR(message);
    return AS_UTF8_CSTR(sql_state);
}
std::string OdbcHelper::GetStmtErrorMessage(SQLHSTMT stmt) {
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = { 0 };
    SQLTCHAR message[MAX_MSG_LEN] = { 0 };
    SQLINTEGER native_error = 0;
    SQLSMALLINT text_length = 0;
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(lib_loader_, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
        SQL_HANDLE_STMT, stmt, 1, sql_state, &native_error, message, MAX_MSG_LEN, &text_length);
    if (SQL_SUCCEEDED(res.fn_result)) {
        return AS_UTF8_CSTR(message);
    }
    return "";
}
