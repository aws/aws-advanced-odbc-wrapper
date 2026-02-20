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

#include "cluster_helper.h"

#include "odbc_helper.h"

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include "../dialect/dialect.h"
#include "../driver.h"
#include "../odbcapi.h"
#include "rds_strings.h"

#include "rds_lib_loader.h"

std::string GetNodeId(SQLHDBC hdbc, const std::shared_ptr<Dialect>& dialect, const std::shared_ptr<OdbcHelper> &odbc_helper) {
    const std::string node_id_query = dialect->GetNodeIdQuery();

    SQLHSTMT stmt = SQL_NULL_HANDLE;
    SQLTCHAR node_id[MAX_HOST_SIZE * 2] = {};
    SQLLEN rt = 0;
    RdsLibResult res;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc || dbc->conn_status != CONN_CONNECTED) {
        return "";
    }

    res = NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt
    );

    if (SQL_SUCCEEDED(res.fn_result)) {
        odbc_helper->ExecDirect(&stmt, node_id_query);

        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
            stmt, 1, SQL_C_TCHAR, &node_id, MAX_HOST_SIZE, &rt
        );

        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLFetch, RDS_STR_SQLFetch,
            stmt
        );

        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
            SQL_HANDLE_STMT, stmt
        );
    }

#if UNICODE
    Convert4To2ByteString(odbc_helper->GetUse4BytesBaseDriver(), node_id, nullptr, MAX_HOST_SIZE);
    return AS_UTF8_CSTR(node_id);
#endif

    return AS_UTF8_CSTR(node_id);
}
