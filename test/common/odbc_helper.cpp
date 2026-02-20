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

#ifdef WIN32
    #include <windows.h>
#endif

#include <sqlext.h>

#include <string>
#include <iostream>

#include "../common/string_helper.h"

SQLRETURN ODBC_HELPER::DriverConnect(SQLHDBC hdbc, std::string conn_str) {
    std::cout << "Connecting to: " << conn_str << std::endl;

    SQLTCHAR conn_str_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(conn_str.c_str(), conn_str_in);

    SQLRETURN ret = SQLDriverConnect(hdbc, nullptr, conn_str_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);

    if (!SQL_SUCCEEDED(ret)) {
        PrintHandleError(hdbc, SQL_HANDLE_DBC);
    }
    return ret;
}

SQLRETURN ODBC_HELPER::DsnConnect(SQLHDBC hdbc, std::string dsn, std::string uid, std::string pwd) {
    SQLTCHAR dsn_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(dsn.c_str(), dsn_in);
    SQLTCHAR uid_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(uid.c_str(), uid_in);
    SQLTCHAR pwd_in[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(pwd.c_str(), pwd_in);

    SQLRETURN ret = SQLConnect(hdbc,
        dsn_in, SQL_NTS,
        uid_in, SQL_NTS,
        pwd_in, SQL_NTS);

    if (!SQL_SUCCEEDED(ret)) {
        PrintHandleError(hdbc, SQL_HANDLE_DBC);
    }
    return ret;
}

void ODBC_HELPER::CleanUpHandles(SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt) {
    if (SQL_NULL_HANDLE != hstmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        hstmt = SQL_NULL_HSTMT;
    }
    if (SQL_NULL_HANDLE != hdbc) {
        SQLDisconnect(hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        hdbc = SQL_NULL_HDBC;
    }
    if (SQL_NULL_HANDLE != henv) {
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        henv = SQL_NULL_HENV;
    }
}

SQLRETURN ODBC_HELPER::ExecuteQuery(SQLHSTMT stmt, std::string query) {
    SQLTCHAR buffer[STRING_HELPER::MAX_SQLCHAR] = { 0 };
    STRING_HELPER::AnsiToUnicode(query.c_str(), buffer);

    SQLRETURN ret = SQLExecDirect(stmt, buffer, query.length());

    if (!SQL_SUCCEEDED(ret)) {
        PrintHandleError(stmt, SQL_HANDLE_STMT);
    }
    return ret;
}

void ODBC_HELPER::PrintHandleError(SQLHANDLE handle, int32_t handle_type) {
    SQLTCHAR    sqlstate[6] = { 0 };
    SQLTCHAR    message[1024] = { 0 };
    SQLINTEGER  native_error = 0;
    SQLSMALLINT textlen = 0;
    SQLRETURN   ret = SQL_ERROR;
    SQLSMALLINT recno = 0;

    do {
        recno++;
        ret = SQLGetDiagRec(handle_type, handle, recno, sqlstate, &native_error, message, sizeof(message), &textlen);
        if (ret == SQL_INVALID_HANDLE) {
            std::cout << "Invalid handle" << std::endl;
        } else if (SQL_SUCCEEDED(ret)) {
            std::cout << STRING_HELPER::SqltcharToAnsi(sqlstate) << ": " << STRING_HELPER::SqltcharToAnsi(message) << std::endl;
        }
    } while (ret == SQL_SUCCESS);
}
