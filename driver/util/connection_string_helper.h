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

#ifndef CONNECTION_STRING_HELPER_H_
#define CONNECTION_STRING_HELPER_H_

#include <map>

#include "rds_strings.h"

namespace ConnectionStringHelper {
    void ParseConnectionString(const RDS_STR &conn_str, std::map<RDS_STR, RDS_STR> &conn_map);
    RDS_STR BuildConnectionString(const std::map<RDS_STR, RDS_STR> &conn_map);
}

#endif // CONNECTION_STRING_HELPER_H_
