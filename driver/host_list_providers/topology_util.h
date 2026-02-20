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

#ifndef TOPOLOGY_UTILS_H_
#define TOPOLOGY_UTILS_H_

#include <vector>
#include <memory>

#include "../dialect/dialect.h"

#include "../host_info.h"
#include "../util/odbc_helper.h"

class TopologyUtil {
public:
    TopologyUtil(const std::shared_ptr<OdbcHelper> &odbc_helper, const std::shared_ptr<Dialect> &dialect);
    virtual std::string GetWriterId(SQLHDBC hdbc);
    virtual std::string GetInstanceId(SQLHDBC hdbc);
    virtual std::vector<HostInfo> QueryTopology(SQLHDBC hdbc, const HostInfo& initial_host, const HostInfo& host_template);
    virtual std::vector<HostInfo> VerifyWriter(const std::vector<HostInfo>& all_hosts);
    virtual HOST_ROLE GetConnectionRole(SQLHDBC hdbc);
    virtual std::vector<HostInfo> GetHosts(SQLHSTMT stmt, const HostInfo &initial_host, const HostInfo &host_template, bool use_4_bytes) = 0;
    virtual HostInfo CreateHost(std::string host, int port, HOST_STATE state, HOST_ROLE, uint64_t weight, std::chrono::steady_clock::time_point last_update);

protected:
    std::shared_ptr<OdbcHelper> odbc_helper_;
    std::shared_ptr<Dialect> dialect_;

    static constexpr int BUFFER_SIZE = 1024;
    static constexpr int IS_READER_COL = 1;
};

#endif // TOPOLOGY_UTILS_H_
