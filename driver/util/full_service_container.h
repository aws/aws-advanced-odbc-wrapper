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

#ifndef FULL_SERVICE_CONTAINER_H_
#define FULL_SERVICE_CONTAINER_H_

#include <memory>
#include <string>

class MonitorService;
class PluginManager;
class PluginService;
class RdsLibLoader;
class StorageService;
struct DBC;
struct ENV;

// The services a single connection uses. Every DBC creates its own.
//
// StorageService and MonitorService are shared by all connections, so they are only retrieved from CoreService.
// PluginService and PluginManager belong to the connection. They need its connection attributes, so they are
// created on the first connect, then kept across SQLDisconnect and reused when the DBC connects again.
//
// Internal DBCs that borrow another connection's PluginService, e.g. a read write splitting reader or a
// monitor's connections, only call SetPluginService(). Without a PluginManager, freeing them does not release
// the plugin chain of the connection they borrow from.
class FullServiceContainer {
public:
    // Retrieves the core services from CoreService
    FullServiceContainer();
    ~FullServiceContainer();

    // Creates the PluginService and PluginManager of a user connection, unless an earlier connect already did
    static void InitConnectionServices(DBC* dbc, const std::string& conn_str);
    // Creates the PluginService and the reduced PluginManager of the DBC a monitor opens its connections from
    static void InitMonitoringServices(DBC* dbc, const std::shared_ptr<RdsLibLoader>& lib_loader, const ENV* env = nullptr);

    // Owned by CoreService, which is never destroyed
    [[nodiscard]] StorageService& GetStorageService() const;
    [[nodiscard]] MonitorService& GetMonitorService() const;

    // nullptr until the first connect
    [[nodiscard]] std::shared_ptr<PluginService> GetPluginService() const;
    [[nodiscard]] std::shared_ptr<PluginManager> GetPluginManager() const;
    void SetPluginService(std::shared_ptr<PluginService> plugin_service);
    void SetPluginManager(std::shared_ptr<PluginManager> plugin_manager);

    // Prevent copy constructors
    FullServiceContainer(const FullServiceContainer&) = delete;
    FullServiceContainer(FullServiceContainer&&) = delete;
    FullServiceContainer& operator=(const FullServiceContainer&) = delete;
    FullServiceContainer& operator=(FullServiceContainer&&) = delete;

private:
    StorageService& storage_service_;
    MonitorService& monitor_service_;

    // Members are destroyed in reverse order, so the plugin chain goes before the PluginService it uses
    std::shared_ptr<PluginService> plugin_service_;
    std::shared_ptr<PluginManager> plugin_manager_;
};

#endif // FULL_SERVICE_CONTAINER_H_
