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

#ifndef BROWSER_AUTH_FLOW_H_
#define BROWSER_AUTH_FLOW_H_

#include <string>

/*
* Mixin base for interactive browser-based auth flows (Okta browser SAML, AWS IAM
* Identity Center PKCE). Owns the shared round-trip: validate the URL, start the
* localhost redirect listener, wait for it to bind, open the system browser, and
* join. Subclasses pick which payload they need from the result.
*/
class BrowserAuthFlow {
protected:
    virtual ~BrowserAuthFlow() = default;

    struct BrowserFlowResult {
        // Authorization code from a GET/POST "code" parameter (PKCE flow), empty if absent.
        std::string auth_code;
        // Raw (still URL-encoded) "SAMLResponse" POST field (SAML flow), empty if absent.
        std::string saml_response;
    };

    /*
    * Run the full browser round-trip against `url`. `state` is the CSRF token the
    * listener validates on redirects that echo it. `listen_port` and `timeout_secs`
    * configure the localhost listener. Returns empty payloads on any failure
    * (unsafe URL, listener failed to bind, browser failed to open, timeout).
    */
    BrowserFlowResult RunBrowserFlow(const std::string& url, const std::string& state,
                                     const std::string& listen_port, const std::string& timeout_secs);

    /*
    * Open `url` in the system browser. Virtual so tests can stub the browser.
    * Returns false if the browser could not be launched.
    */
    virtual bool LaunchBrowser(const std::string& url);

    /*
    * A URL is safe to hand to the browser launcher only if it is https (or http on
    * the loopback host, used by the local MFA form) and cannot break out of the
    * single-quoted shell command used on Unix: reject a single quote or any control
    * char. Query-string chars (& ? = $) are literal inside single quotes, so
    * legitimate redirect/RelayState URLs pass.
    */
    static bool IsSafeBrowserUrl(const std::string& url);
};

#endif // BROWSER_AUTH_FLOW_H_
