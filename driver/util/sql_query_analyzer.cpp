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

#include "sql_query_analyzer.h"

#include "rds_strings.h"

RDS_STR SqlQueryAnalyzer::GetFirstSqlStatement(const RDS_STR &statement)
{
    std::vector<RDS_STR> query_list = ParseMultiStatement(statement);
    if (query_list.empty()) {
        return statement;
    }
    RDS_STR first_statement = query_list.front();
    RDS_STR_UPPER(first_statement);
    // Remove spaces and comments (/* */)
    const RDS_REGEX space_comment_pattern(TEXT(R"(\s*/\*(.*?)\*/\s*)"));
    first_statement = std::regex_replace(first_statement, space_comment_pattern, TEXT(" "));
    first_statement = TrimStr(first_statement);

    return first_statement;
}

std::vector<RDS_STR> SqlQueryAnalyzer::ParseMultiStatement(const RDS_STR &statement)
{
    RDS_STR local_statement(statement);
    if (local_statement.empty()) {
        return {};
    }

    // Remove spaces
    local_statement = TrimStr(local_statement);
    if (local_statement.empty()) {
        return {};
    }

    RDS_STR delimiter = AS_RDS_STR(";");
    return SplitStr(local_statement, delimiter);
}

bool SqlQueryAnalyzer::DoesOpenTransaction(const RDS_STR &statement)
{
    const RDS_STR first_statement = GetFirstSqlStatement(statement);
    return IsStatementStartingTransaction(first_statement);
}

bool SqlQueryAnalyzer::DoesCloseTransaction(DBC* dbc, const RDS_STR &statement)
{
    if (DoesSwitchAutoCommitFalseTrue(dbc, statement)) {
        return true;
    }

    const RDS_STR first_statement = GetFirstSqlStatement(statement);
    return IsStatementClosingTransaction(first_statement);
}

bool SqlQueryAnalyzer::IsStatementStartingTransaction(const RDS_STR &statement)
{
    return statement.starts_with(TEXT("BEGIN"))
        || statement.starts_with(TEXT("START TRANSACTION"));
}

bool SqlQueryAnalyzer::IsStatementClosingTransaction(const RDS_STR &statement)
{
    return statement.starts_with(TEXT("COMMIT"))
        || statement.starts_with(TEXT("ROLLBACK"))
        || statement.starts_with(TEXT("END"))
        || statement.starts_with(TEXT("ABORT"));
}

bool SqlQueryAnalyzer::IsStatementSettingAutoCommit(const RDS_STR &statement)
{
    const RDS_STR first_statement = GetFirstSqlStatement(statement);
    return std::string::npos != first_statement.find(TEXT("SET AUTOCOMMIT"));
}

bool SqlQueryAnalyzer::DoesSwitchAutoCommitFalseTrue(DBC* dbc, const RDS_STR &statement)
{
    const bool last_auto_commit = dbc->auto_commit;
    const bool new_auto_commit = IsStatementSettingAutoCommit(statement)
        ? GetAutoCommitValueFromSqlStatement(statement) : false;
    return !last_auto_commit && new_auto_commit;
}

bool SqlQueryAnalyzer::GetAutoCommitValueFromSqlStatement(const RDS_STR &statement)
{
    RDS_STR first_statement = GetFirstSqlStatement(statement);
    size_t separator_index = first_statement.find(TEXT('='));
    size_t value_index;
    if (std::string::npos != separator_index) {
        value_index = separator_index + 1;
    } else {
        separator_index = first_statement.find(TEXT(" TO "));
        if (std::string::npos == separator_index) {
            return false;
        }
        value_index = separator_index + 1;
    }

    first_statement = first_statement.substr(value_index);
    separator_index = first_statement.find(TEXT(';'));
    if (std::string::npos != separator_index) {
        first_statement = first_statement.substr(separator_index);
    }

    first_statement = TrimStr(first_statement);
    return
        std::string::npos != first_statement.find(TEXT("TRUE"))
        || std::string::npos != first_statement.find(TEXT('1'))
        || std::string::npos != first_statement.find(TEXT("ON"));
}
