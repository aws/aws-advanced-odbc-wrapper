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

#ifndef RDS_STRINGS_H_
#define RDS_STRINGS_H_

#ifdef WIN32
    // TEXT() Define already in <windows.h>
    #include <windows.h> // Required to include sql.h
    #include <tchar.h>
#else // Unix Platforms
    #define TEXT(x) x
#endif // WIN32

#include <sql.h>

#include <regex>
#include <sstream>
#include <string.h>
#include <vector>

#ifdef UNICODE
    #include "unicode/unistr.h"
#endif

#define AS_SQLTCHAR(str) const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(str.c_str()))
#define AS_CHAR(str) (reinterpret_cast<char *>(str))
#define AS_CONST_CHAR(str) (reinterpret_cast<const char *>(str))
#define AS_WCHAR(str) (reinterpret_cast<wchar_t *>(str))
#define AS_CONST_WCHAR(str) (reinterpret_cast<const wchar_t*>(str))

#ifdef WIN32
    #define STR_ICMP(str1, str2) strcmpi(str1, str2)
#else
    #define STR_ICMP(str1, str2) strcasecmp(str1, str2)
#endif

#include <cstring>
typedef std::string RDS_STR;
typedef std::ostringstream RDS_STR_STREAM;
typedef char RDS_CHAR;
#define RDS_STR_LEN(str) strlen(str)
typedef std::regex RDS_REGEX;
typedef std::smatch RDS_MATCH;
#define RDS_sprintf(buffer, max_length, format, ...) snprintf(buffer, max_length, format, __VA_ARGS__)
#define RDS_CHAR_FORMAT "%s"
#define RDS_NUM_APPEND(str, num) str.append(std::to_string(num))

#include "unicode/utypes.h"
#include "unicode/ucasemap.h"
inline std::string RDS_STR_UPPER(std::string str) {
    size_t buf_len = str.length() * 4;
    char *buf = new char[buf_len];
    UErrorCode ucasemapStatus = U_ZERO_ERROR;
    UCaseMap *ucasemap = ucasemap_open(NULL, 0, &ucasemapStatus);
    UErrorCode upperStatus = U_ZERO_ERROR;
    ucasemap_utf8ToUpper(ucasemap, buf, buf_len, str.c_str(), -1, &upperStatus);
    std::string upper(buf);
    delete[] buf;
    return upper;
}

#define EMPTY_RDS_STR ""
#define AS_RDS_CHAR(str) (reinterpret_cast<RDS_CHAR *>(str))
#define AS_RDS_STR(str) RDS_STR((RDS_CHAR *) str)
#define AS_RDS_STR_MAX(str, len) RDS_STR((RDS_CHAR *) str, len)

inline RDS_STR ToRdsStr(const std::string &str)
{
    return str;
}

inline std::string ToStr(const RDS_STR &str)
{
    return str;
}

inline RDS_STR TrimStr(RDS_STR &str) {
    str = str.erase(str.find_last_not_of(TEXT(' ')) + 1);
    str = str.erase(0, str.find_first_not_of(TEXT(' ')));
    return str;
}

inline std::vector<RDS_STR> SplitStr(RDS_STR &str, RDS_STR &delimiter) {
    RDS_REGEX pattern(delimiter);
    RDS_MATCH match;
    RDS_STR str_itr = str;
    std::vector<RDS_STR> matches;
    while (std::regex_search(str_itr, match, pattern)) {
        matches.push_back(match.prefix().str());
        str_itr = match.suffix().str();
    }

    if (matches.empty()) {
        matches.push_back(str);
    }

    return matches;
}

#endif // RDS_STRINGS_H_
