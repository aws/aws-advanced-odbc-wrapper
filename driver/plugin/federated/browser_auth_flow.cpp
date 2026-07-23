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

#if (defined(_WIN32) || defined(_WIN64))
    #include <windows.h>
    #include <shellapi.h>
    #undef GetObject
#endif

#include "browser_auth_flow.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

#include "http/WEBServer.h"

#include "../../util/logger_wrapper.h"
#include "../../util/rds_strings.h"

bool BrowserAuthFlow::IsSafeBrowserUrl(const std::string& url)
{
    if (!url.starts_with("https://") && !url.starts_with("http://127.0.0.1:")) {
        return false;
    }
    // Reject single quotes (the POSIX launch paths shell-quote the URL) and ASCII control characters.
    constexpr unsigned char MIN_PRINTABLE_ASCII = 0x20;
    return std::ranges::all_of(url, [](const char c) {
        return c != '\'' && static_cast<unsigned char>(c) >= MIN_PRINTABLE_ASCII;
    });
}

bool BrowserAuthFlow::LaunchBrowser(const std::string& url)
{
#if (defined(_WIN32) || defined(_WIN64))
    // ShellExecute returns a value greater than 32 on success (per the Win32 API contract).
    constexpr intptr_t SHELL_EXECUTE_SUCCESS_THRESHOLD = 32;
    HINSTANCE result = ShellExecute(nullptr, RDS_TSTR(std::string("open")).c_str(),
        RDS_TSTR(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > SHELL_EXECUTE_SUCCESS_THRESHOLD;
#elif (defined(LINUX) || defined(__linux__))
    return system(("xdg-open '" + url + "'").c_str()) == 0; // NOLINT(bugprone-command-processor)
#else
    return system(("open '" + url + "'").c_str()) == 0; // NOLINT(bugprone-command-processor)
#endif
}

BrowserAuthFlow::BrowserFlowResult BrowserAuthFlow::RunBrowserFlow(
    const std::string& url, const std::string& state,
    const std::string& listen_port, const std::string& timeout_secs)
{
    BrowserFlowResult result;

    if (!IsSafeBrowserUrl(url)) {
        LOG(ERROR) << "Refusing to open unsafe URL for browser authentication";
        return result;
    }

    std::string state_copy = state;
    std::string port_copy = listen_port;
    std::string timeout_copy = timeout_secs;
    WEBServer srv(state_copy, port_copy, timeout_copy);

    srv.LaunchServer();

    // Wait until the listener thread has actually bound the socket before opening the
    // browser. Otherwise a fast browser redirect can hit the port before it is listening
    // and the response is lost. Mirrors the Redshift driver's WaitForServer guard.
    constexpr int SERVER_START_TIMEOUT_MS = 10000;
    constexpr int POLL_INTERVAL_MS = 50;
    int waited_ms = 0;
    while (!srv.IsListening() && waited_ms < SERVER_START_TIMEOUT_MS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
        waited_ms += POLL_INTERVAL_MS;
    }
    if (!srv.IsListening()) {
        srv.Cancel();
        srv.Join();
        LOG(ERROR) << "Browser authentication: local listener failed to start within timeout";
        return result;
    }

    LOG(INFO) << "Browser authentication: opening " << url;
    try {
        if (!LaunchBrowser(url)) {
            srv.Cancel();
        }
    } catch (const std::exception& e) {
        srv.Cancel();
        srv.Join();
        LOG(ERROR) << "Could not open browser for authentication: " << e.what();
        return result;
    }

    srv.Join();

    result.auth_code = srv.GetCode();
    result.saml_response = srv.GetSamlResponse();
    return result;
}
