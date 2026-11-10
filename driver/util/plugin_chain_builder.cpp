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

#include <stdexcept>
#include <string>

#include "../driver.h"
#include "../plugin/aurora_initial_connection_strategy/aurora_initial_connection_strategy_plugin.h"
#include "../plugin/base_plugin.h"
#include "../plugin/blue_green/blue_green_plugin.h"
#include "../plugin/custom_endpoint/custom_endpoint_plugin.h"
#include "../plugin/default_plugin.h"
#include "../plugin/failover/failover_plugin.h"
#include "../plugin/federated/adfs_auth_plugin.h"
#include "../plugin/federated/aws_sso_auth_plugin.h"
#include "../plugin/federated/okta_auth_plugin.h"
#include "../plugin/iam/iam_auth_plugin.h"
#include "../plugin/limitless/limitless_plugin.h"
#include "../plugin/read_write_splitting/read_write_splitting_plugin.h"
#include "../plugin/read_write_splitting/simple_read_write_splitting_plugin.h"
#include "../plugin/secrets_manager/secrets_manager_plugin.h"
#include "auth_provider.h"
#include "connection_string_keys.h"

namespace {
    bool IsEnabled(const DBC* dbc, const std::string& key) {
        return dbc->conn_attr.contains(key) && dbc->conn_attr.at(key) == VALUE_BOOL_TRUE;
    }

    std::shared_ptr<BasePlugin> BuildDefaultAndAuth(DBC* dbc) {
        std::shared_ptr<BasePlugin> plugin_head = std::make_shared<DefaultPlugin>(dbc);

        if (dbc->conn_attr.contains(KEY_AUTH_TYPE)) {
            const AuthType type = AuthProvider::AuthTypeFromString(dbc->conn_attr.at(KEY_AUTH_TYPE));
            switch (type) {
                    case AuthType::IAM:
                        plugin_head = std::make_shared<IamAuthPlugin>(dbc, plugin_head);
                        break;
                    case AuthType::SECRETS_MANAGER:
                        plugin_head = std::make_shared<SecretsManagerPlugin>(dbc, plugin_head);
                        break;
                    case AuthType::ADFS:
                        plugin_head = std::make_shared<AdfsAuthPlugin>(dbc, plugin_head);
                        break;
                    case AuthType::OKTA:
                        plugin_head = std::make_shared<OktaAuthPlugin>(dbc, plugin_head);
                        break;
                    case AuthType::AWS_SSO:
                        plugin_head = std::make_shared<AwsSsoAuthPlugin>(dbc, plugin_head);
                        break;
                    case AuthType::DATABASE:
                    case AuthType::INVALID:
                    default:
                        break;
            }
        }
        return plugin_head;
    }
}  // namespace

std::shared_ptr<BasePlugin> PluginChainBuilder::Build(DBC* dbc) {
    std::shared_ptr<BasePlugin> plugin_head = BuildDefaultAndAuth(dbc);

    if (IsEnabled(dbc, KEY_ENABLE_LIMITLESS)) {
        plugin_head = std::make_shared<LimitlessPlugin>(dbc, plugin_head);
    }

    if (IsEnabled(dbc, KEY_ENABLE_FAILOVER)) {
        plugin_head = std::make_shared<FailoverPlugin>(dbc, plugin_head);
    }

    if (IsEnabled(dbc, KEY_ENABLE_AURORA_INITIAL_CONNECTION_STRATEGY)) {
        plugin_head = std::make_shared<AuroraInitialConnectionStrategyPlugin>(dbc, plugin_head);
    }

    const bool srw_enabled = IsEnabled(dbc, KEY_ENABLE_SRW_SPLIT);
    if (srw_enabled) {
        plugin_head = std::make_shared<SimpleReadWriteSplittingPlugin>(dbc, plugin_head);
    }

    if (IsEnabled(dbc, KEY_ENABLE_RW_SPLIT)) {
        if (srw_enabled) {
            throw std::runtime_error("Only one of the Read Write Splitting and Simple Read Write Splitting plugins should be enabled at a time.");
        }
        plugin_head = std::make_shared<ReadWriteSplittingPlugin>(dbc, plugin_head);
    }

    if (IsEnabled(dbc, KEY_ENABLE_CUSTOM_ENDPOINT)) {
        plugin_head = std::make_shared<CustomEndpointPlugin>(dbc, plugin_head);
    }

    if (IsEnabled(dbc, KEY_ENABLE_BLUE_GREEN)) {
        plugin_head = std::make_shared<BlueGreenPlugin>(dbc, plugin_head);
    }

    if (!plugin_head) {
        throw std::runtime_error("Plugin chain construction produced an empty chain.");
    }
    return plugin_head;
}

std::shared_ptr<BasePlugin> PluginChainBuilder::MonitoringBuild(DBC* dbc) {
    return BuildDefaultAndAuth(dbc);
}
