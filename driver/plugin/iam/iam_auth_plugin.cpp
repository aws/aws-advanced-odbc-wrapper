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

#include "iam_auth_plugin.h"

#include "../../util/auth_provider.h"

#include "../../driver.h"
#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/map_utils.h"
#include "../../util/rds_utils.h"

namespace {
    std::string ResolveIamRegion(DBC* dbc)
    {
        const std::string profile = MapUtils::GetStringValue(dbc->conn_attr, KEY_AWS_PROFILE, "");
        std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
        if (region.empty() && !profile.empty()) {
            region = AuthProvider::GetRegionForProfile(profile);
        }
        if (region.empty()) {
            region = dbc->conn_attr.contains(KEY_SERVER) ?
                RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
                : Aws::Region::US_EAST_1;
        }
        return region;
    }

    std::shared_ptr<AuthProvider> CreateIamAuthProvider(DBC* dbc, const std::shared_ptr<AuthProvider>& auth_provider)
    {
        if (auth_provider) {
            return auth_provider;
        }
        const std::string profile = MapUtils::GetStringValue(dbc->conn_attr, KEY_AWS_PROFILE, "");
        return std::make_shared<AuthProvider>(ResolveIamRegion(dbc), profile);
    }
}  // namespace

IamAuthPlugin::IamAuthPlugin(DBC *dbc) : IamAuthPlugin(dbc, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin) : IamAuthPlugin(dbc, next_plugin, nullptr, nullptr, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin, const std::shared_ptr<AuthProvider>& auth_provider)
    : IamAuthPlugin(dbc, next_plugin, auth_provider, nullptr, nullptr) {}

IamAuthPlugin::IamAuthPlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin,
                             const std::shared_ptr<AuthProvider>& auth_provider,
                             std::shared_ptr<Dialect> dialect,
                             std::shared_ptr<OdbcHelper> odbc_helper)
    : BaseTokenAuthPlugin(dbc, next_plugin, CreateIamAuthProvider(dbc, auth_provider), dialect, odbc_helper)
{
    this->plugin_name = "IAM";
}

std::string IamAuthPlugin::ResolveRegion(DBC* dbc)
{
    return ResolveIamRegion(dbc);
}

bool IamAuthPlugin::EnsureCredentials(DBC* dbc, const std::string& region, std::string& out_error)
{
    if (auth_provider && auth_provider->HasResolvedCredentials()) {
        return true;
    }

    const std::string profile = MapUtils::GetStringValue(dbc->conn_attr, KEY_AWS_PROFILE, "");
    std::string msg = "Unable to resolve AWS credentials for the '" + std::string(KEY_AWS_PROFILE) + "' profile";
    if (!profile.empty()) {
        msg += " '" + profile + "'";
    }
    msg += ". If it is an AWS IAM Identity Center (SSO) profile, run 'aws sso login";
    msg += profile.empty() ? "'." : (" --profile " + profile + "'.");
    out_error = msg;
    return false;
}
