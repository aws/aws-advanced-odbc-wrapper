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

#ifndef LIMITLESS_MOCK_OBJECTS_H_
#define LIMITLESS_MOCK_OBJECTS_H_

#include "gmock/gmock.h"

#include "../../driver/plugin/limitless/limitless_router_service.h"

class MockLimitlessRouterService : public LimitlessRouterService {
    public:
        MockLimitlessRouterService(
            const std::shared_ptr<DialectLimitless> &dialect,
            const std::map<std::string, std::string> &conn_attr,
            const std::shared_ptr<OdbcHelper> &odbc_helper) : LimitlessRouterService(dialect, conn_attr, odbc_helper, nullptr) {}
        MOCK_METHOD(SQLRETURN, EstablishConnection, (BasePlugin* plugin_head, DBC* dbc), ());
        MOCK_METHOD(void, StartMonitoring, (DBC* dbc, const std::shared_ptr<DialectLimitless> &dialect), ());
};

class MockLimitlessRouterMonitor : public LimitlessRouterMonitor {
    public:
        MockLimitlessRouterMonitor(
            BasePlugin* plugin_head,
            const std::shared_ptr<OdbcHelper> &odbc_helper,
            const std::shared_ptr<DialectLimitless>& dialect) : LimitlessRouterMonitor(plugin_head, dialect, odbc_helper, nullptr) {}
        MOCK_METHOD(void, Open, (DBC* dbc, bool block_and_query_immediately, int host_port, unsigned int interval_ms));
};

class TestLimitlessRouterService : public LimitlessRouterService {
public:
    TestLimitlessRouterService(
        const std::shared_ptr<DialectLimitless> &dialect,
        const std::map<std::string, std::string> &conn_attr,
        const std::shared_ptr<OdbcHelper> &odbc_helper) : LimitlessRouterService(dialect, conn_attr, odbc_helper, nullptr) {}
    std::shared_ptr<LimitlessRouterMonitor> CreateMonitor(
        const std::map<std::string, std::string>& conn_attr,
        BasePlugin* plugin_head,
        DBC* dbc,
        const std::shared_ptr<DialectLimitless>& dialect) const override {
        return std::make_shared<MockLimitlessRouterMonitor>(plugin_head, std::make_shared<OdbcHelper>(nullptr), dialect);
    };
};

#endif // LIMITLESS_MOCK_OBJECTS_H_
