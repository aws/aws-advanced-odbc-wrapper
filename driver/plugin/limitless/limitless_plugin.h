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

#ifndef LIMITLESS_PLUGIN_H_
#define LIMITLESS_PLUGIN_H_

#include "../base_plugin.h"
#include "../../dialect/dialect.h"
#include "limitless_router_service.h"

class LimitlessPlugin : public BasePlugin {
public:
    LimitlessPlugin(DBC* dbc);
    LimitlessPlugin(DBC* dbc, BasePlugin* next_plugin);
    LimitlessPlugin(DBC* dbc, BasePlugin* next_plugin, std::shared_ptr<Dialect> dialect, std::shared_ptr<LimitlessRouterService> limitless_router_service_);
    ~LimitlessPlugin() override;

    SQLRETURN Connect(
        SQLHDBC        ConnectionHandle,
        SQLHWND        WindowHandle,
        SQLTCHAR *     OutConnectionString,
        SQLSMALLINT    BufferLength,
        SQLSMALLINT *  StringLengthPtr,
        SQLUSMALLINT   DriverCompletion) override;

private:
    std::shared_ptr<LimitlessRouterService> limitless_router_service_;
    std::shared_ptr<Dialect> dialect_;
};

#endif // LIMITLESS_PLUGIN_H_
