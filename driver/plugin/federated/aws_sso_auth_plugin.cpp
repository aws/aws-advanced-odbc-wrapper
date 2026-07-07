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

#include "aws_sso_auth_plugin.h"

#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/map_utils.h"
#include "../../util/rds_utils.h"

AwsSsoAuthPlugin::AwsSsoAuthPlugin(DBC* dbc) : AwsSsoAuthPlugin(dbc, nullptr) {}

AwsSsoAuthPlugin::AwsSsoAuthPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin)
    : AwsSsoAuthPlugin(dbc, next_plugin, nullptr, nullptr, nullptr, nullptr) {}

AwsSsoAuthPlugin::AwsSsoAuthPlugin(
    DBC* dbc,
    std::shared_ptr<BasePlugin> next_plugin,
    const std::shared_ptr<SsoBrowserLoginUtil>& login_util,
    const std::shared_ptr<AuthProvider>& auth_provider,
    std::shared_ptr<Dialect> dialect,
    std::shared_ptr<OdbcHelper> odbc_helper)
    : BaseTokenAuthPlugin(dbc, next_plugin, auth_provider, dialect, odbc_helper)
{
    this->plugin_name = "AWS_SSO";
    login_util_ = login_util ? login_util : std::make_shared<SsoBrowserLoginUtil>(dbc->conn_attr);
}

AwsSsoAuthPlugin::~AwsSsoAuthPlugin()
{
    if (login_util_) {
        login_util_.reset();
    }
}

std::string AwsSsoAuthPlugin::ResolveRegion(DBC* dbc)
{
    std::string region = MapUtils::GetStringValue(dbc->conn_attr, KEY_SSO_REGION, "");
    if (region.empty()) {
        region = MapUtils::GetStringValue(dbc->conn_attr, KEY_REGION, "");
    }
    if (region.empty()) {
        region = dbc->conn_attr.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(dbc->conn_attr.at(KEY_SERVER))
            : Aws::Region::US_EAST_1;
    }
    return region;
}

bool AwsSsoAuthPlugin::EnsureCredentials(DBC* dbc, const std::string& region, std::string& out_error)
{
    // Acquire credentials via SSO if not already resolved.
    // An interactive browser login is only attempted when the caller permitted prompting.
    if (auth_provider) {
        return true;
    }

    std::string login_error;
    const Aws::Auth::AWSCredentials credentials =
        login_util_->GetAwsCredentials(dbc->allow_interactive_auth, login_error);
    if (credentials.IsEmpty()) {
        out_error = login_error.empty()
            ? "AWS IAM Identity Center (SSO) authentication failed" : login_error;
        return false;
    }
    auth_provider = std::make_shared<AuthProvider>(region, credentials);
    return true;
}

bool AwsSsoAuthPlugin::RefreshCredentials(DBC* dbc, const std::string& region)
{
    // Refresh credentials through SSO before generating a new RDS token.
    std::string login_error;
    const Aws::Auth::AWSCredentials credentials =
        login_util_->GetAwsCredentials(dbc->allow_interactive_auth, login_error);
    if (credentials.IsEmpty()) {
        return false;
    }
    auth_provider->UpdateAwsCredential(credentials, region);
    return true;
}
