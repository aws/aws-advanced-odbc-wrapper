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

#include <memory>
#include <ostream>
#include <string>

typedef enum {
    UP,
    DOWN
} HOST_STATE;

class HostInfo {
public:
    static constexpr uint64_t DEFAULT_WEIGHT = 100;
    static constexpr int NO_PORT = -1;
    static constexpr int MAX_HOST_INFO_BUFFER_SIZE = 1024;

    HostInfo() = default;

    HostInfo(
        std::string host,
        int port,
        HOST_STATE state,
        bool is_writer,
        uint64_t weight = DEFAULT_WEIGHT
    );

    ~HostInfo() = default;

    bool EqualHostPortPair(const HostInfo& hi) const;
    bool IsHostDown() const;
    bool IsHostUp() const;
    bool IsHostWriter() const;

    void MarkAsWriter(bool writer);

    std::string GetHost() const;
    int GetPort() const;
    std::string GetHostId() const;
    std::string GetHostPortPair() const;
    uint64_t GetWeight() const;

    void SetHostState(HOST_STATE state);
    HOST_STATE GetHostState() const;

    bool operator==(const HostInfo& other) const {
        return this->EqualHostPortPair(other) && this->weight == other.GetWeight() && this->is_writer == other.IsHostWriter();
    }

private:
    std::string host_port_separator = ":";
    std::string host_;
    std::string host_id_;
    int port = NO_PORT;
    uint64_t weight = DEFAULT_WEIGHT;

    HOST_STATE host_state;
    bool is_writer;
};

inline std::ostream& operator<<(std::ostream& str, const HostInfo& v) {
    char buf[HostInfo::MAX_HOST_INFO_BUFFER_SIZE];
    sprintf(buf, "HostInfo[host=%s, port=%d, %s]", v.GetHost().c_str(), v.GetPort(), v.IsHostWriter() ? "WRITER" : "READER");
    return str << std::string(buf);
}

#endif // HOST_INFO_H_
