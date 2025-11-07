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

#ifdef WIN32
    #include <windows.h>
#endif

#include <regex>
#include <string>

#include "rds_strings.h"
#include "rds_utils.h"

namespace {
    const std::regex AURORA_DNS_PATTERN(
        R"#((.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-|shardgrp-)?([a-zA-Z0-9]+\.([a-zA-Z0-9\-]+)\.rds\.amazonaws\.com))#",
        std::regex_constants::icase);
    const std::regex AURORA_PROXY_DNS_PATTERN(R"#((.+)\.(proxy-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#", std::regex_constants::icase);
    const std::regex AURORA_CLUSTER_PATTERN(R"#((.+)\.(cluster-|cluster-ro-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
                                            std::regex_constants::icase);
    const std::regex AURORA_WRITER_CLUSTER_PATTERN(R"#((.+)\.(cluster-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
                                                   std::regex_constants::icase);
    const std::regex AURORA_READER_CLUSTER_PATTERN(R"#((.+)\.(cluster-ro-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
                                                   std::regex_constants::icase);
    const std::regex AURORA_CUSTOM_CLUSTER_PATTERN(R"#((.+)\.(cluster-custom-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
                                                   std::regex_constants::icase);
    const std::regex AURORA_LIMITLESS_CLUSTER_PATTERN(R"#((.+)\.(shardgrp-)+([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.(amazonaws\.com|amazonaws\.com\.cn\.?|sc2s\.sgov\.gov\.?|c2s\.ic\.gov\.?)))#",
                                                   std::regex_constants::icase);
    const std::regex AURORA_CHINA_DNS_PATTERN(
        R"#((.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-|shardgrp-)?([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
        std::regex_constants::icase);
    const std::regex AURORA_CHINA_PROXY_DNS_PATTERN(
        R"#((.+)\.(proxy-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#", std::regex_constants::icase);
    const std::regex AURORA_CHINA_CLUSTER_PATTERN(
        R"#((.+)\.(cluster-|cluster-ro-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#",
        std::regex_constants::icase);
    const std::regex AURORA_CHINA_WRITER_CLUSTER_PATTERN(
        R"#((.+)\.(cluster-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#", std::regex_constants::icase);
    const std::regex AURORA_CHINA_READER_CLUSTER_PATTERN(
        R"#((.+)\.(cluster-ro-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#", std::regex_constants::icase);
    const std::regex AURORA_CHINA_CUSTOM_CLUSTER_PATTERN(
        R"#((.+)\.(cluster-custom-)+([a-zA-Z0-9]+\.(rds\.[a-zA-Z0-9\-]+|[a-zA-Z0-9\-]+\.rds)\.amazonaws\.com\.cn))#", std::regex_constants::icase);
    const std::regex IPV4_PATTERN(
        R"#(^(([1-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){1}(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){2}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$)#");
    const std::regex IPV6_PATTERN(R"#(^[0-9a-fA-F]{1,4}(:[0-9a-fA-F]{1,4}){7}$)#");
    const std::regex IPV6_COMPRESSED_PATTERN(R"#(^(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)::(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)$)#");
} // anonymous namespace

bool RdsUtils::IsDnsPatternValid(const std::string& host) {
    return ( host.find('?') != std::string::npos);
}

bool RdsUtils::IsRdsDns(const std::string& host) {
    return std::regex_match(host, AURORA_DNS_PATTERN) || std::regex_match(host, AURORA_CHINA_DNS_PATTERN);
}

bool RdsUtils::IsRdsClusterDns(const std::string& host) {
    return std::regex_match(host, AURORA_CLUSTER_PATTERN) || std::regex_match(host, AURORA_CHINA_CLUSTER_PATTERN);
}

bool RdsUtils::IsRdsProxyDns(const std::string& host) {
    return std::regex_match(host, AURORA_PROXY_DNS_PATTERN) || std::regex_match(host, AURORA_CHINA_PROXY_DNS_PATTERN);
}

bool RdsUtils::IsRdsWriterClusterDns(const std::string& host) {
    return std::regex_match(host, AURORA_WRITER_CLUSTER_PATTERN) || std::regex_match(host, AURORA_CHINA_WRITER_CLUSTER_PATTERN);
}

bool RdsUtils::IsRdsReaderClusterDns(const std::string& host) {
    return std::regex_match(host, AURORA_READER_CLUSTER_PATTERN) || std::regex_match(host, AURORA_CHINA_READER_CLUSTER_PATTERN);
}

bool RdsUtils::IsRdsCustomClusterDns(const std::string& host) {
    return std::regex_match(host, AURORA_CUSTOM_CLUSTER_PATTERN) || std::regex_match(host, AURORA_CHINA_CUSTOM_CLUSTER_PATTERN);
}

bool RdsUtils::IsLimitlessDbShardGroupDns(const std::string& host) {
    return std::regex_match(host, AURORA_LIMITLESS_CLUSTER_PATTERN);
}

std::string RdsUtils::GetRdsClusterHostUrl(const std::string& host) {
    auto f = [ host ](const std::regex& pattern) {
        std::smatch m;
        if (std::regex_search(host, m, pattern) && m.size() > 1) {
            const std::string gr1 = m.size() > 1 ? m.str(1) : std::string("");
            const std::string gr2 = m.size() > 2 ? m.str(2) : std::string("");
            const std::string gr3 = m.size() > 3 ? m.str(3) : std::string("");
            if (!gr1.empty() && !gr3.empty() &&
                (STR_ICMP(gr2.c_str(), "cluster-") == 0 || STR_ICMP(gr2.c_str(), "cluster-ro-") == 0)) {
                std::string result;
                result.assign(gr1);
                result.append(".cluster-");
                result.append(gr3);

                return result;
            }
        }
        return std::string();
    };

    auto result = f(AURORA_CLUSTER_PATTERN);
    if (!result.empty()) {
        return result;
    }

    return f(AURORA_CHINA_CLUSTER_PATTERN);
}

std::string RdsUtils::GetRdsClusterId(const std::string& host) {
    auto f = [ host ](const std::regex& pattern) {
        std::smatch m;
        if (std::regex_search(host, m, pattern) && m.size() > 1 && !m.str(2).empty()) {
            return m.str(1);
        }
        return std::string();
    };

    auto result = f(AURORA_DNS_PATTERN);
    if (!result.empty()) {
        return result;
    }

    return f(AURORA_CHINA_DNS_PATTERN);
}

std::string RdsUtils::GetRdsInstanceId(const std::string& host) {
    auto f = [ host ](const std::regex& pattern) {
        std::smatch m;
        if (std::regex_search(host, m, pattern) && m.size() > 1 && m.str(2).empty()) {
            return m.str(1);
        }
        return std::string();
    };

    auto result = f(AURORA_DNS_PATTERN);
    if (!result.empty()) {
        return result;
    }

    return f(AURORA_CHINA_DNS_PATTERN);
}

std::string RdsUtils::GetRdsInstanceHostPattern(const std::string& host) {
    auto f = [ host ](const std::regex& pattern) {
        std::smatch m;
        if (std::regex_search(host, m, pattern) && m.size() > 4 && !m.str(3).empty()) {
            std::string result("?.");
            result.append(m.str(3));

            return result;
        }
        return std::string();
    };

    auto result = f(AURORA_DNS_PATTERN);
    if (!result.empty()) {
        return result;
    }

    return f(AURORA_CHINA_DNS_PATTERN);
}

std::string RdsUtils::GetRdsRegion(const std::string& host) {
    auto f = [ host ](const std::regex& pattern) {
        std::smatch m;
        if (std::regex_search(host, m, pattern) && m.size() > 4 && !m.str(4).empty()) {
            return m.str(4);
        }
        return std::string();
    };

    auto result = f(AURORA_DNS_PATTERN);
    if (!result.empty()) {
        return result;
    }

    return f(AURORA_CHINA_DNS_PATTERN);
}

bool RdsUtils::IsIpv4(const std::string& host) {
    return std::regex_match(host, IPV4_PATTERN);
}

bool RdsUtils::IsIpv6(const std::string& host) {
    return std::regex_match(host, IPV6_PATTERN) || std::regex_match(host, IPV6_COMPRESSED_PATTERN);
}
