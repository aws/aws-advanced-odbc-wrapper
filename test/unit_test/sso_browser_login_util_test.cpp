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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cctype>
#include <cstdlib>

#include <fstream>

#include <aws/core/client/AWSError.h>
#include <aws/core/platform/FileSystem.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/sso/SSOErrors.h>
#include <aws/sso/model/GetRoleCredentialsResult.h>
#include <aws/sso/model/RoleCredentials.h>
#include <aws/sso-oidc/SSOOIDCErrors.h>
#include <aws/sso-oidc/model/CreateTokenResult.h>
#include <aws/sso-oidc/model/RegisterClientResult.h>

#include "auth_mock_objects.h"

#include "../../driver/plugin/federated/sso_browser_login_util.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"

namespace {
    // Builds a CreateToken failure outcome carrying a specific OIDC error type.
    Aws::SSOOIDC::Model::CreateTokenOutcome OidcError(Aws::SSOOIDC::SSOOIDCErrors type) {
        return Aws::SSOOIDC::Model::CreateTokenOutcome(
            Aws::SSOOIDC::SSOOIDCError(
                Aws::Client::AWSError<Aws::SSOOIDC::SSOOIDCErrors>(type, "err", "error", false)));
    }

    Aws::SSOOIDC::Model::CreateTokenOutcome OidcTokenSuccess() {
        Aws::SSOOIDC::Model::CreateTokenResult token_result;
        token_result.SetAccessToken("sso-access-token");
        token_result.SetRefreshToken("sso-refresh-token");
        token_result.SetExpiresIn(3600);
        return Aws::SSOOIDC::Model::CreateTokenOutcome(token_result);
    }

    Aws::SSOOIDC::Model::RegisterClientOutcome OidcRegisterSuccess() {
        Aws::SSOOIDC::Model::RegisterClientResult reg_result;
        reg_result.SetClientId("client-id");
        reg_result.SetClientSecret("client-secret");
        reg_result.SetClientSecretExpiresAt(4102444800LL);  // far future
        return Aws::SSOOIDC::Model::RegisterClientOutcome(reg_result);
    }

    Aws::SSO::Model::GetRoleCredentialsOutcome SsoRoleSuccess() {
        Aws::SSO::Model::RoleCredentials role_creds;
        role_creds.SetAccessKeyId("AKIA-TEST");
        role_creds.SetSecretAccessKey("secret");
        role_creds.SetSessionToken("session");
        Aws::SSO::Model::GetRoleCredentialsResult role_result;
        role_result.SetRoleCredentials(role_creds);
        return Aws::SSO::Model::GetRoleCredentialsOutcome(role_result);
    }

    Aws::SSO::Model::GetRoleCredentialsOutcome SsoRoleError(Aws::SSO::SSOErrors type) {
        return Aws::SSO::Model::GetRoleCredentialsOutcome(
            Aws::SSO::SSOError(
                Aws::Client::AWSError<Aws::SSO::SSOErrors>(type, "err", "error", false)));
    }

    std::map<std::string, std::string> BaseConfig() {
        return {
            {KEY_SSO_START_URL, "https://my-sso.awsapps.com/start"},
            {KEY_SSO_REGION, "us-east-1"},
            {KEY_SSO_ACCOUNT_ID, "123456789012"},
            {KEY_SSO_ROLE_NAME, "MyRole"},
            {KEY_SSO_SESSION_NAME, "test-session"},
        };
    }
}

// Test subclass: stub the browser + redirect listener so the PKCE exchange can
// be exercised without a real browser, and expose protected statics.
class TestableSsoLoginUtil : public SsoBrowserLoginUtil {
public:
    TestableSsoLoginUtil(std::map<std::string, std::string> attrs,
                         const std::shared_ptr<Aws::SSOOIDC::SSOOIDCClient>& oidc,
                         const std::shared_ptr<Aws::SSO::SSOClient>& sso)
        : SsoBrowserLoginUtil(std::move(attrs), oidc, sso) {}

    std::string stub_auth_code = "stub-auth-code";
    std::string last_authorize_url;

protected:
    std::string FetchAuthorizationCode(const std::string& authorize_url, const std::string& /*state*/) override {
        last_authorize_url = authorize_url;
        return stub_auth_code;
    }
};

class SsoBrowserLoginUtilTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        AwsSdkHelper::EnsureInitialized();
    }

    // Redirect the SSO token cache to an isolated so tests don't read real ~/.aws/sso/cache
    void SetUp() override {
        const Aws::String tmp = Aws::FileSystem::CreateTempFilePath() + "-sso-cache";
        Aws::FileSystem::CreateDirectoryIfNotExists(tmp.c_str(), true);
        cache_dir_ = std::string(tmp.c_str());
#if defined(_WIN32)
        _putenv_s("AWS_ODBC_SSO_CACHE_DIR", cache_dir_.c_str());
#else
        setenv("AWS_ODBC_SSO_CACHE_DIR", cache_dir_.c_str(), 1);
#endif
    }

    void TearDown() override {
#if defined(_WIN32)
        _putenv_s("AWS_ODBC_SSO_CACHE_DIR", "");
#else
        unsetenv("AWS_ODBC_SSO_CACHE_DIR");
#endif
    }

    std::string CachePathFor(const std::string& cache_key) const {
        const Aws::String sha1_hex = Aws::Utils::HashingUtils::HexEncode(
            Aws::Utils::HashingUtils::CalculateSHA1(Aws::String(cache_key.c_str())));
        return cache_dir_ + "/" + std::string(sha1_hex.c_str()) + ".json";
    }

    void SeedCache(const std::string& cache_key, const std::string& access_token,
                   const std::string& refresh_token = "", bool valid_registration = false,
                   bool created_by_wrapper = true) {
        const Aws::Utils::DateTime future(
            std::chrono::system_clock::now() + std::chrono::hours(1));
        Aws::Utils::Json::JsonValue json;
        if (created_by_wrapper) {
            json.WithString("createdBy", "aws-advanced-odbc-wrapper");
        }
        json.WithString("accessToken", access_token.c_str());
        json.WithString("expiresAt", future.ToGmtString(Aws::Utils::DateFormat::ISO_8601));
        if (!refresh_token.empty()) {
            json.WithString("refreshToken", refresh_token.c_str());
        }
        if (valid_registration) {
            json.WithString("clientId", "cached-client-id");
            json.WithString("clientSecret", "cached-client-secret");
            json.WithString("registrationExpiresAt", future.ToGmtString(Aws::Utils::DateFormat::ISO_8601));
        }
        const Aws::String serialized = json.View().WriteCompact();
        std::ofstream out(CachePathFor(cache_key), std::ios::binary | std::ios::trunc);
        out.write(serialized.c_str(), static_cast<std::streamsize>(serialized.size()));
    }

    std::string cache_dir_;
};

TEST_F(SsoBrowserLoginUtilTest, CodeVerifier_LengthAndAlphabet) {
    const std::string verifier = SsoBrowserLoginUtil::GenerateCodeVerifier();
    EXPECT_EQ(60u, verifier.size());
    for (const char c : verifier) {
        const bool valid = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
        EXPECT_TRUE(valid) << "Unexpected char in verifier: " << c;
    }
}

TEST_F(SsoBrowserLoginUtilTest, CodeChallenge_IsBase64UrlSha256) {
    const std::string verifier = "abc123";
    const std::string challenge = SsoBrowserLoginUtil::GenerateCodeChallenge(verifier);

    EXPECT_EQ(std::string::npos, challenge.find('='));
    EXPECT_EQ(std::string::npos, challenge.find('+'));
    EXPECT_EQ(std::string::npos, challenge.find('/'));

    const Aws::Utils::ByteBuffer digest = Aws::Utils::HashingUtils::CalculateSHA256(Aws::String(verifier.c_str()));
    std::string expected(Aws::Utils::HashingUtils::Base64Encode(digest).c_str());
    for (char& c : expected) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    const size_t pad = expected.find('=');
    if (pad != std::string::npos) expected.erase(pad);
    EXPECT_EQ(expected, challenge);
}

TEST_F(SsoBrowserLoginUtilTest, CodeVerifier_IsRandom) {
    EXPECT_NE(SsoBrowserLoginUtil::GenerateCodeVerifier(), SsoBrowserLoginUtil::GenerateCodeVerifier());
}

TEST_F(SsoBrowserLoginUtilTest, BuildOidcHostUrl_ValidRegions) {
    EXPECT_EQ("oidc.us-east-1.amazonaws.com", SsoBrowserLoginUtil::BuildOidcHostUrl("us-east-1"));
    EXPECT_EQ("oidc.eu-west-2.amazonaws.com", SsoBrowserLoginUtil::BuildOidcHostUrl("EU-WEST-2"));
    EXPECT_EQ("oidc.cn-north-1.amazonaws.com.cn", SsoBrowserLoginUtil::BuildOidcHostUrl("cn-north-1"));
    EXPECT_EQ("oidc.us-east-1.amazonaws.com", SsoBrowserLoginUtil::BuildOidcHostUrl("  us-east-1  "));
}

TEST_F(SsoBrowserLoginUtilTest, BuildOidcHostUrl_InvalidRegions) {
    EXPECT_EQ("", SsoBrowserLoginUtil::BuildOidcHostUrl(""));
    EXPECT_EQ("", SsoBrowserLoginUtil::BuildOidcHostUrl("not a region"));
    EXPECT_EQ("", SsoBrowserLoginUtil::BuildOidcHostUrl("us_east_1"));
    EXPECT_EQ("", SsoBrowserLoginUtil::BuildOidcHostUrl("useast1"));
}

TEST_F(SsoBrowserLoginUtilTest, Construct_MissingRequiredParams_Throws) {
    std::map<std::string, std::string> attrs = BaseConfig();
    attrs.erase(KEY_SSO_ROLE_NAME);
    EXPECT_THROW(SsoBrowserLoginUtil util(attrs), std::runtime_error);
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_InteractivePkceRoundTrip) {
    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*oidc, RegisterClient(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(OidcRegisterSuccess()));
    EXPECT_CALL(*oidc, CreateToken(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(OidcTokenSuccess()));
    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SsoRoleSuccess()));

    TestableSsoLoginUtil util(BaseConfig(), oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(true, err);

    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ("AKIA-TEST", creds.GetAWSAccessKeyId());
    EXPECT_EQ("secret", creds.GetAWSSecretKey());
    EXPECT_EQ("session", creds.GetSessionToken());
    EXPECT_NE(std::string::npos, util.last_authorize_url.find("code_challenge="));
    EXPECT_NE(std::string::npos, util.last_authorize_url.find("code_challenge_method=S256"));
    EXPECT_NE(std::string::npos, util.last_authorize_url.find("response_type=code"));
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_PollLoop_PendingThenSlowDownThenSuccess) {
    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*oidc, RegisterClient(testing::_))
        .WillOnce(testing::Return(OidcRegisterSuccess()));
    EXPECT_CALL(*oidc, CreateToken(testing::_))
        .Times(testing::Exactly(3))
        .WillOnce(testing::Return(OidcError(Aws::SSOOIDC::SSOOIDCErrors::AUTHORIZATION_PENDING)))
        .WillOnce(testing::Return(OidcError(Aws::SSOOIDC::SSOOIDCErrors::SLOW_DOWN)))
        .WillOnce(testing::Return(OidcTokenSuccess()));
    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .WillOnce(testing::Return(SsoRoleSuccess()));

    TestableSsoLoginUtil util(BaseConfig(), oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(true, err);

    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ("AKIA-TEST", creds.GetAWSAccessKeyId());
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_PollLoop_NonRetryableError_FailsImmediately) {
    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*oidc, RegisterClient(testing::_))
        .WillOnce(testing::Return(OidcRegisterSuccess()));
    EXPECT_CALL(*oidc, CreateToken(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(OidcError(Aws::SSOOIDC::SSOOIDCErrors::ACCESS_DENIED)));
    EXPECT_CALL(*sso, GetRoleCredentials(testing::_)).Times(testing::Exactly(0));

    TestableSsoLoginUtil util(BaseConfig(), oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(true, err);

    EXPECT_TRUE(creds.IsEmpty());
    EXPECT_FALSE(err.empty());
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_CachedTokenRejected_FallsBackToRefresh) {
    std::map<std::string, std::string> attrs = BaseConfig();
    attrs[KEY_SSO_SESSION_NAME] = "signed-out-refreshable";
    SeedCache(attrs.at(KEY_SSO_SESSION_NAME), "stale-access-token",
              "good-refresh-token", true);

    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    // First call rejects the stale token; second (post-refresh) succeeds.
    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(SsoRoleError(Aws::SSO::SSOErrors::UNAUTHORIZED)))
        .WillOnce(testing::Return(SsoRoleSuccess()));
    // Refresh-token exchange succeeds; no browser (RegisterClient) needed.
    EXPECT_CALL(*oidc, RegisterClient(testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*oidc, CreateToken(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(OidcTokenSuccess()));

    TestableSsoLoginUtil util(attrs, oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(true, err);

    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ("AKIA-TEST", creds.GetAWSAccessKeyId());
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_CachedTokenRejected_NoRefresh_FallsBackToInteractive) {
    std::map<std::string, std::string> attrs = BaseConfig();
    attrs[KEY_SSO_SESSION_NAME] = "signed-out-no-refresh";
    SeedCache(attrs.at(KEY_SSO_SESSION_NAME), "stale-access-token");

    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .Times(testing::Exactly(2))
        .WillOnce(testing::Return(SsoRoleError(Aws::SSO::SSOErrors::UNAUTHORIZED)))
        .WillOnce(testing::Return(SsoRoleSuccess()));
    EXPECT_CALL(*oidc, RegisterClient(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(OidcRegisterSuccess()));
    EXPECT_CALL(*oidc, CreateToken(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(OidcTokenSuccess()));

    TestableSsoLoginUtil util(attrs, oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(true, err);

    EXPECT_TRUE(err.empty()) << err;
    EXPECT_EQ("AKIA-TEST", creds.GetAWSAccessKeyId());
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_CachedTokenRejected_NoPrompt_FailsFast) {
    std::map<std::string, std::string> attrs = BaseConfig();
    attrs[KEY_SSO_SESSION_NAME] = "signed-out-noprompt";
    SeedCache(attrs.at(KEY_SSO_SESSION_NAME), "stale-access-token");

    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SsoRoleError(Aws::SSO::SSOErrors::UNAUTHORIZED)));
    EXPECT_CALL(*oidc, RegisterClient(testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*oidc, CreateToken(testing::_)).Times(testing::Exactly(0));

    TestableSsoLoginUtil util(attrs, oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(false, err);

    EXPECT_TRUE(creds.IsEmpty());
    EXPECT_FALSE(err.empty());
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_CachedTokenTransientError_NoReauth) {
    std::map<std::string, std::string> attrs = BaseConfig();
    attrs[KEY_SSO_SESSION_NAME] = "transient-error";
    SeedCache(attrs.at(KEY_SSO_SESSION_NAME), "cached-access-token",
              "good-refresh-token", true);

    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SsoRoleError(Aws::SSO::SSOErrors::SERVICE_UNAVAILABLE)));
    EXPECT_CALL(*oidc, RegisterClient(testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*oidc, CreateToken(testing::_)).Times(testing::Exactly(0));

    TestableSsoLoginUtil util(attrs, oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(true, err);

    EXPECT_TRUE(creds.IsEmpty());
    EXPECT_FALSE(err.empty());
    EXPECT_TRUE(std::ifstream(CachePathFor(attrs.at(KEY_SSO_SESSION_NAME))).good());
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_CachedTokenRejected_DeletesWrapperCache) {
    std::map<std::string, std::string> attrs = BaseConfig();
    attrs[KEY_SSO_SESSION_NAME] = "signed-out-wrapper-cache";
    SeedCache(attrs.at(KEY_SSO_SESSION_NAME), "stale-access-token",
              "", false, /*created_by_wrapper=*/true);
    const std::string cache_path = CachePathFor(attrs.at(KEY_SSO_SESSION_NAME));
    ASSERT_TRUE(std::ifstream(cache_path).good());

    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SsoRoleError(Aws::SSO::SSOErrors::UNAUTHORIZED)));
    EXPECT_CALL(*oidc, RegisterClient(testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*oidc, CreateToken(testing::_)).Times(testing::Exactly(0));

    TestableSsoLoginUtil util(attrs, oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(false, err);

    EXPECT_TRUE(creds.IsEmpty());
    // The driver-created cache was removed by DeleteCache.
    EXPECT_FALSE(std::ifstream(cache_path).good());
}

// A rejected token in a cache file NOT created by this driver (e.g. the AWS CLI,
// which shares ~/.aws/sso/cache and the same file-naming scheme) must be left
// untouched. NOPROMPT isolates DeleteCache; the file must survive byte-for-byte.
TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_CachedTokenRejected_PreservesForeignCache) {
    std::map<std::string, std::string> attrs = BaseConfig();
    attrs[KEY_SSO_SESSION_NAME] = "signed-out-cli-cache";
    SeedCache(attrs.at(KEY_SSO_SESSION_NAME), "cli-access-token",
              "", false, /*created_by_wrapper=*/false);
    const std::string cache_path = CachePathFor(attrs.at(KEY_SSO_SESSION_NAME));

    const auto read_file = [](const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::string contents;
        contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>{});
        return contents;
    };
    const std::string original_contents = read_file(cache_path);

    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*sso, GetRoleCredentials(testing::_))
        .Times(testing::Exactly(1))
        .WillOnce(testing::Return(SsoRoleError(Aws::SSO::SSOErrors::UNAUTHORIZED)));
    EXPECT_CALL(*oidc, RegisterClient(testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*oidc, CreateToken(testing::_)).Times(testing::Exactly(0));

    TestableSsoLoginUtil util(attrs, oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(false, err);

    EXPECT_TRUE(creds.IsEmpty());
    // The foreign cache must be intact — DeleteCache must not touch it.
    ASSERT_TRUE(std::ifstream(cache_path).good());
    EXPECT_EQ(original_contents, read_file(cache_path));
}

TEST_F(SsoBrowserLoginUtilTest, GetAwsCredentials_NoPromptNoCache_FailsFast) {
    auto oidc = std::make_shared<MOCK_SSO_OIDC_CLIENT>();
    auto sso = std::make_shared<MOCK_SSO_CLIENT>();

    EXPECT_CALL(*oidc, RegisterClient(testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*oidc, CreateToken(testing::_)).Times(testing::Exactly(0));
    EXPECT_CALL(*sso, GetRoleCredentials(testing::_)).Times(testing::Exactly(0));

    std::map<std::string, std::string> attrs = BaseConfig();
    attrs[KEY_SSO_SESSION_NAME] = "no-cache-session-unit-test-xyz";
    TestableSsoLoginUtil util(attrs, oidc, sso);
    std::string err;
    const Aws::Auth::AWSCredentials creds = util.GetAwsCredentials(false, err);

    EXPECT_TRUE(creds.IsEmpty());
    EXPECT_FALSE(err.empty());
    EXPECT_NE(std::string::npos, err.find("interactiv"));
}
