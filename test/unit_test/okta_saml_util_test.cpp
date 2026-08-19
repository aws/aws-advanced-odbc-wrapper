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

#include <map>

#include "auth_mock_objects.h"

#include "../../driver/plugin/federated/okta_auth_plugin.h"
#include "../../driver/util/aws_sdk_helper.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/driver.h"

namespace {
    const std::string IDP_ENDPOINT("endpoint.com");
    const std::string IDP_PORT("1234");
    const std::string IDP_ROLE_ARN("arn:aws:iam::012345678910:role/okta_iam_role");
    const std::string IDP_SAML_ARN("arn:aws:iam::012345678910:saml-provider/okta");
    const std::string IDP_USERNAME("my_user");
    const std::string IDP_PASSWORD("my_pass");
    const std::string IDP_APP_ID("abc123def456");
    const std::string ACCESS_KEY("test_access_key");
    const std::string SECRET_KEY("test_secret_key");
    const std::string SESSION_KEY("test_session_key");
    std::string resp_token_stream("{\"sessionToken\": \"longuniquesessiontoken\"}");
    std::string resp_saml_stream("<input name=\"SAMLResponse\" type=\"hidden\" value=\"long-saml-value-password\"/>");
    const char *saml_resp_str("long-saml-value-password");
}

class OktaSamlUtilTest : public testing::Test {
protected:
    std::shared_ptr<MockHttpClient> mock_http_client_;
    std::shared_ptr<MockStsClient> mock_sts_client_;
    std::map<std::string, std::string> conn_attr_;

    // Runs once per suite
    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }
    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    // Runs per test
    void SetUp() override {
        conn_attr_.insert_or_assign(KEY_IDP_ENDPOINT, IDP_ENDPOINT);
        conn_attr_.insert_or_assign(KEY_IDP_PORT, IDP_PORT);
        conn_attr_.insert_or_assign(KEY_IDP_USERNAME, IDP_USERNAME);
        conn_attr_.insert_or_assign(KEY_IDP_PASSWORD, IDP_PASSWORD);
        conn_attr_.insert_or_assign(KEY_IDP_ROLE_ARN, IDP_ROLE_ARN);
        conn_attr_.insert_or_assign(KEY_IDP_SAML_ARN, IDP_SAML_ARN);
        conn_attr_.insert_or_assign(KEY_APP_ID, IDP_APP_ID);

        mock_http_client_ = std::make_shared<MockHttpClient>();
        mock_sts_client_ = std::make_shared<MockStsClient>();
    }
    void TearDown() override {
        if (mock_sts_client_) mock_sts_client_.reset();
        if (mock_http_client_) mock_http_client_.reset();
    }
};

TEST_F(OktaSamlUtilTest, GetSamlAssertion_Success) {
    std::shared_ptr<MockHttpResp> session_token_resp = std::make_shared<MockHttpResp>();
    EXPECT_CALL(*session_token_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_token_stream);
    EXPECT_CALL(*session_token_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MockHttpResp> saml_resp = std::make_shared<MockHttpResp>();
    EXPECT_CALL(*saml_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> saml_body =
        std::make_shared<std::stringstream>(resp_saml_stream);
    EXPECT_CALL(*saml_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*saml_body));

    EXPECT_CALL(*mock_http_client_, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(session_token_resp))
        .WillOnce(testing::Return(saml_resp));

    OktaSamlUtil okta_saml_util(conn_attr_, mock_http_client_, mock_sts_client_);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ(saml_resp_str, okta_saml.c_str());
}

TEST_F(OktaSamlUtilTest, GetSamlAssertion_BadSessionToken) {
    std::shared_ptr<MockHttpResp> bad_resp = std::make_shared<MockHttpResp>();
    EXPECT_CALL(*bad_resp, GetResponseCode())
        .WillRepeatedly(testing::Return(Aws::Http::HttpResponseCode::NOT_FOUND));
    EXPECT_CALL(*bad_resp, HasClientError())
        .WillRepeatedly(testing::Return(true));
    Aws::String client_err_msg("Bad Request");
    EXPECT_CALL(*bad_resp, GetClientErrorMessage())
        .WillRepeatedly(testing::ReturnRef(client_err_msg));

    EXPECT_CALL(*mock_http_client_, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(bad_resp));

    OktaSamlUtil okta_saml_util(conn_attr_, mock_http_client_, mock_sts_client_);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", okta_saml.c_str());
}

TEST_F(OktaSamlUtilTest, GetSamlAssertion_BadSamlRequest) {
    std::shared_ptr<MockHttpResp> session_token_resp = std::make_shared<MockHttpResp>();
    EXPECT_CALL(*session_token_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_token_stream);
    EXPECT_CALL(*session_token_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MockHttpResp> bad_resp = std::make_shared<MockHttpResp>();
    EXPECT_CALL(*bad_resp, GetResponseCode())
        .WillRepeatedly(testing::Return(Aws::Http::HttpResponseCode::NOT_FOUND));
    EXPECT_CALL(*bad_resp, HasClientError())
        .WillRepeatedly(testing::Return(true));
    Aws::String client_err_msg("Bad Request");
    EXPECT_CALL(*bad_resp, GetClientErrorMessage())
        .WillRepeatedly(testing::ReturnRef(client_err_msg));

    EXPECT_CALL(*mock_http_client_, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(session_token_resp))
        .WillOnce(testing::Return(bad_resp));

    OktaSamlUtil okta_saml_util(conn_attr_, mock_http_client_, mock_sts_client_);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", okta_saml.c_str());
}

TEST_F(OktaSamlUtilTest, GetSamlAssertion_BadSamlResponse) {
    std::shared_ptr<MockHttpResp> session_token_resp = std::make_shared<MockHttpResp>();
    EXPECT_CALL(*session_token_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_token_stream);
    EXPECT_CALL(*session_token_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MockHttpResp> bad_saml_resp = std::make_shared<MockHttpResp>();
    EXPECT_CALL(*bad_saml_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> saml_body =
        std::make_shared<std::stringstream>("bad-saml-body");
    EXPECT_CALL(*bad_saml_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*saml_body));

    EXPECT_CALL(*mock_http_client_, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(session_token_resp))
        .WillOnce(testing::Return(bad_saml_resp));

    OktaSamlUtil okta_saml_util(conn_attr_, mock_http_client_, mock_sts_client_);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", okta_saml.c_str());
}
