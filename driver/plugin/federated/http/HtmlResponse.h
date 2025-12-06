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

#pragma once

inline std::string GetResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 300\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html; charset=utf-8\r\n\r\n"
    "<!DOCTYPE html><html><body><h2>AWS Advanced ODBC Wrapper Okta Plugin MFA Authentication Code</h2>"
    "<form action='http://127.0.0.1:{}' method='post'>"
    "<label for='code'>Authentication Code:</label><br>"
    "<input type='text' id='code' name='code'><br>"
    "<input type='submit' value='Submit'>"
    "</form></body></html>";

const std::string ValidResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 300\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html; charset=utf-8\r\n\r\n"
    "<!DOCTYPE html><html><body><p style='font: italic bold 30px Arial,sans-serif;"
    "background-color: #edde9c;color: #bc2e1e;background-color: #fff;color:#202c2d;"
    "text-shadow:0 1px #808d93,-1px 0 #cdd2d5,-3px 4px #cdd2d5;'>"
    "Thank you for using the AWS Advanced ODBC Wrapper! You can now close this window.</p></body></html>";

const std::string InvalidResponse =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Length: 95\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html; charset=utf-8\r\n\r\n"
    "<!DOCTYPE html><html><body>The request could not be understood by the server!</p></body></html>";
