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

#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/rds/RDSClient.h>
#include <aws/rds/RDSErrors.h>
#include <aws/rds/model/DBClusterEndpoint.h>
#include <aws/rds/model/DescribeDBClusterEndpointsRequest.h>

#include "custom_endpoint_monitor.h"

#include "../../util/aws_sdk_helper.h"
#include "../../util/logger_wrapper.h"
#include "../../util/rds_utils.h"

#include <chrono>

SlidingCacheMap<std::string, HostFilter> CustomEndpointMonitor::endpoint_cache;

CustomEndpointMonitor::CustomEndpointMonitor(
    const std::shared_ptr<TopologyService>& topology_service,
    const std::string& endpoint,
    std::string region,
    std::chrono::milliseconds refresh_rate_ms,
    std::chrono::milliseconds max_refresh_rate_ms,
    int exponential_backoff_rate)
    : topology_service_{ topology_service },
      endpoint_{ endpoint },
      endpoint_identifier_{ RdsUtils::GetRdsClusterId(endpoint) },
      region_{ std::move(region) },
      refresh_rate_ms_{ refresh_rate_ms },
      min_refresh_rate_ms_{ refresh_rate_ms },
      max_refresh_rate_ms_{ max_refresh_rate_ms },
      exponential_backoff_rate_{ exponential_backoff_rate }
{
    AwsSdkHelper::Init();
    is_running_.store(true);
    this->monitoring_thread_ = std::make_shared<std::thread>(&CustomEndpointMonitor::Run, this);
}

CustomEndpointMonitor::~CustomEndpointMonitor() {
    is_running_.store(false);
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    monitoring_thread_ = nullptr;
    AwsSdkHelper::Shutdown();
}

void CustomEndpointMonitor::Run() {
    try {
        Aws::RDS::RDSClientConfiguration client_config;
        if (!region_.empty()) {
            client_config.region = region_;
        }
        const Aws::RDS::RDSClient rds_client(
            Aws::Auth::DefaultAWSCredentialsProviderChain().GetAWSCredentials(),
            client_config
        );

        Aws::RDS::Model::DescribeDBClusterEndpointsRequest request;
        request.SetDBClusterEndpointIdentifier(this->endpoint_identifier_);

        while (is_running_.load()) {
            const std::chrono::time_point start = std::chrono::steady_clock::now();
            const auto response = rds_client.DescribeDBClusterEndpoints(request);
            if (response.IsSuccess()) {
                const auto custom_endpoints = response.GetResult().GetDBClusterEndpoints();
                if (custom_endpoints.size() != 1) {
                    LOG(WARNING)  << "Unexpected number of custom endpoints with endpoint identifier " << endpoint_identifier_
                        << " in region " << region_ << ". Expected 1 custom endpoint, but found " << custom_endpoints.size();
                    std::this_thread::sleep_for(refresh_rate_ms_);
                    continue;
                }
                const auto& endpoint_info = custom_endpoints.front();
                HostFilter filter;
                // Both static and excluded flag can be set to true
                // at the same time despite only able to set one group
                if (endpoint_info.StaticMembersHasBeenSet()) {
                    for (const auto& host : endpoint_info.GetStaticMembers()) {
                        filter.allowed_host_ids.insert(host);
                    }
                }
                if (endpoint_info.ExcludedMembersHasBeenSet()) {
                    for (const auto& host : endpoint_info.GetExcludedMembers()) {
                        filter.blocked_host_ids.insert(host);
                    }
                }

                const HostFilter cached_filter = endpoint_cache.Get(endpoint_identifier_);
                if (cached_filter != filter) {
                    LOG(INFO) << "Detected change in custom endpoint info for " << endpoint_identifier_;
                    this->topology_service_->SetHostFilter(filter);
                    endpoint_cache.Put(this->endpoint_identifier_, filter);
                    DecreaseDelay();
                }
            } else {
                const Aws::RDS::RDSError& err = response.GetError();
                LOG(ERROR) << "Custom Endpoint Monitor encountered error with RDS Client Describe DB Endpoints: " << err.GetMessage();
                if (err.ShouldThrottle()) {
                    IncreaseDelay();
                } else if (const auto& http_code = err.GetResponseCode();
                    http_code == Aws::Http::HttpResponseCode::UNAUTHORIZED
                    || http_code == Aws::Http::HttpResponseCode::FORBIDDEN)
                {
                    refresh_rate_ms_ = UNAUTHORIZED_SLEEP_DIR;
                }
            }

            const auto elapsed_time = std::chrono::steady_clock::now() - start;
            const auto sleep_dir = (refresh_rate_ms_ - elapsed_time).count() > 0 ?
                (refresh_rate_ms_ - elapsed_time).count() : 0;
            std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_dir));
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Custom Endpoint Monitor encountered error: " << ex.what();
    }
    is_running_.store(false);
}

bool CustomEndpointMonitor::HasInfo() {
    return endpoint_cache.Get(endpoint_identifier_) != HostFilter{};
}

void CustomEndpointMonitor::IncreaseDelay() {
    const std::chrono::milliseconds updated_rate = refresh_rate_ms_ * exponential_backoff_rate_;
    if (updated_rate < max_refresh_rate_ms_) {
        refresh_rate_ms_ = updated_rate;
    }
}

void CustomEndpointMonitor::DecreaseDelay() {
    const std::chrono::milliseconds updated_rate = refresh_rate_ms_ / exponential_backoff_rate_;
    if (updated_rate > min_refresh_rate_ms_) {
        // Clamp back to max refresh rate after unauthorized refresh rate was hit
        refresh_rate_ms_ = updated_rate > max_refresh_rate_ms_ ? max_refresh_rate_ms_ : updated_rate;
    }
}
