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

#ifndef SSO_BROWSER_LOGIN_UTIL_H_
#define SSO_BROWSER_LOGIN_UTIL_H_

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/sso/SSOClient.h>
#include <aws/sso-oidc/SSOOIDCClient.h>

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "browser_auth_flow.h"

class SsoBrowserLoginUtil : protected BrowserAuthFlow {
public:
    SsoBrowserLoginUtil() = default;
    explicit SsoBrowserLoginUtil(std::map<std::string, std::string> connection_attributes);
    SsoBrowserLoginUtil(std::map<std::string, std::string> connection_attributes,
                        const std::shared_ptr<Aws::SSOOIDC::SSOOIDCClient>& oidc_client,
                        const std::shared_ptr<Aws::SSO::SSOClient>& sso_client);
    virtual ~SsoBrowserLoginUtil();

    // Cached token -> refresh-token -> (if allow_interactive) browser login.
    // Returns empty credentials on failure, with the reason in out_error.
    virtual Aws::Auth::AWSCredentials GetAwsCredentials(bool allow_interactive, std::string& out_error);

    static std::string GenerateCodeVerifier();
    static std::string GenerateCodeChallenge(const std::string& code_verifier);
    static std::string BuildOidcHostUrl(const std::string& region);

protected:
    virtual std::string FetchAuthorizationCode(const std::string& authorize_url, const std::string& state);

private:
    struct SsoToken {
        std::string access_token;
        std::string refresh_token;
        std::chrono::system_clock::time_point expires_at;
    };

    struct ClientRegistration {
        std::string client_id;
        std::string client_secret;
        std::chrono::system_clock::time_point expires_at;
    };

    void ParseSsoConfig(const std::map<std::string, std::string>& connection_attributes);

    bool RefreshAccessToken(const ClientRegistration& registration, const std::string& refresh_token,
                            SsoToken& out_token, std::string& out_error);
    bool InteractiveLogin(SsoToken& out_token, std::string& out_error);

    bool RegisterOidcClient(ClientRegistration& out_registration, std::string& out_error);
    Aws::Auth::AWSCredentials GetRoleCredentials(const std::string& access_token, std::string& out_error,
                                                 bool* out_token_rejected = nullptr);

    static bool IsRegistrationValid(const ClientRegistration& registration);

    // ~/.aws/sso/cache file, keyed by SHA1(session name, else start URL)
    std::string CacheFilePath() const;
    bool ReadCache(SsoToken& out_token, ClientRegistration& out_registration);
    void WriteCache(const SsoToken& token, const ClientRegistration& registration);
    void DeleteCache();

    std::shared_ptr<Aws::SSOOIDC::SSOOIDCClient> oidc_client_;
    std::shared_ptr<Aws::SSO::SSOClient> sso_client_;

    std::string start_url_;
    std::string sso_region_;
    std::string account_id_;
    std::string role_name_;
    std::string session_name_;
    std::string listen_port_;
    int idp_response_timeout_secs_ = DEFAULT_IDP_RESPONSE_TIMEOUT_SECS;

    // Process-wide guard around the cache file + client registration.
    static std::recursive_mutex cache_mutex_;

    static inline const std::string DEFAULT_LISTEN_PORT = "8080";
    static inline const std::string WEBSERVER_HOST = "http://127.0.0.1";
    static inline const std::string SSO_SCOPE = "sso:account:access";
    static inline const std::string CACHE_CREATED_BY_KEY = "createdBy";
    static inline const std::string CACHE_CREATED_BY_VALUE = "aws-advanced-odbc-wrapper";
    static inline const std::chrono::seconds EXPIRY_SKEW = std::chrono::seconds(30);
    static constexpr int CODE_VERIFIER_LENGTH = 60;
    static constexpr int DEFAULT_IDP_RESPONSE_TIMEOUT_SECS = 120;
    static constexpr int MIN_IDP_RESPONSE_TIMEOUT_SECS = 10;
    static constexpr int CREATE_TOKEN_POLL_INTERVAL_SECS = 1;
};

#endif // SSO_BROWSER_LOGIN_UTIL_H_
