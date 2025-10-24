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
    const std::string idp_endpoint("endpoint.com");
    const std::string idp_port("1234");
    const std::string idp_role_arn("arn:aws:iam::012345678910:role/adfs_iam_role");
    const std::string idp_saml_arn("arn:aws:iam::012345678910:saml-provider/adfs");
    const std::string idp_username("my_user");
    const std::string idp_password("my_pass");
    const std::string idp_app_id("abc123def456");
    const std::string access_key("test_access_key");
    const std::string secret_key("test_secret_key");
    const std::string session_key("test_session_key");
    std::string resp_token_stream("{\"sessionToken\": \"longuniquesessiontoken\"}");
    std::string resp_saml_stream("<input name=\"SAMLResponse\" type=\"hidden\" value=\"long-saml-value-password\"/>");
    const char *saml_resp_str("long-saml-value-password");
}

class OktaSamlUtilTest : public testing::Test {
protected:
    std::shared_ptr<MOCK_HTTP_CLIENT> mock_http_client;
    std::shared_ptr<MOCK_STS_CLIENT> mock_sts_client;
    std::map<RDS_STR, RDS_STR> conn_attr;

    // Runs once per suite
    static void SetUpTestSuite() {
        AwsSdkHelper::Init();
    }
    static void TearDownTestSuite() {
        AwsSdkHelper::Shutdown();
    }

    // Runs per test
    void SetUp() override {
        conn_attr.insert_or_assign(KEY_IDP_ENDPOINT, idp_endpoint);
        conn_attr.insert_or_assign(KEY_IDP_PORT, idp_port);
        conn_attr.insert_or_assign(KEY_IDP_USERNAME, idp_username);
        conn_attr.insert_or_assign(KEY_IDP_PASSWORD, idp_password);
        conn_attr.insert_or_assign(KEY_IDP_ROLE_ARN, idp_role_arn);
        conn_attr.insert_or_assign(KEY_IDP_SAML_ARN, idp_saml_arn);
        conn_attr.insert_or_assign(KEY_APP_ID, idp_app_id);

        mock_http_client = std::make_shared<MOCK_HTTP_CLIENT>();
        mock_sts_client = std::make_shared<MOCK_STS_CLIENT>();
    }
    void TearDown() override {
        if (mock_sts_client) mock_sts_client.reset();
        if (mock_http_client) mock_http_client.reset();
    }
};

TEST_F(OktaSamlUtilTest, GetSamlAssertion_Success) {
    std::shared_ptr<MOCK_HTTP_RESP> session_token_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*session_token_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_token_stream);
    EXPECT_CALL(*session_token_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MOCK_HTTP_RESP> saml_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*saml_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> saml_body =
        std::make_shared<std::stringstream>(resp_saml_stream);
    EXPECT_CALL(*saml_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*saml_body));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(session_token_resp))
        .WillOnce(testing::Return(saml_resp));

    OktaSamlUtil okta_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ(saml_resp_str, okta_saml.c_str());
}

TEST_F(OktaSamlUtilTest, GetSamlAssertion_BadSessionToken) {
    std::shared_ptr<MOCK_HTTP_RESP> bad_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*bad_resp, GetResponseCode())
        .WillRepeatedly(testing::Return(Aws::Http::HttpResponseCode::NOT_FOUND));
    EXPECT_CALL(*bad_resp, HasClientError())
        .WillRepeatedly(testing::Return(true));
    Aws::String clientErrMsg("Bad Request");
    EXPECT_CALL(*bad_resp, GetClientErrorMessage())
        .WillRepeatedly(testing::ReturnRef(clientErrMsg));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(bad_resp));

    OktaSamlUtil okta_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", okta_saml.c_str());
}

TEST_F(OktaSamlUtilTest, GetSamlAssertion_BadSamlRequest) {
    std::shared_ptr<MOCK_HTTP_RESP> session_token_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*session_token_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_token_stream);
    EXPECT_CALL(*session_token_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MOCK_HTTP_RESP> bad_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*bad_resp, GetResponseCode())
        .WillRepeatedly(testing::Return(Aws::Http::HttpResponseCode::NOT_FOUND));
    EXPECT_CALL(*bad_resp, HasClientError())
        .WillRepeatedly(testing::Return(true));
    Aws::String clientErrMsg("Bad Request");
    EXPECT_CALL(*bad_resp, GetClientErrorMessage())
        .WillRepeatedly(testing::ReturnRef(clientErrMsg));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(session_token_resp))
        .WillOnce(testing::Return(bad_resp));

    OktaSamlUtil okta_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", okta_saml.c_str());
}

TEST_F(OktaSamlUtilTest, GetSamlAssertion_BadSamlResponse) {
    std::shared_ptr<MOCK_HTTP_RESP> session_token_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*session_token_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_token_stream);
    EXPECT_CALL(*session_token_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MOCK_HTTP_RESP> bad_saml_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*bad_saml_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> saml_body =
        std::make_shared<std::stringstream>("bad-saml-body");
    EXPECT_CALL(*bad_saml_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*saml_body));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(session_token_resp))
        .WillOnce(testing::Return(bad_saml_resp));

    OktaSamlUtil okta_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string okta_saml = okta_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", okta_saml.c_str());
}
