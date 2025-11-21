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

#ifdef WIN32
    #include <windows.h>
#endif

#include "limitless_query_helper.h"

#include <sql.h>
#include <sqlext.h>

#include <cmath>
#include <string>

#include "../../driver.h"
#include "../../host_info.h"
#include "../../util/logger_wrapper.h"

std::vector<HostInfo> LimitlessQueryHelper::QueryForLimitlessRouters(const SQLHDBC conn, const int host_port_to_map, const std::shared_ptr<DialectLimitless> &dialect) {
    const std::string limitless_router_endpoint_query = dialect->GetLimitlessRouterEndpointQuery();

    const DBC* dbc = static_cast<DBC*>(conn);
    SQLHSTMT stmt = SQL_NULL_HANDLE;

    const RdsLibResult res = NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLAllocHandle, RDS_STR_SQLAllocHandle,
        SQL_HANDLE_STMT, dbc->wrapped_dbc, &stmt
    );
    if (!SQL_SUCCEEDED(res.fn_result)) {
        LOG(WARNING) << "Failed to allocate statement handle to query routers";
        return {};
    }

    SQLTCHAR router_endpoint_value[ROUTER_ENDPOINT_LENGTH] = {};
    SQLLEN ind_router_endpoint_value = 0;

    SQLTCHAR load_value[LOAD_LENGTH] = {};
    SQLLEN ind_load_value = 0;

    NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
        stmt, 1, SQL_C_TCHAR, &router_endpoint_value, sizeof(router_endpoint_value), &ind_router_endpoint_value
    );
    NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLBindCol, RDS_STR_SQLBindCol,
        stmt, 2, SQL_C_TCHAR, &load_value, sizeof(load_value), &ind_load_value
    );

    NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLExecDirect, RDS_STR_SQLExecDirect,
        stmt, AS_SQLTCHAR(limitless_router_endpoint_query), SQL_NTS
    );

    std::vector<HostInfo> limitless_routers;

    while (SQL_SUCCEEDED((NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLFetch, RDS_STR_SQLFetch, stmt)).fn_result)) {
        const HostInfo host_info = CreateHost(load_value, router_endpoint_value, host_port_to_map);
        limitless_routers.push_back(host_info);
    }
    LOG_IF(WARNING, limitless_routers.empty()) << "Failed to fetch any Limitless Routers";

    NULL_CHECK_CALL_LIB_FUNC(dbc->env->driver_lib_loader, RDS_FP_SQLFreeHandle, RDS_STR_SQLFreeHandle,
        SQL_HANDLE_STMT, stmt
    );

    return limitless_routers;
}

HostInfo LimitlessQueryHelper::CreateHost(SQLTCHAR* load, SQLTCHAR* router_endpoint, const int host_port_to_map) {
    const double load_num = std::strtod(AS_UTF8_CSTR(load), nullptr);
    uint64_t weight = static_cast<uint64_t>(WEIGHT_SCALING - std::floor(load_num * WEIGHT_SCALING));

    if (weight < MIN_WEIGHT || weight > MAX_WEIGHT) {
        weight = MIN_WEIGHT;
        LOG(WARNING) << "Invalid router load of " << AS_UTF8_CSTR(load) << " for " << AS_UTF8_CSTR(router_endpoint);
    }

    const std::string router_endpoint_str(AS_UTF8_CSTR(router_endpoint));

    return {
        router_endpoint_str,
        host_port_to_map,
        UP,
        true,
        weight
    };
}
