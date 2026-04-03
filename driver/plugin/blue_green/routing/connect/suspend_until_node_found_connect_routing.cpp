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

#include "suspend_until_node_found_connect_routing.h"

#include "../../../../util/connection_string_keys.h"
#include "../../../../util/logger_wrapper.h"
#include "../../../../util/map_utils.h"

SQLRETURN SuspendUntilNodeFoundConnectRouting::Connect(
    DBC* dbc,
    HostInfo info,
    std::shared_ptr<OdbcHelper> odbc_helper,
    std::shared_ptr<ConcurrentMap<std::string, BlueGreenStatus>> status_cache)
{
    BlueGreenStatus cached_status = status_cache->Get(this->blue_green_id_);
    std::string host = info.GetHost();
    std::map<std::string, std::pair<HostInfo, HostInfo>> nodes = cached_status.GetCorrespondingNodes()->GetMapCopy();

    std::chrono::milliseconds timeout = MapUtils::GetMillisecondsValue(dbc->conn_attr, KEY_BG_CONNECT_TIMEOUT_MS, DEFAULT_CONNECT_TIMEOUT_MS);
    std::chrono::system_clock::time_point start_time = GetCurrTime();
    std::chrono::system_clock::time_point end_time = start_time + timeout;

    while (GetCurrTime() <= end_time && cached_status.GetCurrentPhase().GetPhase() != BlueGreenPhase::COMPLETED && !nodes.contains(host)) {
        this->Delay(SLEEP_DURATION_MS, cached_status, status_cache, this->blue_green_id_);
        cached_status = status_cache->Get(this->blue_green_id_);
    };

    if (cached_status.GetCurrentPhase().GetPhase() == BlueGreenPhase::COMPLETED) {
        return SQL_SUCCESS_WITH_INFO;
    } else if (GetCurrTime() > end_time) {
        return SQL_ERROR;
    }

    return SQL_SUCCESS;
}
