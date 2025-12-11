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

#ifndef ODBC_HELPER_H
#define ODBC_HELPER_H

#include "../driver.h"
#include "../odbcapi.h"
#include "rds_lib_loader.h"

class OdbcHelper {
public:
    void Disconnect(const DBC *&dbc, const std::shared_ptr<RdsLibLoader> &lib_loader);
    void Disconnect(SQLHDBC hdbc, const std::shared_ptr<RdsLibLoader> &lib_loader);
    void DisconnectAndFree(const SQLHDBC &hdbc, const std::shared_ptr<RdsLibLoader> &lib_loader);
    RdsLibResult AllocEnv(SQLHENV &henv, ENV *&env, const std::shared_ptr<RdsLibLoader> &lib_loader);
    void FreeEnv(const SQLHENV &henv);
    RdsLibResult AllocStmt(const SQLHDBC &wrapped_dbc, const std::shared_ptr<RdsLibLoader> &lib_loader, SQLHSTMT &stmt);
    RdsLibResult FreeStmt(const std::shared_ptr<RdsLibLoader> &lib_loader, SQLHSTMT &stmt);

    DBC *AllocDbc(SQLHENV &henv, SQLHDBC &hdbc);
    void SQLSetEnvAttr(const std::shared_ptr<RdsLibLoader> &lib_loader, const ENV *henv, SQLINTEGER attribute,
                       SQLPOINTER pointer, int length);
    RdsLibResult SQLFetch(const std::shared_ptr<RdsLibLoader> &lib_loader, SQLHSTMT &stmt);
    RdsLibResult SQLBindCol(const std::shared_ptr<RdsLibLoader> &lib_loader, const SQLHSTMT &stmt, int column, int type,
                            void *value, size_t size, SQLLEN len);
    RdsLibResult SQLExecDirect(const std::shared_ptr<RdsLibLoader> &lib_loader, const SQLHSTMT &stmt,
                               const std::string &query);
};


#endif //ODBC_HELPER_H