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

#include "logger_wrapper.h"
#include "unicode/utypes.h"
#include "unicode/ucasemap.h"

#ifdef UNICODE
#include "unicode/unistr.h"
inline size_t ushort_strlen(const unsigned short* str) {
    size_t length = 0;
    while (str[length] != 0) {
        length++;
    }
    return length;
}

inline std::vector<unsigned short> convertUTF8ToUTF16(std::string input) {
    icu::StringPiece string_piece(input);
    icu::UnicodeString string_utf16 = icu::UnicodeString::fromUTF8(string_piece);
    unsigned short *ushort_string = (unsigned short *)(string_utf16.getBuffer());
    size_t size = ushort_strlen(ushort_string);
    std::vector<unsigned short> ushort_vec(ushort_string, ushort_string + size);
    return ushort_vec;
}

inline std::string convertUTF16ToUTF8(unsigned short *buffer_utf16) {
    icu::UnicodeString unicode_str(reinterpret_cast<const char16_t*>(buffer_utf16));
    std::string buffer_utf8;
    unicode_str.toUTF8String(buffer_utf8);
    return buffer_utf8;
}
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

inline std::string RDS_STR_UPPER(std::string str) {
    size_t buf_len = str.length() * 4;
    char *buf = new char[buf_len];
    UErrorCode ucasemap_status = U_ZERO_ERROR;
    UCaseMap *ucasemap = ucasemap_open(NULL, 0, &ucasemap_status);
    if (U_FAILURE(ucasemap_status)) {
       LOG(ERROR) << std::format("Failed to convert string {} to uppercase when opening ucasemap: {}", str, u_errorName(ucasemap_status)); 
       return str;
    }
    UErrorCode upper_status = U_ZERO_ERROR;
    ucasemap_utf8ToUpper(ucasemap, buf, buf_len, str.c_str(), -1, &upper_status);
    if (U_FAILURE(upper_status)) {
       LOG(ERROR) << std::format("Failed to convert string {} to uppercase: {}\n", str, u_errorName(upper_status)); 
       return str;
    }
    std::string upper(buf);
    delete[] buf;
    return upper;
}

#define EMPTY_RDS_STR ""
#define AS_RDS_CHAR(str) (reinterpret_cast<RDS_CHAR *>(str))
#define AS_RDS_STR(str) RDS_STR((RDS_CHAR *) str)
#define AS_RDS_STR_MAX(str, len) RDS_STR((RDS_CHAR *) str, len)

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
