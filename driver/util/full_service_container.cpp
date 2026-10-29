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

#include "full_service_container.h"

#include <utility>

#include "../driver.h"
#include "connection_string_helper.h"
#include "core_service.h"
#include "plugin_manager.h"
#include "plugin_service.h"

FullServiceContainer::FullServiceContainer()
    : storage_service_{ CoreService::Instance().GetStorageService() },
      monitor_service_{ CoreService::Instance().GetMonitorService() } {}

FullServiceContainer::~FullServiceContainer() = default;

void FullServiceContainer::InitConnectionServices(DBC* dbc, const std::string& conn_str)
{
    FullServiceContainer& container = *dbc->service_container;
    if (!container.plugin_service_) {
        PluginService::InitClusterId(dbc->conn_attr);
        const ENV* env = dbc->env;
        container.plugin_service_ = std::make_shared<PluginService>(env->driver_lib_loader, env, dbc->conn_attr,
            conn_str.empty()
            ? ConnectionStringHelper::BuildFullConnectionString(dbc->conn_attr)
            : conn_str
        );
    }
    if (!container.plugin_manager_) {
        // Published only once built, so the next connect retries a chain that failed to build
        const std::shared_ptr<PluginManager> plugin_manager = std::make_shared<PluginManager>();
        plugin_manager->InitPlugins(dbc);
        container.plugin_manager_ = plugin_manager;
    }
}

void FullServiceContainer::InitMonitoringServices(DBC* dbc, const std::shared_ptr<RdsLibLoader>& lib_loader, const ENV* env)
{
    FullServiceContainer& container = *dbc->service_container;
    container.plugin_service_ = std::make_shared<PluginService>(lib_loader, env, dbc->conn_attr);
    const std::shared_ptr<PluginManager> plugin_manager = std::make_shared<PluginManager>();
    plugin_manager->InitMonitoringPlugins(dbc);
    container.plugin_manager_ = plugin_manager;
}

StorageService& FullServiceContainer::GetStorageService() const
{
    return storage_service_;
}

MonitorService& FullServiceContainer::GetMonitorService() const
{
    return monitor_service_;
}

std::shared_ptr<PluginService> FullServiceContainer::GetPluginService() const
{
    return plugin_service_;
}

std::shared_ptr<PluginManager> FullServiceContainer::GetPluginManager() const
{
    return plugin_manager_;
}

void FullServiceContainer::SetPluginService(std::shared_ptr<PluginService> plugin_service)
{
    plugin_service_ = std::move(plugin_service);
}

void FullServiceContainer::SetPluginManager(std::shared_ptr<PluginManager> plugin_manager)
{
    plugin_manager_ = std::move(plugin_manager);
}
