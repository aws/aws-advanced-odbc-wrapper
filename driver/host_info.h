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

#ifndef HOST_INFO_H_
#define HOST_INFO_H_

#include <chrono>
#include <memory>
#include <ostream>
#include <string>

typedef enum {
    UP,
    DOWN
} HOST_STATE;

typedef enum {
    READER,
    WRITER,
    UNKNOWN
} HOST_ROLE;

class HostInfo {
public:
    static constexpr uint64_t DEFAULT_WEIGHT = 100;
    static constexpr int NO_PORT = -1;
    static constexpr int MAX_HOST_INFO_BUFFER_SIZE = 1024;

    HostInfo() = default;

    HostInfo(
        std::string host,
        int port = NO_PORT,
        HOST_STATE state = DOWN,
        HOST_ROLE role = UNKNOWN,
        uint64_t weight = DEFAULT_WEIGHT,
        std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now()
    );

    ~HostInfo() = default;

    std::string GetHost() const;
    int GetPort() const;
    std::string GetHostId() const;
    std::string GetHostPortPair() const;
    uint64_t GetWeight() const;
    HOST_STATE GetHostState() const;
    HOST_ROLE GetHostRole() const;
    std::chrono::steady_clock::time_point GetLastUpdate() const;

    void SetHostState(HOST_STATE state);
    void SetHostRole(HOST_ROLE state);

    bool IsHostWriter() const;
    bool IsHostUp() const;

    bool operator==(const HostInfo& other) const {
        return this->host_ == other.GetHost()
            && this->port_ == other.GetPort()
            && this->weight_ == other.GetWeight()
            && this->state_ == other.GetHostState()
            && this->role_ == other.GetHostRole();
    }

private:
    static constexpr char HOST_PORT_SEPARATOR = ':';
    std::string host_;
    std::string host_id_;
    int port_ = NO_PORT;
    uint64_t weight_ = DEFAULT_WEIGHT;

    HOST_ROLE role_;
    HOST_STATE state_;

    std::chrono::steady_clock::time_point last_update_;
};

inline std::ostream& operator<<(std::ostream& str, const HostInfo& v) {
    char buf[HostInfo::MAX_HOST_INFO_BUFFER_SIZE];
    sprintf(buf, "HostInfo[host=%s, port=%d, %s]",
        v.GetHost().c_str(),
        v.GetPort(),
        v.GetHostRole() == WRITER ? "WRITER" : "READER");
    return str << std::string(buf);
}

#endif // HOST_INFO_H_
