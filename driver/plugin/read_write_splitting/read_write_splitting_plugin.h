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

#ifndef READ_WRITE_SPLITTING_PLUGIN_H
#define READ_WRITE_SPLITTING_PLUGIN_H

#include "../../host_selector/host_selector.h"
#include "abstract_read_write_splitting_plugin.h"

class ReadWriteSplittingPlugin : public AbstractReadWriteSplittingPlugin {
public:
    ReadWriteSplittingPlugin(DBC* dbc);
    ReadWriteSplittingPlugin(DBC* dbc, std::shared_ptr<BasePlugin> next_plugin);

    SQLRETURN InitializeReaderConnection() override;
    SQLRETURN InitializeWriterConnection() override;
    bool ShouldUpdateWriterConnection(const HostInfo &current_host) override;
    bool ShouldUpdateReaderConnection(const HostInfo &current_host) override;
    SQLRETURN RefreshAndStoreTopology() override;
    void CloseReaderIfNecessary() override;
    SQLRETURN OpenNewReaderConnection();

    static std::shared_ptr<HostSelector> InitRwHostSelector(
        const std::map<std::string, std::string>& conn_info);

private:
    std::vector<HostInfo> hosts_;
    std::shared_ptr<HostSelector> host_selector_;
};

#endif // READ_WRITE_SPLITTING_PLUGIN_H
