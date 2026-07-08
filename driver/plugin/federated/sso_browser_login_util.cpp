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

#include "sso_browser_login_util.h"

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/AWSError.h>
#include <aws/core/platform/FileSystem.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/sso/SSOErrors.h>
#include <aws/sso/model/GetRoleCredentialsRequest.h>
#include <aws/sso/model/GetRoleCredentialsResult.h>
#include <aws/sso/model/RoleCredentials.h>
#include <aws/sso-oidc/SSOOIDCErrors.h>
#include <aws/sso-oidc/model/CreateTokenRequest.h>
#include <aws/sso-oidc/model/RegisterClientRequest.h>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <system_error>
#include <thread>

#include "http/WEBServer_utils.h"

#include "../../util/aws_sdk_helper.h"
#include "../../util/connection_string_keys.h"
#include "../../util/logger_wrapper.h"
#include "../../util/map_utils.h"
#include "../../util/rds_strings.h"
#include "../../util/rds_utils.h"

std::recursive_mutex SsoBrowserLoginUtil::cache_mutex_;

namespace {
    // base64url alphabet (RFC 7636) used for the PKCE code_verifier.
    constexpr char CODE_VERIFIER_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string ToBase64Url(const Aws::String& base64) {
        std::string result(base64.c_str(), base64.size());
        for (char& c : result) {
            if (c == '+') {
                c = '-';
            } else if (c == '/') {
                c = '_';
            }
        }
        const size_t pad = result.find('=');
        if (pad != std::string::npos) {
            result.erase(pad);
        }
        return result;
    }

    // SSO-OIDC errors often have an empty message, so include exception name, HTTP status, and request id to surface the real cause.
    template <typename ERROR_TYPE>
    std::string FormatAwsError(const Aws::Client::AWSError<ERROR_TYPE>& error) {
        std::string detail;
        const std::string name(error.GetExceptionName().c_str());
        const std::string message(error.GetMessage().c_str());
        if (!name.empty()) {
            detail += name;
        }
        if (!message.empty()) {
            detail += (detail.empty() ? "" : ": ") + message;
        }
        if (detail.empty()) {
            detail = "HTTP " + std::to_string(static_cast<int>(error.GetResponseCode()));
        }
        const std::string request_id(error.GetRequestId().c_str());
        if (!request_id.empty()) {
            detail += " (requestId=" + request_id + ")";
        }
        return detail;
    }
}  // namespace

SsoBrowserLoginUtil::SsoBrowserLoginUtil(std::map<std::string, std::string> connection_attributes)
    : SsoBrowserLoginUtil(std::move(connection_attributes), nullptr, nullptr) {}

SsoBrowserLoginUtil::SsoBrowserLoginUtil(
    std::map<std::string, std::string> connection_attributes,
    const std::shared_ptr<Aws::SSOOIDC::SSOOIDCClient>& oidc_client,
    const std::shared_ptr<Aws::SSO::SSOClient>& sso_client)
{
    ParseSsoConfig(connection_attributes);
    AwsSdkHelper::EnsureInitialized();

    // SSO/SSO-OIDC are not SigV4 APIs; anonymous creds stop the SDK signing
    // IdC rejects a signed CreateToken with InvalidRequestException
    const auto anonymous_provider = Aws::MakeShared<Aws::Auth::AnonymousAWSCredentialsProvider>("SsoBrowserLoginUtil");

    if (oidc_client) {
        oidc_client_ = oidc_client;
    } else {
        Aws::SSOOIDC::SSOOIDCClientConfiguration oidc_config;
        oidc_config.region = sso_region_;
        oidc_client_ = std::make_shared<Aws::SSOOIDC::SSOOIDCClient>(anonymous_provider, nullptr, oidc_config);
    }

    if (sso_client) {
        sso_client_ = sso_client;
    } else {
        Aws::SSO::SSOClientConfiguration sso_config;
        sso_config.region = sso_region_;
        sso_client_ = std::make_shared<Aws::SSO::SSOClient>(anonymous_provider, nullptr, sso_config);
    }
}

SsoBrowserLoginUtil::~SsoBrowserLoginUtil()
{
    if (oidc_client_) {
        oidc_client_ = nullptr;
    }
    if (sso_client_) {
        sso_client_ = nullptr;
    }
}

void SsoBrowserLoginUtil::ParseSsoConfig(const std::map<std::string, std::string>& connection_attributes)
{
    start_url_ = MapUtils::GetStringValue(connection_attributes, KEY_SSO_START_URL, "");
    account_id_ = MapUtils::GetStringValue(connection_attributes, KEY_SSO_ACCOUNT_ID, "");
    role_name_ = MapUtils::GetStringValue(connection_attributes, KEY_SSO_ROLE_NAME, "");
    session_name_ = MapUtils::GetStringValue(connection_attributes, KEY_SSO_SESSION_NAME, "");
    listen_port_ = MapUtils::GetStringValue(connection_attributes, KEY_SSO_LISTEN_PORT, DEFAULT_LISTEN_PORT);

    // Region precedence: explicit SSO_REGION, then REGION, then host-derived, then us-east-1
    sso_region_ = MapUtils::GetStringValue(connection_attributes, KEY_SSO_REGION, "");
    if (sso_region_.empty()) {
        sso_region_ = MapUtils::GetStringValue(connection_attributes, KEY_REGION, "");
    }
    if (sso_region_.empty()) {
        sso_region_ = connection_attributes.contains(KEY_SERVER) ?
            RdsUtils::GetRdsRegion(connection_attributes.at(KEY_SERVER))
            : Aws::Region::US_EAST_1;
    }

    idp_response_timeout_secs_ = DEFAULT_IDP_RESPONSE_TIMEOUT_SECS;
    if (connection_attributes.contains(KEY_SSO_IDP_RESPONSE_TIMEOUT)) {
        const int configured = static_cast<int>(
            std::strtol(connection_attributes.at(KEY_SSO_IDP_RESPONSE_TIMEOUT).c_str(), nullptr, 10));
        idp_response_timeout_secs_ = configured > MIN_IDP_RESPONSE_TIMEOUT_SECS
            ? configured : MIN_IDP_RESPONSE_TIMEOUT_SECS;
    }

    if (start_url_.empty() || account_id_.empty() || role_name_.empty()) {
        std::string err_msg = "Missing required parameter for AWS IAM Identity Center (SSO) authentication:";
        err_msg += start_url_.empty() ? std::string("\n\t") + KEY_SSO_START_URL : "";
        err_msg += account_id_.empty() ? std::string("\n\t") + KEY_SSO_ACCOUNT_ID : "";
        err_msg += role_name_.empty() ? std::string("\n\t") + KEY_SSO_ROLE_NAME : "";
        throw std::runtime_error(err_msg);
    }
}

std::string SsoBrowserLoginUtil::GenerateCodeVerifier()
{
    constexpr int ALPHABET_SIZE = static_cast<int>(sizeof(CODE_VERIFIER_CHARS)) - 2; // exclude trailing '\0'
    std::string verifier;
    verifier.reserve(CODE_VERIFIER_LENGTH);
    for (int i = 0; i < CODE_VERIFIER_LENGTH; ++i) {
        verifier.push_back(CODE_VERIFIER_CHARS[WebServerUtils::GenerateRandomInteger(0, ALPHABET_SIZE)]);
    }
    return verifier;
}

std::string SsoBrowserLoginUtil::GenerateCodeChallenge(const std::string& code_verifier)
{
    const Aws::Utils::ByteBuffer sha256 = Aws::Utils::HashingUtils::CalculateSHA256(Aws::String(code_verifier));
    return ToBase64Url(Aws::Utils::HashingUtils::Base64Encode(sha256));
}

std::string SsoBrowserLoginUtil::BuildOidcHostUrl(const std::string& region)
{
    // Lowercase + trim the region before validation to avoid SDK boundary issues.
    std::string normalized = region;
    normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
    const size_t last = normalized.find_last_not_of(" \t\r\n");
    if (last != std::string::npos) {
        normalized.erase(last + 1);
    }
    for (char& c : normalized) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Validate to avoid SDK boundary issues with a malformed region.
    static const std::regex REGION_PATTERN("^[a-z]{2,3}(-[a-z]+)+-[0-9]+$");
    if (!std::regex_match(normalized, REGION_PATTERN)) {
        LOG(ERROR) << "Invalid AWS region for SSO-OIDC endpoint: '" << region << "'";
        return "";
    }

    const std::string suffix = normalized.starts_with("cn-") ? "amazonaws.com.cn" : "amazonaws.com";
    return "oidc." + normalized + "." + suffix;
}

Aws::Auth::AWSCredentials SsoBrowserLoginUtil::GetAwsCredentials(bool allow_interactive, std::string& out_error)
{
    const std::lock_guard<std::recursive_mutex> lock_guard(cache_mutex_);

    SsoToken cached_token;
    ClientRegistration registration;
    const bool have_cache = ReadCache(cached_token, registration);

    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    if (have_cache && !cached_token.access_token.empty() && now + EXPIRY_SKEW < cached_token.expires_at) {
        LOG(INFO) << "Using cached AWS IAM Identity Center access token";
        bool token_rejected = false;
        const Aws::Auth::AWSCredentials credentials =
            GetRoleCredentials(cached_token.access_token, out_error, &token_rejected);
        if (!credentials.IsEmpty() || !token_rejected) {
            return credentials;
        }
        LOG(WARNING) << "Cached AWS IAM Identity Center token was rejected; re-authenticating";
        DeleteCache();
        cached_token.access_token.clear();
        out_error.clear();
    }

    if (have_cache && !cached_token.refresh_token.empty() && IsRegistrationValid(registration)) {
        SsoToken refreshed;
        if (RefreshAccessToken(registration, cached_token.refresh_token, refreshed, out_error)) {
            WriteCache(refreshed, registration);
            return GetRoleCredentials(refreshed.access_token, out_error);
        }
        LOG(WARNING) << "AWS IAM Identity Center token refresh failed; falling back to interactive login if allowed";
    }

    if (!allow_interactive) {
        out_error = "AWS IAM Identity Center login required. No valid cached or refreshable token is available, "
                    "and the connection is non-interactive (SQL_DRIVER_NOPROMPT). Reconnect interactively to complete the browser login.";
        LOG(ERROR) << out_error;
        return {};
    }

    SsoToken token;
    if (!InteractiveLogin(token, out_error)) {
        return {};
    }
    return GetRoleCredentials(token.access_token, out_error);
}

bool SsoBrowserLoginUtil::IsRegistrationValid(const ClientRegistration& registration)
{
    return !registration.client_id.empty()
        && std::chrono::system_clock::now() + EXPIRY_SKEW < registration.expires_at;
}

bool SsoBrowserLoginUtil::RegisterOidcClient(ClientRegistration& out_registration, std::string& out_error)
{
    Aws::SSOOIDC::Model::RegisterClientRequest req;
    req.SetClientName("aws-advanced-odbc-wrapper");
    req.SetClientType("public");
    req.SetScopes(Aws::Vector<Aws::String>{SSO_SCOPE.c_str()});
    req.SetGrantTypes(Aws::Vector<Aws::String>{"authorization_code", "refresh_token"});
    req.SetRedirectUris(Aws::Vector<Aws::String>{(WEBSERVER_HOST + ":" + listen_port_).c_str()});
    if (!start_url_.empty()) {
        req.SetIssuerUrl(start_url_.c_str());
    }

    const Aws::SSOOIDC::Model::RegisterClientOutcome outcome = oidc_client_->RegisterClient(req);
    if (!outcome.IsSuccess()) {
        out_error = "AWS IAM Identity Center RegisterClient failed: " + FormatAwsError(outcome.GetError());
        LOG(ERROR) << out_error;
        return false;
    }

    const Aws::SSOOIDC::Model::RegisterClientResult& result = outcome.GetResult();
    out_registration.client_id = result.GetClientId().c_str();
    out_registration.client_secret = result.GetClientSecret().c_str();
    out_registration.expires_at =
        std::chrono::system_clock::from_time_t(static_cast<time_t>(result.GetClientSecretExpiresAt()));
    return true;
}

bool SsoBrowserLoginUtil::InteractiveLogin(SsoToken& out_token, std::string& out_error)
{
    ClientRegistration registration;
    if (!RegisterOidcClient(registration, out_error)) {
        return false;
    }

    const std::string code_verifier = GenerateCodeVerifier();
    const std::string code_challenge = GenerateCodeChallenge(code_verifier);
    const std::string state = WebServerUtils::GenerateState();
    const std::string redirect_uri = WEBSERVER_HOST + ":" + listen_port_;

    const std::string oidc_host = BuildOidcHostUrl(sso_region_);
    if (oidc_host.empty()) {
        out_error = "AWS IAM Identity Center login failed: invalid region '" + sso_region_ + "'";
        return false;
    }

    const std::string authorize_url =
        "https://" + oidc_host + "/authorize?response_type=code"
        + "&client_id=" + Aws::Utils::StringUtils::URLEncode(registration.client_id.c_str()).c_str()
        + "&redirect_uri=" + Aws::Utils::StringUtils::URLEncode(redirect_uri.c_str()).c_str()
        + "&scopes=" + Aws::Utils::StringUtils::URLEncode(SSO_SCOPE.c_str()).c_str()
        + "&state=" + Aws::Utils::StringUtils::URLEncode(state.c_str()).c_str()
        + "&code_challenge=" + code_challenge
        + "&code_challenge_method=S256";

    const std::string auth_code = FetchAuthorizationCode(authorize_url, state);
    if (auth_code.empty()) {
        out_error = "AWS IAM Identity Center login failed: no authorization code was returned (browser closed, "
                    "timed out, or the local redirect listener could not bind to " + redirect_uri + ").";
        LOG(ERROR) << out_error;
        return false;
    }

    // Exchange the auth code for a token, poll until the response timeout
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(idp_response_timeout_secs_);
    int poll_interval = CREATE_TOKEN_POLL_INTERVAL_SECS;
    while (true) {
        Aws::SSOOIDC::Model::CreateTokenRequest req;
        req.SetClientId(registration.client_id.c_str());
        req.SetClientSecret(registration.client_secret.c_str());
        req.SetGrantType("authorization_code");
        req.SetCode(auth_code.c_str());
        req.SetCodeVerifier(code_verifier.c_str());
        req.SetRedirectUri(redirect_uri.c_str());

        const Aws::SSOOIDC::Model::CreateTokenOutcome outcome = oidc_client_->CreateToken(req);
        if (outcome.IsSuccess()) {
            const Aws::SSOOIDC::Model::CreateTokenResult& result = outcome.GetResult();
            out_token.access_token = result.GetAccessToken().c_str();
            out_token.refresh_token = result.GetRefreshToken().c_str();
            out_token.expires_at =
                std::chrono::system_clock::now() + std::chrono::seconds(result.GetExpiresIn());
            WriteCache(out_token, registration);
            return true;
        }

        const Aws::SSOOIDC::SSOOIDCErrors err = outcome.GetError().GetErrorType();
        if (err == Aws::SSOOIDC::SSOOIDCErrors::SLOW_DOWN) {
            poll_interval += CREATE_TOKEN_POLL_INTERVAL_SECS;  // back off, do not fail
        } else if (err != Aws::SSOOIDC::SSOOIDCErrors::AUTHORIZATION_PENDING) {
            out_error = "AWS IAM Identity Center CreateToken failed: "
                        + FormatAwsError(outcome.GetError());
            LOG(ERROR) << out_error;
            return false;
        }

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            out_error = "AWS IAM Identity Center CreateToken timed out waiting for authorization.";
            LOG(ERROR) << out_error;
            return false;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(deadline - now);
        const std::chrono::seconds sleep_for_secs =
            remaining < std::chrono::seconds(poll_interval) ? remaining : std::chrono::seconds(poll_interval);
        std::this_thread::sleep_for(sleep_for_secs);
    }
}

bool SsoBrowserLoginUtil::RefreshAccessToken(
    const ClientRegistration& registration,
    const std::string& refresh_token,
    SsoToken& out_token,
    std::string& out_error)
{
    Aws::SSOOIDC::Model::CreateTokenRequest req;
    req.SetClientId(registration.client_id.c_str());
    req.SetClientSecret(registration.client_secret.c_str());
    req.SetGrantType("refresh_token");
    req.SetRefreshToken(refresh_token.c_str());

    const Aws::SSOOIDC::Model::CreateTokenOutcome outcome = oidc_client_->CreateToken(req);
    if (!outcome.IsSuccess()) {
        out_error = "AWS IAM Identity Center token refresh failed: "
                    + FormatAwsError(outcome.GetError());
        LOG(WARNING) << out_error;
        return false;
    }

    const Aws::SSOOIDC::Model::CreateTokenResult& result = outcome.GetResult();
    out_token.access_token = result.GetAccessToken().c_str();
    // Keep old if refresh did not update
    out_token.refresh_token = result.GetRefreshToken().empty()
        ? refresh_token : std::string(result.GetRefreshToken().c_str());
    out_token.expires_at = std::chrono::system_clock::now() + std::chrono::seconds(result.GetExpiresIn());
    return true;
}

Aws::Auth::AWSCredentials SsoBrowserLoginUtil::GetRoleCredentials(
    const std::string& access_token, std::string& out_error, bool* out_token_rejected)
{
    if (out_token_rejected) {
        *out_token_rejected = false;
    }
    if (access_token.empty()) {
        out_error = "AWS IAM Identity Center login did not yield an access token";
        return {};
    }

    Aws::SSO::Model::GetRoleCredentialsRequest req;
    req.SetAccessToken(access_token.c_str());
    req.SetAccountId(account_id_.c_str());
    req.SetRoleName(role_name_.c_str());

    const Aws::SSO::Model::GetRoleCredentialsOutcome outcome = sso_client_->GetRoleCredentials(req);
    if (!outcome.IsSuccess()) {
        out_error = "AWS IAM Identity Center GetRoleCredentials failed: "
                    + FormatAwsError(outcome.GetError());
        LOG(ERROR) << out_error;
        // Rejected access token due to user signout or server invalidated
        if (out_token_rejected) {
            const Aws::SSO::SSOErrors err = outcome.GetError().GetErrorType();
            *out_token_rejected = err == Aws::SSO::SSOErrors::UNAUTHORIZED
                || err == Aws::SSO::SSOErrors::ACCESS_DENIED
                || err == Aws::SSO::SSOErrors::UNRECOGNIZED_CLIENT
                || err == Aws::SSO::SSOErrors::INVALID_CLIENT_TOKEN_ID;
        }
        return {};
    }

    const Aws::SSO::Model::RoleCredentials& role_credentials = outcome.GetResult().GetRoleCredentials();
    return Aws::Auth::AWSCredentials(
        role_credentials.GetAccessKeyId(),
        role_credentials.GetSecretAccessKey(),
        role_credentials.GetSessionToken());
}

std::string SsoBrowserLoginUtil::FetchAuthorizationCode(const std::string& authorize_url, const std::string& state)
{
    return RunBrowserFlow(authorize_url, state, listen_port_,
        std::to_string(idp_response_timeout_secs_)).auth_code;
}

std::string SsoBrowserLoginUtil::CacheFilePath() const
{
    // Key on the session name when present, else the start URL, matching the SDK's ~/.aws/sso/cache layout (SHA1 hex of the key)
    const std::string key = session_name_.empty() ? start_url_ : session_name_;
    const Aws::String sha1_hex =
        Aws::Utils::HashingUtils::HexEncode(Aws::Utils::HashingUtils::CalculateSHA1(Aws::String(key.c_str())));

    // Allow tests (and advanced users) to redirect the cache directory so the real ~/.aws/sso/cache is never touched during a test run
    Aws::String cache_dir;
    if (const char* override_dir = std::getenv("AWS_ODBC_SSO_CACHE_DIR"); override_dir && override_dir[0] != '\0') {
        cache_dir = override_dir;
    } else {
        const Aws::String home = Aws::FileSystem::GetHomeDirectory();
        const Aws::String aws_dir = Aws::FileSystem::Join(home, ".aws");
        const Aws::String sso_dir = Aws::FileSystem::Join(aws_dir, "sso");
        cache_dir = Aws::FileSystem::Join(sso_dir, "cache");
    }
    Aws::FileSystem::CreateDirectoryIfNotExists(cache_dir.c_str(), true);

    const Aws::String file = Aws::FileSystem::Join(cache_dir, sha1_hex + ".json");
    return {file.c_str()};
}

bool SsoBrowserLoginUtil::ReadCache(SsoToken& out_token, ClientRegistration& out_registration)
{
    const std::string path = CacheFilePath();
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    const Aws::Utils::Json::JsonValue json(in);
    if (!json.WasParseSuccessful()) {
        LOG(WARNING) << "AWS IAM Identity Center token cache is not valid JSON; ignoring: " << path;
        return false;
    }

    const Aws::Utils::Json::JsonView view = json.View();
    if (view.KeyExists("accessToken")) {
        out_token.access_token = view.GetString("accessToken").c_str();
    }
    if (view.KeyExists("refreshToken")) {
        out_token.refresh_token = view.GetString("refreshToken").c_str();
    }
    if (view.KeyExists("expiresAt")) {
        const Aws::Utils::DateTime expires(view.GetString("expiresAt"), Aws::Utils::DateFormat::ISO_8601);
        out_token.expires_at = expires.UnderlyingTimestamp();
    }
    if (view.KeyExists("clientId")) {
        out_registration.client_id = view.GetString("clientId").c_str();
    }
    if (view.KeyExists("clientSecret")) {
        out_registration.client_secret = view.GetString("clientSecret").c_str();
    }
    if (view.KeyExists("registrationExpiresAt")) {
        const Aws::Utils::DateTime reg_expires(view.GetString("registrationExpiresAt"), Aws::Utils::DateFormat::ISO_8601);
        out_registration.expires_at = reg_expires.UnderlyingTimestamp();
    }
    return true;
}

void SsoBrowserLoginUtil::WriteCache(const SsoToken& token, const ClientRegistration& registration)
{
    Aws::Utils::Json::JsonValue json;
    json.WithString(CACHE_CREATED_BY_KEY.c_str(), CACHE_CREATED_BY_VALUE.c_str());
    json.WithString("startUrl", start_url_.c_str());
    json.WithString("region", sso_region_.c_str());
    if (!session_name_.empty()) {
        json.WithString("sessionName", session_name_.c_str());
    }
    json.WithString("accessToken", token.access_token.c_str());
    if (!token.refresh_token.empty()) {
        json.WithString("refreshToken", token.refresh_token.c_str());
    }
    json.WithString("expiresAt",
        Aws::Utils::DateTime(token.expires_at).ToGmtString(Aws::Utils::DateFormat::ISO_8601));
    if (!registration.client_id.empty()) {
        json.WithString("clientId", registration.client_id.c_str());
        json.WithString("clientSecret", registration.client_secret.c_str());
        json.WithString("registrationExpiresAt",
            Aws::Utils::DateTime(registration.expires_at).ToGmtString(Aws::Utils::DateFormat::ISO_8601));
    }

    const std::string path = CacheFilePath();
    const Aws::String serialized = json.View().WriteCompact();

    // Cache holds secrets, write owner-only to a temp file, then atomic rename
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            LOG(WARNING) << "Unable to write AWS IAM Identity Center token cache: " << tmp_path;
            return;
        }
        std::error_code perm_ec;
        std::filesystem::permissions(
            tmp_path,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace,
            perm_ec);
        if (perm_ec) {
            LOG(WARNING) << "Unable to restrict permissions on AWS IAM Identity Center token cache: "
                         << perm_ec.message();
        }
        out.write(serialized.c_str(), static_cast<std::streamsize>(serialized.size()));
        out.flush();
        if (!out.good()) {
            LOG(WARNING) << "Failed to write AWS IAM Identity Center token cache: " << tmp_path;
            out.close();
            std::error_code rm_ec;
            std::filesystem::remove(tmp_path, rm_ec);
            return;
        }
    }

    std::error_code rename_ec;
    std::filesystem::rename(tmp_path, path, rename_ec);
    if (rename_ec) {
        LOG(WARNING) << "Unable to finalize AWS IAM Identity Center token cache: " << rename_ec.message();
        std::error_code rm_ec;
        std::filesystem::remove(tmp_path, rm_ec);
    }
}

void SsoBrowserLoginUtil::DeleteCache()
{
    const std::string path = CacheFilePath();

    // The cache directory (~/.aws/sso/cache) and file-naming scheme are shared with the AWS CLI/SDK
    // Only remove a file this plugin created, identified by marker written in WriteCache
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return;
    }
    const Aws::Utils::Json::JsonValue json(in);
    in.close();
    const bool created_by_wrapper = json.WasParseSuccessful()
        && json.View().GetString(CACHE_CREATED_BY_KEY.c_str()) == CACHE_CREATED_BY_VALUE.c_str();
    if (!created_by_wrapper) {
        LOG(INFO) << "Leaving AWS IAM Identity Center token cache in place; not created by this driver: " << path;
        return;
    }

    std::error_code rm_ec;
    std::filesystem::remove(path, rm_ec);
    if (rm_ec) {
        LOG(WARNING) << "Unable to remove AWS IAM Identity Center token cache: " << rm_ec.message();
    }
}
