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

#include <unordered_map>

enum class STATE
{
    PARSE_REQUEST,
    PARSE_HEADER,
    PARSE_BODY,
    PARSE_FINISHED,
    PARSE_GET_REQUEST
};

enum class STATUS
{
    SUCCEED,
    FAILED,
    EMPTY_REQUEST,
    GET_SUCCESS
};

/*
* This class is used to parse the HTTP POST request
* and retrieve authorization code.
*/
class Parser {
    public:

        Parser();

        ~Parser() = default;

        /*
        * Initiate request parsing.
        *
        * Return true if parse was successful, otherwise false.
        */
        STATUS Parse(std::istream &in);
        
        /*
        * Check if parser is finished to parse the POST request.
        *
        * Return true if parsing was successfully finished, otherwise false.
        */
        bool IsFinished() const;
        
        /*
        * Retrieve authorization code.
        *
        * Return received authorization code or empty string.
        */
        std::string RetrieveAuthCode(std::string& state);
        
    private:
        /*
        * Parse received POST request line by line.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParsePostRequest(std::string& str);
                
        /*
        * Parse request-line and perform verification for:
        * method, Request-URI and HTTP-Version.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParseRequestLine(std::string& str);
                
        /*
        * Parse request header line.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParseHeaderLine(std::string& str);
                
        /*
        * Parse request body line in application/x-www-form-urlencoded format.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParseBodyLine(std::string& str);

        /*
        * Expected METHOD, URI and HTTP VERSION in request line.
        */
        const std::string METHOD = "POST";
        // const std::string URI = "/redshift/";
        const std::string HTTP_VERSION = "HTTP/1.1";
        const int MAX_HEADER_SIZE = 8192;

        const std::string GET_METHOD = "GET";
        const std::string PKCE_URI = "/?code=";
                        
        STATE parser_state_;
        size_t header_size_;
        std::unordered_map<std::string, std::string> header_;
        std::unordered_map<std::string, std::string> body_;
                        
};
