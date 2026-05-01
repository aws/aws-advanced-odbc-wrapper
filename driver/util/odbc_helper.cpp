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

// codechecker_suppress [readability-convert-member-functions-to-static]
ConvertedSqltchar OdbcHelper::ConvertInput(SQLTCHAR* in, SQLINTEGER in_length) const {
    ConvertedSqltchar result;
#if UNICODE
    if (!in) {
        // Return as-is if this is a nullptr, let the caller decide how to handle nullptr.
        return result;
    }
    result.data = ConvertUserAppInputToBaseDriver(
        use_4_bytes_user_app_,
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
    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;
    SQLTCHAR sql_state[MAX_SQL_STATE_LEN] = { 0 };
    SQLTCHAR message[MAX_MSG_LEN] = { 0 };
    RDS_SQLError(nullptr, static_cast<SQLHDBC>(dbc), nullptr, sql_state, &native_error, message, MAX_MSG_LEN, &stmt_length, true);
    LOG(WARNING) << "SQL State: " << AS_UTF8_CSTR(sql_state) << ". Message: " << AS_UTF8_CSTR(message);
    return AS_UTF8_CSTR(sql_state);
}

// Convert base driver output to either 2-byte or 4-byte output for the client application
void OdbcHelper::ConvertDriverOutputToTarget(
    const SQLTCHAR* src,
    SQLTCHAR* dst,
    const size_t dst_byte_count) const
{
    ConvertDriverOutputToTarget(false, src, dst, dst_byte_count);
}

// codechecker_suppress [readability-convert-member-functions-to-static]
void OdbcHelper::ConvertDriverOutputToTarget(
    const bool wrapper_call,
    const SQLTCHAR* src,
    SQLTCHAR* dst,
    const size_t dst_byte_count) const
{
#if UNICODE
    const size_t dst_char_count = dst_byte_count / sizeof(SQLTCHAR);
    const bool user_4_byte = !wrapper_call && use_4_bytes_user_app_;

    if (user_4_byte && use_4_bytes_base_driver_) {
        // Both the client application and the base driver use 4-byte data.
        // Copy the data as-is.
        std::memcpy(dst, src, dst_byte_count);
    } else if (user_4_byte) {
        // Expand the driver output from 2-byte to 4-byte for the client application.
        ConvertUTF16ToUTF32(src, dst, dst_char_count > 0 ? dst_char_count - 1 : 0, dst_char_count);
    } else {
        // Narrow driver output from 4-byte to 2-byte for the client application.
        // Or copy as-is if both are 2-byte.
        Convert4To2ByteString(use_4_bytes_base_driver_, const_cast<SQLTCHAR*>(src), dst, dst_char_count);
    }
#else
    std::memcpy(dst, src, dst_byte_count);
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
#if UNICODE
    // Wrapper output is already in 2-bytes. Only need to expand if client application requires 4-bytes.
    if (!wrapper_call && use_4_bytes_user_app_) {
        const size_t buf_char_count = buf_byte_count / sizeof(SQLTCHAR);
        ExpandUTF16ToUTF32InPlace(buf, char_count, buf_char_count);
    }
#endif
}

// codechecker_suppress [readability-convert-member-functions-to-static]
bool OdbcHelper::NeedsConversion() const {
#if UNICODE
    return use_4_bytes_user_app_ != use_4_bytes_base_driver_;
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
