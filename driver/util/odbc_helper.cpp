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

#include <sql.h>
#include <sqlext.h>

#include "../dialect/dialect.h"
#include "../driver.h"
#include "../odbcapi.h"
#include "rds_strings.h"

std::string GetNodeId(SQLHDBC hdbc, const std::shared_ptr<Dialect>& dialect) {
    const RDS_STR node_id_query = dialect->GetNodeIdQuery();

    SQLHSTMT stmt = SQL_NULL_HANDLE;
    SQLTCHAR node_id[MAX_HOST_SIZE] = {};
    SQLLEN rt = 0;
    RdsLibResult res;
    const DBC* dbc = static_cast<DBC*>(hdbc);

    if (!dbc || !dbc->wrapped_dbc) {
        return "";
    }

    res = NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt
    );

    if (SQL_SUCCEEDED(res.fn_result)) {
        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
            stmt, AS_SQLTCHAR(node_id_query.c_str()), SQL_NTS
        );

        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
            stmt, 1, SQL_C_TCHAR, &node_id, sizeof(node_id), &rt
        );

        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLFetch, RDS_STR_SQLFetch,
            stmt
        );

        NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
            SQL_HANDLE_STMT, stmt
        );
    }

    return ToStr(AS_RDS_CHAR(node_id));
}
