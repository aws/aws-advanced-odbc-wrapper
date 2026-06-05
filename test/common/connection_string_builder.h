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

#ifndef CONNECTION_STRING_BUILDER_H_
#define CONNECTION_STRING_BUILDER_H_

#include <string>

class ConnectionStringBuilder {
public:
    ConnectionStringBuilder(const std::string& dsn, const std::string& server, int port) {
        conn_str_ = "DSN=" + dsn + ";SERVER=" + server + ";PORT=" + std::to_string(port) + ";SSLMODE=prefer;COMMLOG=1;DEBUG=1;LOGDIR=logs/;";
    }

    ConnectionStringBuilder(const std::string& str) : conn_str_(str) {}

    ConnectionStringBuilder& withDSN(const std::string& dsn) {
        conn_str_ += "DSN=" + dsn + ";";
        return *this;
    }

    ConnectionStringBuilder& withServer(const std::string& server) {
        conn_str_ += "SERVER=" + server + ";";
        return *this;
    }

    ConnectionStringBuilder& withPort(const int port) {
        conn_str_ += "PORT=" + std::to_string(port) + ";";
        return *this;
    }

    ConnectionStringBuilder& withUID(const std::string& uid) {
        conn_str_ += "UID=" + uid + ";";
        return *this;
    }

    ConnectionStringBuilder& withPWD(const std::string& pwd) {
        conn_str_ += "PWD=" + pwd + ";";
        return *this;
    }

    ConnectionStringBuilder& withDatabase(const std::string& db) {
        conn_str_ += "DATABASE=" + db + ";";
        return *this;
    }

    ConnectionStringBuilder& withBaseDriver(const std::string& driver) {
        conn_str_ += "BASE_DRIVER=" + driver + ";";
        return *this;
    }

    ConnectionStringBuilder& withBaseDSN(const std::string& dsn) {
        conn_str_ += "BASE_DSN=" + dsn + ";";
        return *this;
    }

    ConnectionStringBuilder& withDatabaseDialect(const std::string& dialect) {
        conn_str_ += "DATABASE_DIALECT=" + dialect + ";";
        return *this;
    }

    ConnectionStringBuilder& withEnableRwSplitting(const bool& enable_rw_splitting) {
        conn_str_ += "ENABLE_RW_SPLIT=" + std::to_string(enable_rw_splitting ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withEnableSrwSplitting(const bool& enable_srw_splitting, const std::string& write_endpoint, const std::string& read_endpoint, const bool verify) {
        conn_str_ += "ENABLE_SRW_SPLIT=" + std::to_string(enable_srw_splitting ? 1 : 0) + ";";
        conn_str_ += "SRW_READ_ENDPOINT=" + read_endpoint + ";";
        conn_str_ += "SRW_WRITE_ENDPOINT=" + write_endpoint + ";";
        conn_str_ += "SRW_VERIFY_CONNS=" + std::to_string(verify ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withEnableClusterFailover(const bool& enable_cluster_failover) {
        conn_str_ += "ENABLE_CLUSTER_FAILOVER=" + std::to_string(enable_cluster_failover ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withFailoverMode(const std::string& failover_mode) {
        conn_str_ += "FAILOVER_MODE=" + failover_mode + ";";
        return *this;
    }

    ConnectionStringBuilder& withReaderHostSelectorStrategy(const std::string& strategy) {
        conn_str_ += "HOST_SELECTOR_STRATEGY=" + strategy + ";";
        return *this;
    }

    ConnectionStringBuilder& withIgnoreTopologyRequest(const int& ignore_topology_request) {
        conn_str_ += "IGNORE_TOPOLOGY_REQUEST_MS=" + std::to_string(ignore_topology_request) + ";";
        return *this;
    }

    ConnectionStringBuilder& withTopologyHighRefreshRate(const int& topology_high_refresh_rate) {
        conn_str_ += "TOPOLOGY_HIGH_REFRESH_RATE_MS=" + std::to_string(topology_high_refresh_rate) + ";";
        return *this;
    }

    ConnectionStringBuilder& withTopologyRefreshRate(const int& topology_refresh_rate) {
        conn_str_ += "TOPOLOGY_REFRESH_RATE_MS=" + std::to_string(topology_refresh_rate) + ";";
        return *this;
    }

    ConnectionStringBuilder& withFailoverTimeout(const int& failover_t) {
        conn_str_ += "FAILOVER_TIMEOUT_MS=" + std::to_string(failover_t) + ";";
        return *this;
    }

    ConnectionStringBuilder& withHostPattern(const std::string& host_pattern) {
        conn_str_ += "HOST_PATTERN=" + host_pattern + ";";
        return *this;
    }

    ConnectionStringBuilder& withAuthMode(const std::string& auth_mode) {
        conn_str_ += "RDS_AUTH_TYPE=" + auth_mode + ";";
        return *this;
    }

    ConnectionStringBuilder& withAuthRegion(const std::string& auth_region) {
        conn_str_ += "REGION=" + auth_region + ";";
        return *this;
    }

    ConnectionStringBuilder& withAuthExpiration(const int& auth_expiration) {
        conn_str_ += "TOKEN_EXPIRATION=" + std::to_string(auth_expiration) + ";";
        return *this;
    }

    ConnectionStringBuilder& withIamHost(const std::string& iam_host) {
        conn_str_ += "IAM_HOST=" + iam_host + ";";
        return *this;
    }

    ConnectionStringBuilder& withSecretId(const std::string& secret_id) {
        conn_str_ += "SECRET_ID=" + secret_id + ";";
        return *this;
    }

    ConnectionStringBuilder& withLimitlessEnabled(const bool& limitless_enabled) {
        conn_str_ += "ENABLE_LIMITLESS=" + std::to_string(limitless_enabled ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withLimitlessMode(const std::string& limitless_mode) {
        conn_str_ += "LIMITLESS_MODE=" + limitless_mode + ";";
        return *this;
    }

    ConnectionStringBuilder& withLimitlessMonitorIntervalMs(const int& interval) {
        conn_str_ += "LIMITLESS_MONITOR_INTERVAL_MS=" + std::to_string(interval) + ";";
        return *this;
    }

    ConnectionStringBuilder& withExtraUrlEncode(const bool& with_encode) {
        conn_str_ += "EXTRA_URL_ENCODE=" + std::to_string(with_encode ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withSslMode(const std::string& ssl_mode) {
        conn_str_ += "SSLMODE=" + ssl_mode + ";";
        return *this;
    }

    // MySQL/MariaDB ODBC: SSLVERIFY=0 keeps TLS on but skips cert CN check (needed for IP hosts).
    ConnectionStringBuilder& withSslVerify(const bool& ssl_verify) {
        conn_str_ += "SSLVERIFY=" + std::to_string(ssl_verify ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withCustomEndpoint(const bool& custom_endpoint_enabled) {
        conn_str_ += "ENABLE_CUSTOM_ENDPOINT=" + std::to_string(custom_endpoint_enabled ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withAuroraInitialConnectionStrategy(const bool& aurora_initial_connection_strategy_enabled) {
        conn_str_ += "ENABLE_AURORA_INITIAL_CONNECTION_STRATEGY=" + std::to_string(aurora_initial_connection_strategy_enabled ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withVerifyInitialConnectionType(const std::string& connection_type) {
        conn_str_ += "VERIFY_INITIAL_CONNECTION_TYPE=" + connection_type + ";";
        return *this;
    }

    ConnectionStringBuilder& withBlueGreenEnabled(const bool& bg_enabled) {
        conn_str_ += "ENABLE_BLUE_GREEN=" + std::to_string(bg_enabled ? 1 : 0) + ";";
        return *this;
    }

    ConnectionStringBuilder& withClusterId(const std::string& cluster_id) {
        conn_str_ += "CLUSTER_ID=" + cluster_id + ";";
        return *this;
    }

    ConnectionStringBuilder& withBlueGreenId(const std::string& blue_green_id) {
        conn_str_ += "BG_ID=" + blue_green_id + ";";
        return *this;
    }

    std::string getString() const {
        return conn_str_;
    }

private:
    std::string conn_str_;
};

#endif  // CONNECTION_STRING_BUILDER_H_
