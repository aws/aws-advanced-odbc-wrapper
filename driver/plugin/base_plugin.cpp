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

#include "base_plugin.h"

#include "../driver.h"
#include "../util/logger_wrapper.h"

namespace {

template <typename HandleT>
SQLRETURN ChainDelegationError(HandleT* handle, const std::string& plugin_name, const char* operation) {
    LOG(ERROR) << "[" << plugin_name << "] Plugin chain is misconfigured: no valid next plugin to delegate "
               << operation << " to";
    if (!handle) {
        return SQL_INVALID_HANDLE;
    }
    const std::lock_guard<std::recursive_mutex> lock_guard(handle->lock);
    ClearError(handle);
    const std::string error_msg = "The plugin chain is misconfigured: ["
        + plugin_name + "] has no valid next plugin to delegate " + operation + " to.";
    handle->err = std::make_unique<ERR_INFO>(error_msg.c_str(), ERR_GENERAL_ERROR);
    return SQL_ERROR;
}

}  // namespace

BasePlugin::BasePlugin(DBC *dbc) : BasePlugin(dbc, nullptr) {}

BasePlugin::BasePlugin(DBC *dbc, std::shared_ptr<BasePlugin> next_plugin) :
    next_plugin(next_plugin),
    plugin_name("BasePlugin") {}

BasePlugin::~BasePlugin() = default;

// codechecker_suppress [misc-no-recursion]
SQLRETURN BasePlugin::Connect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    return ConnectNext(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion
    );
}

// codechecker_suppress [misc-no-recursion]
SQLRETURN BasePlugin::Execute(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    return ExecuteNext(StatementHandle, StatementText, TextLength);
}

// codechecker_suppress [misc-no-recursion]
SQLRETURN BasePlugin::ConnectNext(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLUSMALLINT   DriverCompletion)
{
    if (!next_plugin || next_plugin.get() == this) {
        return ChainDelegationError(static_cast<DBC*>(ConnectionHandle), plugin_name, "Connect");
    }
    return next_plugin->Connect(
        ConnectionHandle,
        WindowHandle,
        OutConnectionString,
        BufferLength,
        StringLengthPtr,
        DriverCompletion
    );
}

// codechecker_suppress [misc-no-recursion]
SQLRETURN BasePlugin::ExecuteNext(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    if (!next_plugin || next_plugin.get() == this) {
        return ChainDelegationError(static_cast<STMT*>(StatementHandle), plugin_name, "Execute");
    }
    return next_plugin->Execute(StatementHandle, StatementText, TextLength);
}

// codechecker_suppress [misc-no-recursion]
void BasePlugin::ReleaseResources() {
    if (next_plugin && next_plugin.get() != this) {
        next_plugin->ReleaseResources();
    }
}
