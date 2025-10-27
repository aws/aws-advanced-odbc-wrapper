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
// limitations under the License.w

#ifndef ODBC_DSN_HELPER_H_
#define ODBC_DSN_HELPER_H_

#include <map>

#include "rds_strings.h"

#define MAX_KEY_SIZE 8192
#define MAX_VAL_SIZE 8192
#define ODBC_INI "ODBC.INI"
#define ODBCINST_INI "ODBCINST.INI"

namespace OdbcDsnHelper {
    void LoadAll(const RDS_STR &dsn_key, std::map<RDS_STR, RDS_STR> &conn_map);
    RDS_STR Load(const RDS_STR &dsn_key, const RDS_STR &entry_key);
}

#endif // ODBC_DSN_HELPER_H_
