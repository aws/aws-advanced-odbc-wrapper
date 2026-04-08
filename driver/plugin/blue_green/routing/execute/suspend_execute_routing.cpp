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

#include "suspend_execute_routing.h"

#include "../../../../error.h"

#include "../../../../util/connection_string_keys.h"
#include "../../../../util/logger_wrapper.h"
#include "../../../../util/map_utils.h"

SQLRETURN SuspendExecuteRouting::Execute(
    STMT* stmt,
    std::shared_ptr<OdbcHelper> odbc_helper,
    std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache)
{
    DBC* dbc = stmt->dbc;
    std::map<std::string, std::string> conn_attr = dbc->conn_attr;

    std::chrono::milliseconds timeout = MapUtils::GetMillisecondsValue(conn_attr, KEY_BG_CONNECT_TIMEOUT_MS, DEFAULT_CONNECT_TIMEOUT_MS);
    std::chrono::steady_clock::time_point start_time = GetCurrTime();
    std::chrono::steady_clock::time_point end_time = start_time + timeout;

    BlueGreenStatus cached_status = status_cache->Get(this->blue_green_id_);

    while (GetCurrTime() <= end_time && cached_status.GetCurrentPhase().GetPhase() == BlueGreenPhase::IN_PROGRESS) {
        this->Delay(SLEEP_DURATION_MS, cached_status, status_cache, this->blue_green_id_);
        cached_status = status_cache->Get(this->blue_green_id_);
    };

    if (cached_status.GetCurrentPhase().GetPhase() == BlueGreenPhase::IN_PROGRESS) {
        std::string error_msg("Blue/Green Deployment switchover is still in progress after ");
        error_msg += std::to_string(timeout.count());
        error_msg += " ms. Try executing again later.";
        LOG(ERROR) << error_msg;
        throw std::runtime_error(error_msg);
    }

    return SQL_ERROR;
}
