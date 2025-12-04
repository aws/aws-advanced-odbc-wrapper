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

#ifndef ODBC_HELPER_H_
#define ODBC_HELPER_H_

#ifdef WIN32
    #include <windows.h>
#endif

#include <sqlext.h>

#include <string>

#define SQL_MAX_MESSAGE_LENGTH  512
#define SQL_ERR_UNABLE_TO_CONNECT "08001"
#define SQL_ERR_INVALID_PARAMETER "01S00"

namespace ODBC_HELPER {
    SQLRETURN DriverConnect(SQLHDBC hdbc, std::string conn_str);
    SQLRETURN DsnConnect(SQLHDBC hdbc, std::string dsn, std::string uid, std::string pwd);
    void CleanUpHandles(SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt);
    SQLRETURN ExecuteQuery(SQLHSTMT stmt, std::string query);
    void PrintHandleError(SQLHANDLE handle, int32_t handle_type);
};

#endif // ODBC_HELPER_H_
