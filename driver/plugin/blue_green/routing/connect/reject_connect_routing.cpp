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

#include "reject_connect_routing.h"

#include "../../../../error.h"

#include "../../../../util/logger_wrapper.h"

SQLRETURN RejectConnectRouting::Connect(
    DBC* dbc,
    HostInfo info,
    std::shared_ptr<OdbcHelper> odbc_helper,
    std::shared_ptr<ConcurrentMap<std::string, std::unique_ptr<BaseConnectRouting>>> status_cache)
{
    std::string error_msg("Blue/Green Deployment switchover is in progress. New connection can't be opened.");
    throw std::runtime_error(error_msg);
}
