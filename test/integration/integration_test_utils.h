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

#ifndef INTEGRATION_TEST_UTILS_H_
#define INTEGRATION_TEST_UTILS_H_

#include <gtest/gtest.h>

#ifdef WIN32
    #include <windows.h>
#endif

#include <sqlext.h>

#define SQL_MAX_MESSAGE_LENGTH  512
#define SQL_ERR_UNABLE_TO_CONNECT "08001"
#define SQL_ERR_INVALID_PARAMETER "01S00"

namespace INTEGRATION_TEST_UTILS {
    char* get_env_var(const char* key, char* default_value);
    int str_to_int(const char* str);
    double str_to_double(const char* str);
    std::string host_to_IP(std::string hostname);
    void print_errors(SQLHANDLE handle, int32_t handle_type);
    SQLRETURN exec_query(SQLHSTMT stmt, char *query_buffer);
	void clear_memory(void* dest, size_t count);
    void odbc_cleanup(SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt);
};

#endif // INTEGRATION_TEST_UTILS_H_
