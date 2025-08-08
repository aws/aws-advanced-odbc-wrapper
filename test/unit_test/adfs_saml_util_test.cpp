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

#include "mock_objects.h"

#include "../../driver/plugin/federated/adfs_auth_plugin.h"
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
    const std::string idp_relay_party_id("urn:amazon:webservices");
    const std::string access_key("test_access_key");
    const std::string secret_key("test_secret_key");
    const std::string session_key("test_session_key");
    const std::string resp_stream(
        "<form method=\"post\" id=\"loginForm\" autocomplete=\"off\" novalidate=\"novalidate\" onKeyPress=\"if (event && event.keyCode == 13) Login.submitLoginRequest();\" action=\"/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=urn:amazon:webservices&client-request-id=1234-uuid-5678\">"
        "<input id=\"userNameInput\" name=\"UserName\" type=\"email\" value=\"\" tabindex=\"1\" class=\"text fullWidth\""
    );
    const std::string resp_saml_stream("<input type=\"hidden\" name=\"SAMLResponse\" value=\"long-saml-value-password\" /><noscript><p>Script is disabled. Click Submit to continue.</p><input type=\"submit\" value=\"Submit\" /></noscript></form><script language=\"javascript\">window.setTimeout('document.forms[0].submit()', 0);</script></body></html>");
    const char *saml_resp_str("long-saml-value-password");
}

class AdfsSamlUtilTest : public testing::Test {
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
        conn_attr.insert_or_assign(KEY_RELAY_PARTY_ID, idp_relay_party_id);

        mock_http_client = std::make_shared<MOCK_HTTP_CLIENT>();
        mock_sts_client = std::make_shared<MOCK_STS_CLIENT>();
    }
    void TearDown() override {
        if (mock_sts_client) mock_sts_client.reset();
        if (mock_http_client) mock_http_client.reset();
    }
};

TEST_F(AdfsSamlUtilTest, GetSamlAssertion_Success) {
    std::shared_ptr<MOCK_HTTP_RESP> resp;
    resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_stream);
    EXPECT_CALL(*resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MOCK_HTTP_RESP> saml_resp;
    saml_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*saml_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> saml_body =
        std::make_shared<std::stringstream>(resp_saml_stream);
    EXPECT_CALL(*saml_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*saml_body));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(resp))
        .WillOnce(testing::Return(saml_resp));

    AdfsSamlUtil adfs_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string adfs_saml = adfs_saml_util.GetSamlAssertion();
    EXPECT_STREQ(saml_resp_str, adfs_saml.c_str());
}

TEST_F(AdfsSamlUtilTest, GetSamlAssertion_BadRequest_Initial) {

    std::shared_ptr<MOCK_HTTP_RESP> bad_resp;
    bad_resp = std::make_shared<MOCK_HTTP_RESP>();

    EXPECT_CALL(*bad_resp, GetResponseCode())
        .WillRepeatedly(testing::Return(Aws::Http::HttpResponseCode::NOT_FOUND));
    EXPECT_CALL(*bad_resp, HasClientError())
        .WillRepeatedly(testing::Return(true));
    Aws::String clientErrMsg("Bad Request");
    EXPECT_CALL(*bad_resp, GetClientErrorMessage())
        .WillRepeatedly(testing::ReturnRef(clientErrMsg));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(bad_resp));

    AdfsSamlUtil adfs_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string adfs_saml = adfs_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", adfs_saml.c_str());
}

TEST_F(AdfsSamlUtilTest, GetSamlAssertion_BadRequest_ActionBody) {
    std::shared_ptr<MOCK_HTTP_RESP> bad_resp;
    bad_resp = std::make_shared<MOCK_HTTP_RESP>();

    EXPECT_CALL(*bad_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>("Fake Body - Bad Response");
    EXPECT_CALL(*bad_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(bad_resp));

    AdfsSamlUtil adfs_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string adfs_saml = adfs_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", adfs_saml.c_str());
}

TEST_F(AdfsSamlUtilTest, GetSamlAssertion_BadSamlResponse) {
    std::shared_ptr<MOCK_HTTP_RESP> resp;
    resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> resp_body =
        std::make_shared<std::stringstream>(resp_stream);
    EXPECT_CALL(*resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*resp_body));

    std::shared_ptr<MOCK_HTTP_RESP> bad_saml_resp;
    bad_saml_resp = std::make_shared<MOCK_HTTP_RESP>();
    EXPECT_CALL(*bad_saml_resp, GetResponseCode())
        .WillOnce(testing::Return(Aws::Http::HttpResponseCode::OK));
    std::shared_ptr<Aws::IOStream> saml_body =
        std::make_shared<std::stringstream>("Fake Body - Bad SAML Response");
    EXPECT_CALL(*bad_saml_resp, GetResponseBody())
        .WillOnce(testing::ReturnRef(*saml_body));

    EXPECT_CALL(*mock_http_client, MakeRequest(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(resp))
        .WillOnce(testing::Return(bad_saml_resp));

    AdfsSamlUtil adfs_saml_util(conn_attr, mock_http_client, mock_sts_client);
    std::string adfs_saml = adfs_saml_util.GetSamlAssertion();
    EXPECT_STREQ("", adfs_saml.c_str());
}
