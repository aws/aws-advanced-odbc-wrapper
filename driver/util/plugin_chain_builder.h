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

#ifndef PLUGIN_CHAIN_BUILDER_H_
#define PLUGIN_CHAIN_BUILDER_H_

#include <memory>

class BasePlugin;
struct DBC;

// Used by PluginManager. The plugins read the DBC's PluginService while being constructed, so it must already be set
namespace PluginChainBuilder {
    // The plugins enabled in the connection attributes. Throws if the configuration is invalid
    std::shared_ptr<BasePlugin> Build(DBC* dbc);
    // The default plugin plus the auth plugin, for the connections monitors open
    std::shared_ptr<BasePlugin> MonitoringBuild(DBC* dbc);
}  // namespace PluginChainBuilder

#endif // PLUGIN_CHAIN_BUILDER_H_
