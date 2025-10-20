
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

#include "html_util.h"

#include <ng-log/logging.h>

#include "../../util/logger_wrapper.h"

const std::unordered_map<std::string, char> HtmlUtil::HTML_DECODE_MAP = {
    {"&#x2b;",  '+' },
    {"&#x3d;",  '=' },
    {"&lt;",    '<' },
    {"&gt;",    '>' },
    {"&amp;",   '&' },
    {"&apos;",  '\''},
    {"&quot;",  '"' }
};

std::string HtmlUtil::EscapeHtmlEntity(const std::string& html) {
    std::string retval;
    DLOG(INFO) << "Before HTML escape modification: " << html;
    int i = 0;
    const int length = html.length();
    while (i < length) {
        const char c = html[i];
        if (c != '&') {
            retval.append(1, c);
            i++;
            continue;
        }

        size_t semicolon_idx = html.find(';', i);
        if (semicolon_idx != std::string::npos) {
            std::string html_code = html.substr(i, semicolon_idx - i + 1);
            if (auto itr = HtmlUtil::HTML_DECODE_MAP.find(html_code); itr != HtmlUtil::HTML_DECODE_MAP.end()) {
                retval.append(1, itr->second);
                i = semicolon_idx + 1;
            }
        } else {
            retval.append(1, c);
            i++;
        }

    }
    DLOG(INFO) << "After HTML escape modification: " << html;
    return retval;
}
