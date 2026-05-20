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

#ifndef MULTI_AZ_TOPOLOGY_UTIL_H
#define MULTI_AZ_TOPOLOGY_UTIL_H

#include "topology_util.h"

class MultiAzTopologyUtil : public TopologyUtil {
public:
    MultiAzTopologyUtil(const std::shared_ptr<OdbcHelper> &odbc_helper, const std::shared_ptr<Dialect> &dialect);
    std::string GetWriterId(SQLHDBC hdbc) override;
    std::vector<HostInfo> GetHosts(SQLHDBC hdbc, const HostInfo &initial_host, const HostInfo &host_template) override;
    virtual HostInfo CreateHost(SQLTCHAR *endpoint, HOST_ROLE role, const HostInfo &host_template);
};

#endif // MULTI_AZ_TOPOLOGY_UTIL_H
