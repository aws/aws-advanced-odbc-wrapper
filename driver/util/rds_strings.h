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
    #ifdef UNICODE
        #define TEXT(x) L##x
    #else // Ansi
        #define TEXT(x) x
    #endif // UNICODE
#endif // WIN32

#include <sql.h>

#include <regex>
#include <string>
#include <sstream>

#define AS_SQLTCHAR(str) (const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(str)))
#define AS_CHAR(str) (reinterpret_cast<char *>(str))
#define AS_CONST_CHAR(str) (reinterpret_cast<const char *>(str))
#define AS_WCHAR(str) (reinterpret_cast<wchar_t *>(str))
#define AS_CONST_WCHAR(str) (reinterpret_cast<const wchar_t*>(str))

#ifdef WIN32
    #define STR_ICMP(str1, str2) strcmpi(str1, str2)
#else
    #define STR_ICMP(str1, str2) strcasecmp(str1, str2)
#endif

#ifdef UNICODE
    #include <cwctype>
    #include <wchar.h>
    typedef std::wstring RDS_STR;
    typedef std::wostringstream RDS_STR_STREAM;
    typedef wchar_t RDS_CHAR;
    #define TO_UPPER(c) std::towupper(c)
    #define RDS_STR_LEN(str) wcslen(str)
    typedef std::wregex RDS_REGEX;
    typedef std::wsmatch RDS_MATCH;
    #define RDS_sprintf(buffer, max_length, format, ...) swprintf(buffer, max_length, format, __VA_ARGS__)
    #define RDS_CHAR_FORMAT TEXT("%ws")
#else
    #include <cstring>
    typedef std::string RDS_STR;
    typedef std::ostringstream RDS_STR_STREAM;
    typedef char RDS_CHAR;
    #define TO_UPPER(c) std::toupper(c)
    #define RDS_STR_LEN(str) strlen(str)
    typedef std::regex RDS_REGEX;
    typedef std::smatch RDS_MATCH;
    #define RDS_sprintf(buffer, max_length, format, ...) snprintf(buffer, max_length, format, __VA_ARGS__)
    #define RDS_CHAR_FORMAT TEXT("%s")
#endif

#define EMPTY_RDS_STR TEXT("")
#define AS_RDS_CHAR(str) (reinterpret_cast<RDS_CHAR *>(str))
#define AS_RDS_STR(str) RDS_STR((RDS_CHAR *) str)
#define AS_RDS_STR_MAX(str, len) RDS_STR((RDS_CHAR *) str, len)
#define RDS_STR_UPPER(str) std::transform(str.begin(), str.end(), str.begin(), [](RDS_CHAR c) {return TO_UPPER(c);});

inline RDS_STR ToRdsStr(const std::string &str)
{
    RDS_STR converted;
#ifdef UNICODE
    size_t new_len = str.size() * 2 + 2;
    wchar_t* buf = new wchar_t[new_len];
    swprintf(buf, new_len, L"%S", str.c_str());
    converted = buf;
    delete[] buf;
#else
    converted = str;
#endif
    return converted;
}

// TODO - Fix lossy conversion
inline std::string ToStr(const RDS_STR &str)
{
#ifdef UNICODE
    std::string ret(str.length(), 0);
    std::transform(str.begin(), str.end(), ret.begin(), [] (wchar_t c) {
        return (char)c;
    });
    return ret;
#else
    return str;
#endif
}

#endif // RDS_STRINGS_H_
