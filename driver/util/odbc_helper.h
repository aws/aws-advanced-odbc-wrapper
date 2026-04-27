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

#include <vector>

// Holds a converted SQLTCHAR buffer and a pointer suitable for passing to the
// base driver.  The vector keeps the data alive; `ptr` is nullptr when the
// original input was nullptr (preserving ODBC nullptr semantics).
struct ConvertedSqltchar {
    std::vector<SQLTCHAR> data;
    SQLTCHAR* tchar_ptr = nullptr;
};

class OdbcHelper {
public:
    OdbcHelper(const std::shared_ptr<RdsLibLoader> &lib_loader);

    virtual void Disconnect(const DBC *dbc);
    virtual void Disconnect(SQLHDBC *hdbc);
    virtual void DisconnectAndFree(SQLHDBC *hdbc);

    virtual SQLRETURN AllocEnv(SQLHENV *henv);
    virtual SQLRETURN FreeEnv(SQLHENV *henv);
    virtual SQLRETURN AllocDbc(SQLHENV &henv, SQLHDBC &hdbc);

    virtual RdsLibResult SetEnvAttr(const ENV *henv, SQLINTEGER attribute, SQLPOINTER pointer, int length);
    virtual RdsLibResult Fetch(SQLHSTMT *stmt);
    virtual RdsLibResult BindCol(const SQLHSTMT *stmt, int column, int type, void *value, size_t size, SQLLEN *len);
    virtual RdsLibResult ExecDirect(const SQLHSTMT *stmt, const std::string &query);

    virtual RdsLibResult BaseAllocEnv(ENV *env);
    virtual RdsLibResult BaseAllocStmt(const SQLHDBC *wrapped_dbc, SQLHSTMT *stmt);
    virtual RdsLibResult BaseFreeStmt(SQLHSTMT *stmt);

    virtual std::shared_ptr<RdsLibLoader> GetLibLoader();

    bool GetUse4BytesBaseDriver() const;
    bool GetUse4BytesUserApp() const;
    void SetUse4BytesBaseDriver(bool use_4_bytes);
    void SetUse4BytesUserApp(bool use_4_bytes);

    // Converts a user-app SQLTCHAR* input to the encoding expected by the base
    // driver, respecting the current user_4_byte / driver_4_byte flags.
    // Returns a ConvertedSqltchar whose `tchar_ptr` is nullptr when `in` is nullptr.
    // In non-Unicode builds the input pointer is passed through unchanged.
    ConvertedSqltchar ConvertInput(SQLTCHAR* in, SQLINTEGER in_length) const;

    virtual std::string GetSqlStateAndLogMessage(DBC *dbc);

private:
    std::shared_ptr<RdsLibLoader> lib_loader_;
    bool use_4_bytes_base_driver_ = false;
    bool use_4_bytes_user_app_ = false;
};

#endif //ODBC_HELPER_H
