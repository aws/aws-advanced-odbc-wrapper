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

#include "plugin_chain_builder.h"

#include "../driver.h"

#include "../plugin/base_plugin.h"
#include "../plugin/default_plugin.h"
#include "../plugin/federated/adfs_auth_plugin.h"
#include "../plugin/federated/okta_auth_plugin.h"
#include "../plugin/iam/iam_auth_plugin.h"
#include "../plugin/secrets_manager/secrets_manager_plugin.h"

std::shared_ptr<BasePlugin> PluginChainBuilder::MonitoringBuild(std::map<std::string, std::string> conn_attr, std::shared_ptr<PluginService> plugin_service) {
    DBC dbc;
    dbc.conn_attr = conn_attr;
    dbc.plugin_service = plugin_service;
    std::shared_ptr<BasePlugin> plugin_head = std::make_shared<DefaultPlugin>(&dbc);
    std::shared_ptr<BasePlugin> next_plugin;

    // Auth Plugins
    if (dbc.conn_attr.contains(KEY_AUTH_TYPE)) {
        const AuthType type = AuthProvider::AuthTypeFromString(dbc.conn_attr.at(KEY_AUTH_TYPE));
        switch (type) {
                case AuthType::IAM:
                    next_plugin = std::make_shared<IamAuthPlugin>(&dbc, plugin_head);
                    plugin_head = next_plugin;
                    break;
                case AuthType::SECRETS_MANAGER:
                    next_plugin = std::make_shared<SecretsManagerPlugin>(&dbc, plugin_head);
                    plugin_head = next_plugin;
                    break;
                case AuthType::ADFS:
                    next_plugin = std::make_shared<AdfsAuthPlugin>(&dbc, plugin_head);
                    plugin_head = next_plugin;
                    break;
                case AuthType::OKTA:
                    next_plugin = std::make_shared<OktaAuthPlugin>(&dbc, plugin_head);
                    plugin_head = next_plugin;
                    break;
                case AuthType::DATABASE:
                case AuthType::INVALID:
                default:
                    break;
        }
    }
    return plugin_head;
}
