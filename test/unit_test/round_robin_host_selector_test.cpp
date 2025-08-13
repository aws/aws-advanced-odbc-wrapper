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

#include "../../driver/host_selector/round_robin_host_selector.h"

#include "../../driver/host_info.h"

#include <unordered_map>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
    const int base_port = 1234;
    HostInfo writer_host_info_a("writer", base_port, HOST_STATE::UP, true, 1);
    HostInfo reader_host_info_a("reader_a", base_port, HOST_STATE::UP, false, 1);
    HostInfo reader_host_info_b("reader_b", base_port, HOST_STATE::UP, false, 1);
    HostInfo reader_host_info_c("reader_c", base_port, HOST_STATE::UP, false, 1);
    HostInfo reader_host_info_down("reader_down", base_port, HOST_STATE::DOWN, false, 1);
}

class RoundRobinHostSelectorTest : public testing::Test {
  protected:
    // Runs once per suite
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    // Runs per test case
    void SetUp() override {
        RoundRobinHostSelector::ClearCache();
    }
    void TearDown() override {}
};

TEST_F(RoundRobinHostSelectorTest, GetWriter) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::vector<HostInfo> hosts = {writer_host_info_a, reader_host_info_a, reader_host_info_b};
    HostInfo host_info = host_selector.GetHost(hosts, true, props);
    EXPECT_EQ(writer_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, get_writer_fail_all_readers) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, true, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, get_reader_one_down) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::vector<HostInfo> hosts = {reader_host_info_down, reader_host_info_a};
    HostInfo host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, no_hosts) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::vector<HostInfo> hosts = {};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, get_first_reader) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b, reader_host_info_c};
    HostInfo host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, get_first_reader_unsorted_input) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::vector<HostInfo> hosts = {reader_host_info_c, reader_host_info_a, reader_host_info_b};
    HostInfo host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, get_round_robin_readers_default) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};

    HostInfo host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_b.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, get_round_robin_readers_weighted) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":2"
         + "," + reader_host_info_b.GetHost() + ":1"
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};

    HostInfo host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_b.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, get_round_robin_readers_weighted_change) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":2"
         + "," + reader_host_info_b.GetHost() + ":1"
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};

    HostInfo host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_b.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());

    std::unordered_map<std::string, std::string> updated_props;
    updated_props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":1"
         + "," + reader_host_info_b.GetHost() + ":2"
    );

    host_info = host_selector.GetHost(hosts, false, updated_props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, updated_props);
    EXPECT_EQ(reader_host_info_b.GetHost(), host_info.GetHost());
    host_info = host_selector.GetHost(hosts, false, updated_props);
    EXPECT_EQ(reader_host_info_b.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, updated_props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, set_round_robin_weight) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    std::string expected_weight_prop(
        reader_host_info_a.GetHost() + ":" + std::to_string(reader_host_info_a.GetWeight())
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    RoundRobinHostSelector::SetRoundRobinWeight(hosts, props);

    std::string weight_prop = props.at(round_robin_property::HOST_WEIGHT_KEY);
    EXPECT_STREQ(expected_weight_prop.c_str(), weight_prop.c_str());

    HostInfo host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_b.GetHost(), host_info.GetHost());

    host_info = host_selector.GetHost(hosts, false, props);
    EXPECT_EQ(reader_host_info_a.GetHost(), host_info.GetHost());
}

TEST_F(RoundRobinHostSelectorTest, invalid_host_weight_empty_host) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        ":" + std::to_string(reader_host_info_a.GetWeight())
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_host_weight_empty_weight) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":"
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_host_weight_zero_weight) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":0"
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_host_weight_negative_weight) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":-1"
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_host_weight_float_weight) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":1.1"
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_host_weight_non_int_weight) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":one"
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_host_weight_format) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::HOST_WEIGHT_KEY] = std::string(
        reader_host_info_a.GetHost() + ":111:" + std::to_string(reader_host_info_a.GetWeight())
         + "," + reader_host_info_b.GetHost() + ":" + std::to_string(reader_host_info_b.GetWeight())
    );
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_default_zero) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::DEFAULT_WEIGHT_KEY] = std::string("0");
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_default_negative) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::DEFAULT_WEIGHT_KEY] = std::string("-100");
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_default_float) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::DEFAULT_WEIGHT_KEY] = std::string("1.1");
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}

TEST_F(RoundRobinHostSelectorTest, invalid_default_non_int) {
    RoundRobinHostSelector host_selector;
    std::unordered_map<std::string, std::string> props;
    props[round_robin_property::DEFAULT_WEIGHT_KEY] = std::string("one");
    std::vector<HostInfo> hosts = {reader_host_info_a, reader_host_info_b};
    EXPECT_THROW(host_selector.GetHost(hosts, false, props), std::runtime_error);
}
