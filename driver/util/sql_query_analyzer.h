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

#ifndef SQL_QUERY_ANALYZER_H_
#define SQL_QUERY_ANALYZER_H_

#include <vector>

#include "rds_strings.h"
#include "../driver.h"

class SqlQueryAnalyzer {
public:
    static RDS_STR GetFirstSqlStatement(const RDS_STR& statement);
    static std::vector<RDS_STR> ParseMultiStatement(const RDS_STR& statement);
    static bool DoesOpenTransaction(const RDS_STR& statement);
    static bool DoesCloseTransaction(DBC* dbc, const RDS_STR& statement);
    static bool IsStatementStartingTransaction(const RDS_STR& statement);
    static bool IsStatementClosingTransaction(const RDS_STR& statement);
    static bool IsStatementSettingAutoCommit(const RDS_STR& statement);
    static bool DoesSwitchAutoCommitFalseTrue(DBC* dbc, const RDS_STR& statement);
    static bool GetAutoCommitValueFromSqlStatement(const RDS_STR& statement);
};

#endif // SQL_QUERY_ANALYZER_H_
