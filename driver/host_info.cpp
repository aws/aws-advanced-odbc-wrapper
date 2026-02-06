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

#include "host_info.h"

HostInfo::HostInfo(std::string host, int port, HOST_STATE state, HOST_ROLE role, uint64_t weight, std::chrono::steady_clock::time_point last_update) :
    host_ { std::move(host) },
    port_ { port },
    state_ { state },
    role_ { role },
    weight_ { weight },
    last_update_ {last_update}
{
    auto idx = host_.find('.');
    if (idx != std::string::npos) {
        host_id_ = host_.substr(0, idx);
    }
}

/**
 * Returns the host.
 *
 * @return the host
 */
std::string HostInfo::GetHost() const {
    return host_;
}

/**
 * Returns the port.
 *
 * @return the port
 */
int HostInfo::GetPort() const {
    return port_;
}

/**
 * Returns the Host ID.
 *
 * @return the Host ID
 */
std::string HostInfo::GetHostId() const {
    return host_id_;
}

/**
 * Returns the weight
 *
 * @return the weight
 */
uint64_t HostInfo::GetWeight() const {
    return weight_;
}

/**
 * Returns a host:port representation of this host.
 *
 * @return the host:port representation of this host
 */
std::string HostInfo::GetHostPortPair() const {
    return host_ + HOST_PORT_SEPARATOR + std::to_string(port_);
}

HOST_STATE HostInfo::GetHostState() const {
    return state_;
}

HOST_ROLE HostInfo::GetHostRole() const {
    return role_;
}

std::chrono::steady_clock::time_point HostInfo::GetLastUpdate() const {
    return last_update_;
}

void HostInfo::SetHostState(HOST_STATE state) {
    state_ = state;
}

void HostInfo::SetHostRole(HOST_ROLE role) {
    role_ = role;
}

bool HostInfo::IsHostWriter() const {
    return role_ == WRITER;
}

bool HostInfo::IsHostUp() const {
    return state_ == UP;
}
