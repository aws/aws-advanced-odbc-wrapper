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

#ifndef LIMITLESS_QUERY_HELPER_H_
#define LIMITLESS_QUERY_HELPER_H_

#ifdef WIN32
    #include <windows.h>
#endif

#include <sqltypes.h>
#include <vector>

#include "../../host_info.h"
#include "../../dialect/dialect.h"

class LimitlessQueryHelper {
public:
    // Generally accepted URL endpoint max length (2048) + 1 for null terminator
    static const int ROUTER_ENDPOINT_LENGTH = 2049;
    static const int LOAD_LENGTH = 5;
    static const int WEIGHT_SCALING = 10;
    static const int MAX_WEIGHT = 10;
    static const int MIN_WEIGHT = 1;

    static std::vector<HostInfo> QueryForLimitlessRouters(SQLHDBC conn, int host_port_to_map, const std::shared_ptr<DialectLimitless> &dialect);

private:
    static HostInfo create_host(SQLTCHAR* load, SQLTCHAR* router_endpoint, int host_port_to_map);
};

#endif // LIMITLESS_QUERY_HELPER_H_
