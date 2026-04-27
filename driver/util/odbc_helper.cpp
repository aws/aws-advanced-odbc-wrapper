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

OdbcHelper::OdbcHelper(const std::shared_ptr<RdsLibLoader> &lib_loader, const ENV* env) {
    this->lib_loader_ = lib_loader;
    this->env_ = env;
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
    const std::vector<uint16_t> query_vector = ConvertUTF8ToUTF16(query);
    SQLTCHAR* query_sqltchar = const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(query_vector.data()));
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
        *stmt, query_sqltchar, SQL_NTS
    );
#else
    return NULL_CHECK_CALL_LIB_FUNC(this->lib_loader_ , RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
        *stmt, AS_SQLTCHAR(query), SQL_NTS
    );
#endif
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
    return this->env_ != nullptr && this->env_->use_4_bytes_user_app.load();
}

void OdbcHelper::SetUse4BytesBaseDriver(const bool use_4_bytes) {
    this->use_4_bytes_base_driver_ = use_4_bytes;
}

// codechecker_suppress [readability-convert-member-functions-to-static]
ConvertedSqltchar OdbcHelper::ConvertInput(SQLTCHAR* in, SQLINTEGER in_length) const {
    ConvertedSqltchar result;
#if UNICODE
    if (!in) {
        // Return as-is if this is a nullptr, let the caller decide how to handle nullptr.
        return result;
    }
    result.data = ConvertUserAppInputToBaseDriver(
        GetUse4BytesUserApp(),
        use_4_bytes_base_driver_,
        in,
        in_length);
    result.tchar_ptr = result.data.data();
#else
    result.tchar_ptr = in;
#endif
    return result;
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
#if UNICODE
    const std::string state_str = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(sql_state));
    const std::string message_str = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(message));
#else
    const std::string state_str = reinterpret_cast<const char*>(sql_state);
    const std::string message_str = reinterpret_cast<const char*>(message);
#endif
    LOG(WARNING) << "SQL State: " << state_str << ". Message: " << message_str;
    out_message = message_str;
    return state_str;
}

std::string OdbcHelper::GetStmtErrorMessage(SQLHSTMT stmt) {
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = { 0 };
    SQLTCHAR message[MAX_MSG_LEN] = { 0 };
    SQLINTEGER native_error = 0;
    SQLSMALLINT text_length = 0;
    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(lib_loader_, RDS_FP_SQLGetDiagRec, RDS_STR_SQLGetDiagRec,
        SQL_HANDLE_STMT, stmt, 1, sql_state, &native_error, message, MAX_MSG_LEN, &text_length);
    if (SQL_SUCCEEDED(res.fn_result)) {
#if UNICODE
        return ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(message));
#else
        return {reinterpret_cast<const char*>(message)};
#endif
    }
    return "";
}

// Convert base driver output to either 2-byte or 4-byte output for the client application
void OdbcHelper::ConvertDriverOutputToTarget(
    const SQLTCHAR* src,
    SQLTCHAR* dst,
    const size_t src_byte_count,
    const size_t dst_byte_count) const
{
    ConvertDriverOutputToTarget(false, src, dst, src_byte_count, dst_byte_count);
}

// codechecker_suppress [readability-convert-member-functions-to-static]
void OdbcHelper::ConvertDriverOutputToTarget(
    const bool wrapper_call,
    const SQLTCHAR* src,
    SQLTCHAR* dst,
    const size_t src_byte_count,
    const size_t dst_byte_count) const
{
    if (dst == nullptr || src == nullptr || dst_byte_count == 0) {
        return;
    }
#if UNICODE
    const bool user_4_byte = !wrapper_call && GetUse4BytesUserApp();
    const size_t src_char_count = use_4_bytes_base_driver_
        ? src_byte_count / sizeof(SQLTCHAR) / 2
        : src_byte_count / sizeof(SQLTCHAR);
    const size_t dst_char_count = user_4_byte
        ? dst_byte_count / sizeof(SQLTCHAR) / 2
        : dst_byte_count / sizeof(SQLTCHAR);

    if (user_4_byte == use_4_bytes_base_driver_) {
        // Both the client application and the base driver use 4-byte data.
        // Copy the data as-is.
        const size_t max_copy = src_byte_count > dst_byte_count
            ? dst_byte_count
            : src_byte_count;
        std::memcpy(dst, src, max_copy);
    } else if (user_4_byte) {
        // Expand the driver output from 2-byte to 4-byte for the client application.
        ConvertUTF16ToUTF32(src, dst, src_char_count, dst_char_count * 2);
    } else {
        // Narrow driver output from 4-byte to 2-byte for the client application.
        // Or copy as-is if both are 2-byte.
        Convert4To2ByteString(use_4_bytes_base_driver_, const_cast<SQLTCHAR*>(src), dst, dst_char_count);
    }
#else
    const size_t max_copy = src_byte_count > dst_byte_count
        ? dst_byte_count
        : src_byte_count;
    std::memcpy(dst, src, max_copy);
#endif
}

// Convert wrapper output to either 2-byte or 4-byte output for the client application
void OdbcHelper::ConvertWrapperOutputToTarget(
    SQLTCHAR* buf,
    const size_t char_count,
    const size_t buf_byte_count) const
{
    ConvertWrapperOutputToTarget(false, buf, char_count, buf_byte_count);
}

void OdbcHelper::ConvertWrapperOutputToTarget(
    const bool wrapper_call,
    SQLTCHAR* buf,
    const size_t char_count,
    const size_t buf_byte_count) const
{
    ConvertWrapperOutputToTarget(GetUse4BytesUserApp(), wrapper_call, buf, char_count, buf_byte_count);
}

// codechecker_suppress [readability-convert-member-functions-to-static]
void OdbcHelper::ConvertWrapperOutputToTarget(
    const bool user_4_byte,
    const bool wrapper_call,
    SQLTCHAR* buf,
    const size_t char_count,
    const size_t buf_byte_count)
{
#if UNICODE
    if (!wrapper_call && user_4_byte) {
        const size_t buf_char_count = buf_byte_count / sizeof(SQLTCHAR);
        ExpandUTF16ToUTF32InPlace(buf, char_count, buf_char_count);
    }
#endif
}

// codechecker_suppress [readability-convert-member-functions-to-static]
bool OdbcHelper::NeedsConversion() const {
#if UNICODE
    return GetUse4BytesUserApp() != use_4_bytes_base_driver_;
#else
    return false;
#endif
}

std::vector<SQLTCHAR> OdbcHelper::AllocateConversionBuffer(const size_t byte_count) const {
    const size_t char_count = byte_count / sizeof(SQLTCHAR);
    const size_t local_buf_size = use_4_bytes_base_driver_
        ? char_count * 2
        : char_count;
    return std::vector<SQLTCHAR>(local_buf_size, 0);
}

size_t OdbcHelper::GetTargetByteCount(const bool wrapper_call, const size_t char_count) const {
    const size_t multiplier = !wrapper_call && GetUse4BytesUserApp() ? 2 : 1;
    return char_count * multiplier * sizeof(SQLTCHAR);
}

bool OdbcHelper::IsStringConnectAttr(const SQLINTEGER attribute) {
    switch (attribute) {
        case SQL_ATTR_CURRENT_CATALOG:
        case SQL_ATTR_TRACEFILE:
        case SQL_ATTR_TRANSLATE_LIB:
            return true;
        default:
            return false;
    }
}

bool OdbcHelper::IsStringDescField(const SQLSMALLINT field_identifier) {
    switch (field_identifier) {
        case SQL_DESC_BASE_COLUMN_NAME:
        case SQL_DESC_BASE_TABLE_NAME:
        case SQL_DESC_CATALOG_NAME:
        case SQL_DESC_LABEL:
        case SQL_DESC_LITERAL_PREFIX:
        case SQL_DESC_LITERAL_SUFFIX:
        case SQL_DESC_LOCAL_TYPE_NAME:
        case SQL_DESC_NAME:
        case SQL_DESC_SCHEMA_NAME:
        case SQL_DESC_TABLE_NAME:
        case SQL_DESC_TYPE_NAME:
            return true;
        default:
            return false;
    }
}

bool OdbcHelper::IsStringDiagField(const SQLSMALLINT diag_identifier) {
    switch (diag_identifier) {
        case SQL_DIAG_CLASS_ORIGIN:
        case SQL_DIAG_CONNECTION_NAME:
        case SQL_DIAG_DYNAMIC_FUNCTION:
        case SQL_DIAG_MESSAGE_TEXT:
        case SQL_DIAG_SERVER_NAME:
        case SQL_DIAG_SQLSTATE:
        case SQL_DIAG_SUBCLASS_ORIGIN:
            return true;
        default:
            return false;
    }
}

void OdbcHelper::AdjustByteLength(SQLSMALLINT* length_ptr) const {
#ifdef UNICODE
    if (length_ptr && *length_ptr > 0) {
        const bool user_4_byte = GetUse4BytesUserApp();
        // Expanded, driver 2-byte -> user 4-byte
        if (user_4_byte && !use_4_bytes_base_driver_) {
            *length_ptr *= 2;
        }
        // Shrunk, driver 4-byte -> user 2-byte
        if (!user_4_byte && use_4_bytes_base_driver_) {
            *length_ptr /= 2;
        }
    }
#endif
}

void OdbcHelper::AdjustByteLength(SQLINTEGER* length_ptr) const {
    #ifdef UNICODE
        if (length_ptr && *length_ptr > 0) {
            const bool user_4_byte = GetUse4BytesUserApp();
            // Expanded, driver 2-byte -> user 4-byte
            if (user_4_byte && !use_4_bytes_base_driver_) {
                *length_ptr *= 2;
            }
            // Shrunk, driver 4-byte -> user 2-byte
            if (!user_4_byte && use_4_bytes_base_driver_) {
                *length_ptr /= 2;
            }
        }
    #endif
}
