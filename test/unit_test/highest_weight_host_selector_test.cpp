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

#include "../../driver/host_selector/highest_weight_host_selector.h"

#include "../../driver/host_info.h"

#include <unordered_map>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
    constexpr int base_port = 1234;
    HostInfo reader_host_info_a("reader_a", base_port, UP, READER, HostInfo::DEFAULT_WEIGHT - 2);
    HostInfo reader_host_info_b("reader_b", base_port, UP, READER, HostInfo::DEFAULT_WEIGHT - 1);
    HostInfo reader_host_info_c("reader_c", base_port, UP, READER, HostInfo::DEFAULT_WEIGHT - 1);
    HostInfo reader_host_info_down_a("reader_down_a", base_port, DOWN, READER, HostInfo::DEFAULT_WEIGHT);
    HostInfo reader_host_info_down_b("reader_down_b", base_port, DOWN, READER, HostInfo::DEFAULT_WEIGHT + 1);
    HostInfo writer_host_info_a("writer_a", base_port, UP, WRITER, HostInfo::DEFAULT_WEIGHT - 2);
    HostInfo writer_host_info_b("writer_b", base_port, UP, WRITER, HostInfo::DEFAULT_WEIGHT - 1);
    HostInfo writer_host_info_c("writer_c", base_port, UP, WRITER, HostInfo::DEFAULT_WEIGHT - 1);
    HostInfo writer_host_info_down_a("writer_down_a", base_port, DOWN, WRITER, HostInfo::DEFAULT_WEIGHT);
    HostInfo writer_host_info_down_b("writer_down_b", base_port, DOWN, WRITER, HostInfo::DEFAULT_WEIGHT + 1);
    std::vector<HostInfo> all_hosts = {
        reader_host_info_a,
        reader_host_info_b,
        reader_host_info_c,
        reader_host_info_down_a,
        reader_host_info_down_b,
        writer_host_info_a,
        writer_host_info_b,
        writer_host_info_c,
        writer_host_info_down_a,
        writer_host_info_down_b
    };
    std::unordered_map<std::string, std::string> empty_map;
}

class HighestWeightHostSelectorTest : public testing::Test {
protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    // Runs per test case
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HighestWeightHostSelectorTest, get_highest_reader) {
    HighestWeightHostSelector host_selector;
    HostInfo host_info = host_selector.GetHost(all_hosts, false, empty_map);
    EXPECT_EQ(reader_host_info_b.GetHost(), host_info.GetHost());
}

TEST_F(HighestWeightHostSelectorTest, get_highest_writer) {
    HighestWeightHostSelector host_selector;
    HostInfo host_info = host_selector.GetHost(all_hosts, true, empty_map);
    EXPECT_EQ(writer_host_info_b.GetHost(), host_info.GetHost());
}

TEST_F(HighestWeightHostSelectorTest, get_reader_fail_all_writers) {
    HighestWeightHostSelector host_selector;
    std::vector<HostInfo> hosts = {
        writer_host_info_a,
        writer_host_info_b,
        writer_host_info_c,
        writer_host_info_down_a
    };
    EXPECT_THROW(host_selector.GetHost(hosts, false, empty_map), std::runtime_error);
}

TEST_F(HighestWeightHostSelectorTest, get_writer_fail_all_readers) {
    HighestWeightHostSelector host_selector;
    std::vector<HostInfo> hosts = {
        reader_host_info_a,
        reader_host_info_b,
        reader_host_info_c,
        reader_host_info_down_a
    };
    EXPECT_THROW(host_selector.GetHost(hosts, true, empty_map), std::runtime_error);
}

TEST_F(HighestWeightHostSelectorTest, get_reader_all_down) {
    HighestWeightHostSelector host_selector;
    std::vector<HostInfo> hosts = {
        reader_host_info_down_a,
        reader_host_info_down_b
    };
    EXPECT_THROW(host_selector.GetHost(hosts, false, empty_map), std::runtime_error);
}

TEST_F(HighestWeightHostSelectorTest, get_writer_all_down) {
    HighestWeightHostSelector host_selector;
    std::vector<HostInfo> hosts = {
        writer_host_info_down_a,
        writer_host_info_down_b
    };
    EXPECT_THROW(host_selector.GetHost(hosts, true, empty_map), std::runtime_error);
}

TEST_F(HighestWeightHostSelectorTest, get_reader_no_hosts) {
    HighestWeightHostSelector host_selector;
    std::vector<HostInfo> hosts = {};
    EXPECT_THROW(host_selector.GetHost(hosts, false, empty_map), std::runtime_error);
}

TEST_F(HighestWeightHostSelectorTest, get_writer_no_hosts) {
    HighestWeightHostSelector host_selector;
    std::vector<HostInfo> hosts = {};
    EXPECT_THROW(host_selector.GetHost(hosts, true, empty_map), std::runtime_error);
}
