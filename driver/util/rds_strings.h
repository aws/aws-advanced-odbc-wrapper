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
inline size_t UShortStrlen(const uint16_t* str) {
    size_t length = 0;
    while (str[length] != 0) {
        length++;
    }
    return length;
}

inline std::wstring ConvertUTF8ToWString(std::string input) {
    icu::StringPiece string_piece(input.c_str(), input.length());
    icu::UnicodeString string_utf16 = icu::UnicodeString::fromUTF8(string_piece);

    int32_t size;
    UErrorCode error = U_ZERO_ERROR;
    u_strToWCS(nullptr, 0, &size, string_utf16.getBuffer(), string_utf16.length(), &error);

    error = U_ZERO_ERROR; // Reset error
    std::wstring wstr(size, 0);
    u_strToWCS(wstr.data(), wstr.size(), nullptr, string_utf16.getBuffer(), string_utf16.length(), &error);

    return wstr;
}

inline std::vector<uint16_t> ConvertUTF8ToUTF16(std::string input) {
    icu::StringPiece string_piece(input.c_str(), input.length());
    icu::UnicodeString string_utf16 = icu::UnicodeString::fromUTF8(string_piece);
    uint16_t *ushort_string = reinterpret_cast<uint16_t*>(const_cast<char16_t*>(string_utf16.getTerminatedBuffer()));
    size_t size = UShortStrlen(ushort_string);
    std::vector<uint16_t> ushort_vec(ushort_string, ushort_string + size);
    // Insert null terminator because vector.data() returns NULL when empty
    ushort_vec.push_back(0);
    return ushort_vec;
}

// Assumes that the passed in vec is null terminated and was produced by ConvertUTF8ToUTF16
inline int CopyUTF16StringToBuffer(uint16_t* buf, size_t buf_len, std::vector<uint16_t> vec) {
    size_t end = buf_len < vec.size() ? buf_len : vec.size();
    std::copy(vec.begin(), vec.begin() + end, buf);
    if (end > 0) {
        buf[end - 1] = 0;
    }
    return vec.size() - 1;
}

inline int CopyUTF8ToUTF16Buffer(uint16_t* buf, size_t buf_len, std::string str) {
    return CopyUTF16StringToBuffer(buf, buf_len, ConvertUTF8ToUTF16(str));
}

// The input string buffer is assumed to be null terminated
inline std::string ConvertUTF16ToUTF8(uint16_t *buffer_utf16) {
    icu::UnicodeString unicode_str(reinterpret_cast<const char16_t*>(buffer_utf16));
    std::string buffer_utf8;
    unicode_str.toUTF8String(buffer_utf8);
    return buffer_utf8;
}
#endif

#ifdef UNICODE
    #define AS_SQLTCHAR(str) const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(ConvertUTF8ToUTF16(str).data()))
    #define AS_UTF8_CSTR(str) ConvertUTF16ToUTF8(reinterpret_cast<uint16_t *>(str)).c_str()
    #define RDS_TSTR(str) ConvertUTF8ToWString(str)
#else
    #define AS_SQLTCHAR(str) const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(str.c_str()))
    #define AS_UTF8_CSTR(str) reinterpret_cast<const char *>(str)
    #define RDS_TSTR(str) str
#endif

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

inline std::string RDS_STR_UPPER(std::string str) {
    if (!str.empty()) {
        size_t buf_len = str.length() * 4;
        char *buf = new char[buf_len];
        UErrorCode ucasemap_status = U_ZERO_ERROR;
        UCaseMap *ucasemap = ucasemap_open(NULL, 0, &ucasemap_status);
        if (U_FAILURE(ucasemap_status)) {
            LOG(ERROR) << std::format("Failed to convert string {} to uppercase when opening ucasemap: {}", str, u_errorName(ucasemap_status));
            delete[] buf;
            return str;
        }
        UErrorCode upper_status = U_ZERO_ERROR;
        ucasemap_utf8ToUpper(ucasemap, buf, buf_len, str.c_str(), -1, &upper_status);
        if (U_FAILURE(upper_status)) {
            LOG(ERROR) << std::format("Failed to convert string {} to uppercase: {}\n", str, u_errorName(upper_status));
            ucasemap_close(ucasemap);
            delete[] buf;
            return str;
        }
        std::string upper(buf);
        ucasemap_close(ucasemap);
        delete[] buf;
        return upper;
    }
    return str;
}

#define EMPTY_RDS_STR ""

inline std::string TrimStr(std::string &str) {
    str = str.erase(str.find_last_not_of(TEXT(' ')) + 1);
    str = str.erase(0, str.find_first_not_of(TEXT(' ')));
    return str;
}

inline std::vector<std::string> SplitStr(std::string &str, std::string &delimiter) {
    const std::regex pattern(delimiter);
    std::smatch match;
    std::string str_itr = str;
    std::vector<std::string> matches;
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
