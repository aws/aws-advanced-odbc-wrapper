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

#include <gtest/gtest.h>

#include "../../driver/plugin/federated/browser_auth_flow.h"

// Expose the protected static for testing.
class BrowserAuthFlowTest : public BrowserAuthFlow, public testing::Test {
protected:
    // Stub: never actually open a browser from a unit test.
    bool LaunchBrowser(const std::string&) override { return false; }
};

TEST_F(BrowserAuthFlowTest, SafeUrl_HttpsAccepted) {
    EXPECT_TRUE(IsSafeBrowserUrl("https://example.okta.com/home/amazon_aws/abc/272"));
}

TEST_F(BrowserAuthFlowTest, SafeUrl_QueryStringCharsAccepted) {
    // & ? = $ are literal inside the single-quoted shell command; RelayState URLs use them.
    EXPECT_TRUE(IsSafeBrowserUrl("https://sso.example.com/authorize?a=1&b=$x"));
}

TEST_F(BrowserAuthFlowTest, SafeUrl_LoopbackHttpAccepted) {
    // The Okta MFA form is served from the local listener over plain http.
    EXPECT_TRUE(IsSafeBrowserUrl("http://127.0.0.1:8080"));
}

TEST_F(BrowserAuthFlowTest, UnsafeUrl_PlainHttpRejected) {
    EXPECT_FALSE(IsSafeBrowserUrl("http://example.okta.com/login"));
}

TEST_F(BrowserAuthFlowTest, UnsafeUrl_SingleQuoteRejected) {
    EXPECT_FALSE(IsSafeBrowserUrl("https://example.com/'; rm -rf /'"));
}

TEST_F(BrowserAuthFlowTest, UnsafeUrl_ControlCharsRejected) {
    EXPECT_FALSE(IsSafeBrowserUrl("https://example.com/a\nb"));
    EXPECT_FALSE(IsSafeBrowserUrl("https://example.com/a\rb"));
    EXPECT_FALSE(IsSafeBrowserUrl("https://example.com/a\tb"));
}

TEST_F(BrowserAuthFlowTest, UnsafeUrl_EmptyRejected) {
    EXPECT_FALSE(IsSafeBrowserUrl(""));
}

TEST_F(BrowserAuthFlowTest, RunBrowserFlow_UnsafeUrlReturnsEmptyPayloads) {
    const BrowserFlowResult result = RunBrowserFlow("javascript:alert(1)", "state", "8080", "1");
    EXPECT_TRUE(result.auth_code.empty());
    EXPECT_TRUE(result.saml_response.empty());
}
