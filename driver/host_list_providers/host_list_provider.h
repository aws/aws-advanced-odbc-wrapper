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

#ifndef HOST_LIST_PROVIDER_H_
#define HOST_LIST_PROVIDER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "../host_info.h"
#include "../util/connection_string_keys.h"

class HostListProvider {
public:
    HostListProvider(std::string cluster_id) : cluster_id_{ cluster_id } {};
    virtual std::vector<HostInfo> GetCurrentTopology(SQLHDBC hdbc, const HostInfo& initial_host) { return {}; };
    virtual std::vector<HostInfo> Refresh()  { return {}; };
    virtual std::vector<HostInfo> ForceRefresh(bool verify_writer, uint32_t timeout_ms) { return {}; };
    virtual HOST_ROLE GetConnectionRole(SQLHDBC hdbc) { return HOST_ROLE::UNKNOWN; };
    virtual HostInfo GetConnectionInfo(SQLHDBC hdbc) { return {}; }
    virtual std::string GetClusterId() { return {}; }

protected:
    std::string cluster_id_;
};

#endif // HOST_LIST_PROVIDER_H_
