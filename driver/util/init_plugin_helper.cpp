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

#include <map>

#include "../dialect/dialect.h"
#include "../dialect/dialect_aurora_postgres.h"
#include "init_plugin_helper.h"
#include "rds_utils.h"

std::shared_ptr<Dialect> InitDialect(std::map<std::string, std::string> conn_info)
{
    DatabaseDialectType dialect = DatabaseDialectType::UNKNOWN_DIALECT;
    if (conn_info.contains(KEY_DATABASE_DIALECT)) {
        dialect = Dialect::DatabaseDialectFromString(conn_info.at(KEY_DATABASE_DIALECT));
    }

    if (dialect == DatabaseDialectType::UNKNOWN_DIALECT) {
        // TODO - Dialect from host
        // For release, we are only supporting Aurora PostgreSQL and Aurora PostgreSQL Limitless
        const std::string host = conn_info.at(KEY_SERVER);

        if (RdsUtils::IsLimitlessDbShardGroupDns(host)) {
            dialect = DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS;
        } else {
            dialect = DatabaseDialectType::AURORA_POSTGRESQL;
        }
    }

    switch (dialect) {
        case DatabaseDialectType::AURORA_POSTGRESQL:
            return std::make_shared<DialectAuroraPostgres>();
        case DatabaseDialectType::AURORA_POSTGRESQL_LIMITLESS:
            return std::make_shared<DialectAuroraPostgresLimitless>();
        default:
            return std::make_shared<Dialect>();
    }
}
