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

#ifndef TOXIPROXY_HELPER_H_
#define TOXIPROXY_HELPER_H_

#include <memory>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <toxiproxy/toxiproxy_client.h>
#include <toxiproxy/toxiproxy_http_client.h>

#include "test_utils.h"

class ToxiproxyHelper {
public:
    int ListenPort() const {
        return listen_port_;
    }

    // Replaces any proxy left behind by a previous run, then creates one that forwards to the upstream.
    // Throws std::exception if the Toxiproxy server is unreachable or rejects the request.
    void CreateProxy(const std::string& upstream_host, const int upstream_port) {
        http_client_ = std::make_unique<TOXIPROXY::TOXIPROXY_HTTP_CLIENT>("http", host_, port_);
        http_client_->delete_path(ProxyPath());

        // The client returns an empty string value instead of throwing when the server cannot be reached.
        const auto created = http_client_->post("/proxies", nlohmann::json{
            {"name", PROXY_NAME},
            {"listen", "127.0.0.1:" + std::to_string(listen_port_)},
            {"upstream", upstream_host + ":" + std::to_string(upstream_port)}});
        if (!created.is_object() || created.contains("error")) {
            throw std::runtime_error("Toxiproxy create proxy failed: " + created.dump());
        }
        proxy_created_ = true;

        // Looking up an existing proxy is a plain GET, which the library handles correctly.
        api_client_ = std::make_unique<TOXIPROXY::TOXIPROXY_CLIENT>(host_, port_);
        proxy_ = api_client_->get_proxy(PROXY_NAME);
    }

    // Reset every connection through the proxy, in both directions, including new ones, as a network failure would.
    void DisableConnectivity() const {
        const auto toxics = proxy_->get_toxics();
        const std::unique_ptr<TOXIPROXY::RESET_PEER> downstream(
            toxics->reset_peer(DOWNSTREAM_TOXIC, TOXIPROXY::TOXIC_DIRECTION::DOWNSTREAM, 0));
        const std::unique_ptr<TOXIPROXY::RESET_PEER> upstream(
            toxics->reset_peer(UPSTREAM_TOXIC, TOXIPROXY::TOXIC_DIRECTION::UPSTREAM, 0));
    }

    void EnableConnectivity() const {
        const auto toxics = proxy_->get_toxics();
        for (const char* name : {DOWNSTREAM_TOXIC, UPSTREAM_TOXIC}) {
            const std::unique_ptr<TOXIPROXY::TOXIC> toxic(toxics->get(name));
            if (toxic) {
                toxic->remove();
            }
        }
    }

    // Deleting the proxy also removes its toxics. Safe to call when no proxy was created. Never throws.
    void DeleteProxy() noexcept {
        try {
            if (http_client_ && proxy_created_) {
                http_client_->delete_path(ProxyPath());
                proxy_created_ = false;
            }
        } catch (const std::exception& e) {
            std::cout << "[Toxiproxy] Cleanup failed: " << e.what() << std::endl;
        }
    }

private:
    static constexpr const char* PROXY_NAME = "writer";
    static constexpr const char* UPSTREAM_TOXIC = "UPSTREAM";
    static constexpr const char* DOWNSTREAM_TOXIC = "DOWNSTREAM";

    static std::string ProxyPath() {
        return std::string("/proxies/") + PROXY_NAME;
    }

    std::string host_ = TEST_UTILS::GetEnvVar("TOXIPROXY_HOST", "localhost");
    int port_ = TEST_UTILS::StrToInt(TEST_UTILS::GetEnvVar("TOXIPROXY_PORT", "8474"));
    int listen_port_ = TEST_UTILS::StrToInt(TEST_UTILS::GetEnvVar("TOXIPROXY_LISTEN_PORT", "15432"));

    std::unique_ptr<TOXIPROXY::TOXIPROXY_HTTP_CLIENT> http_client_;  // Raw HTTP client, creates and deletes the proxy.
    std::unique_ptr<TOXIPROXY::TOXIPROXY_CLIENT> api_client_;        // Library client, looks up the proxy and its toxics.
    TOXIPROXY::PROXY* proxy_ = nullptr;                              // Owned by api_client_, do not delete.
    bool proxy_created_ = false;
};

#endif  // TOXIPROXY_HELPER_H_
