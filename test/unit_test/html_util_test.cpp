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

#include "../../driver/plugin/federated/html_util.h"

class HtmlUtilTest : public testing::Test {};

TEST_F(HtmlUtilTest, TestEscapeHtmlEntity) {
    const std::map<std::string, std::string> params = {
        {"&#x2b;",  "+" },
        {"&#x3d;",  "=" },
        {"&lt;",  "<" },
        {"&gt;",  ">" },
        {"&amp;",  "&" },
        {"&apos;",  "'" },
        {"&quot;",  "\"" },
        {"&#ax2b;",  "&#ax2b;" },
        {"&#ax2b;&#ax3d;",  "&#ax2b;&#ax3d;" },
        {"abc&#x2b;&#x3d;&lt;&gt;&amp;&apos;&quot;def",  "abc+=<>&\'\"def" },
        {"abc&#ax2b;&#x3d;\n&lt;&gt;&amp;&apos;&quot;def",  "abc&#ax2b;=\n<>&\'\"def" },
        {"",  "" },
    };

    for (auto pair: params) {
        EXPECT_EQ(pair.second, HtmlUtil::EscapeHtmlEntity(pair.first));
    }
}
