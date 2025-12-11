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

#include "../../../util/logger_wrapper.h"
#include "Parser.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iterator>
#include <sstream>
#include <thread>

void Parser::ParseRequestLine(std::string& str)
{
    std::istringstream istr(str);
    std::vector<std::string> command(
        (std::istream_iterator<std::string>(istr)),
        std::istream_iterator<std::string>()
    );

    if ((command.size() == 3) && (command[0].find(METHOD) != std::string::npos) &&
        (command[2].find(HTTP_VERSION) != std::string::npos)) {
        parser_state_ = STATE::PARSE_HEADER;
    } else if ((command.size() == 3) &&
        (command[0].find(GET_METHOD) != std::string::npos) &&
        command[2].find(HTTP_VERSION) != std::string::npos) {
        const size_t question_mark_pos = command[1].find('?');
        if (question_mark_pos != std::string::npos) {
            std::string parsed_body = command[1].substr(question_mark_pos + 1);
            ParseBodyLine(parsed_body);
        } else {
            parser_state_ = STATE::PARSE_GET_REQUEST;
        }
    } else {
        throw std::runtime_error("Request line contains wrong information.");
    }
}

void Parser::ParseHeaderLine(std::string& str)
{
    if (str == "\r") {
        parser_state_ = STATE::PARSE_BODY;

        return;
    }

    const size_t ind = str.find(':');
    header_size_ += str.size();

    if ((ind == std::string::npos) || (header_size_ > MAX_HEADER_SIZE)) {
        throw std::runtime_error("Received invalid header line.");
    }

    str.erase(std::remove_if(str.begin() + static_cast<int>(ind), str.end(), ::isspace), str.end());

    header_.insert({
        std::string(str.begin(), str.begin() + static_cast<int>(ind)),
        std::string(str.begin() + static_cast<int>(ind) + 1, str.end())
    });
}

void Parser::ParseBodyLine(std::string& str)
{
    auto str_begin = str.begin();
    auto str_end = str.end();

    while (true) {
        auto equal_it = find(str_begin, str_end, '=');
        auto ampersand_it = find(str_begin, str_end, '&');

        if (equal_it == str_end) {
            throw std::runtime_error("Received invalid body line.");
        }

        body_.insert({
            std::string(str_begin, equal_it),
            std::string(equal_it + 1, ampersand_it)
        });

        if ((equal_it == str_end) || (ampersand_it == str_end)) {
            break;
        }

        str_begin = ampersand_it + 1;
    }

    parser_state_ = STATE::PARSE_FINISHED;
}

void Parser::ParsePostRequest(std::string& str)
{
    switch (parser_state_) {
        case STATE::PARSE_REQUEST:
            ParseRequestLine(str);
            break;
        case STATE::PARSE_HEADER:
            ParseHeaderLine(str);
            break;
        case STATE::PARSE_BODY:
            if (!header_.contains("Content-Type") || !header_.contains("Content-Length") ||
                (header_["Content-Type"] != "application/x-www-form-urlencoded") ||
                (header_["Content-Length"] != std::to_string(str.size())))
            {
                throw std::runtime_error("Can't start parsing body as header contains invalid information.");
            }

            ParseBodyLine(str);
            break;
        default:
            break;
    }
}

STATUS Parser::Parse(std::istream &in)
{
    std::string str;
    bool is_line_received = false;

    while (getline(in, str) && parser_state_ != STATE::PARSE_GET_REQUEST) {
        is_line_received = true;

        try {
            ParsePostRequest(str);
        } catch (std::exception& e) {
            DLOG(INFO) << e.what();
            break;
        }
    }

    if (parser_state_ == STATE::PARSE_GET_REQUEST) {
        parser_state_ = STATE::PARSE_REQUEST;
        return STATUS::GET_SUCCESS;
    }

    if (parser_state_ != STATE::PARSE_FINISHED) {
        return is_line_received ? STATUS::FAILED : STATUS::EMPTY_REQUEST;
    }

    return STATUS::SUCCEED;
}

Parser::Parser()
    : parser_state_(STATE::PARSE_REQUEST)
    , header_size_(0)
{
    ; // Do nothing.
}

bool Parser::IsFinished() const
{
    return parser_state_ == STATE::PARSE_FINISHED;
}

std::string Parser::RetrieveAuthCode(std::string& state)
{
    if (body_.contains("code"))
    {
        return body_["code"];
    }

    return "";
}
