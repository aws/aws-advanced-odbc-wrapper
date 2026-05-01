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

inline size_t GetLenOfSqltcharArray(SQLTCHAR *in, SQLLEN buffer_len, bool use_4_bytes) {
    if (buffer_len > 0) {
        return static_cast<int>(buffer_len);
    }

    if (buffer_len == SQL_NTS) {
        bool end_found = false;
        size_t len = 0;
        int i = 0;

        while (!end_found) {
            if (!use_4_bytes) {
                if (in[i] == '\0') {
                    end_found = true;
                }
                i++;
            } else {
                if (in[i] == '\0' && in[i + 1] == '\0') {
                    end_found = true;
                }
                i += 2;
            }
            len++;
        }

        return len;
    }

    return 0;
}

#ifdef UNICODE
#include "unicode/unistr.h"
inline size_t UShortStrlen(const uint16_t* str, const bool use_4_byte = false) {
    size_t length = 0;
    if (!str) {
        return length;
    }

    if (use_4_byte) {
        while (str[length * 2] != 0 || str[length * 2 + 1] != 0) {
            length++;
        }
    } else {
        while (str[length] != 0) {
            length++;
        }
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

inline std::vector<uint16_t>  ConvertUTF8ToUTF16(std::string input) {
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

// Expand UTF16 (2-byte) into UTF32 (4-byte)
inline size_t ConvertUTF16ToUTF32(const SQLTCHAR* src, SQLTCHAR* dst, const size_t src_len, const size_t dst_len) {
    if (src == nullptr || dst == nullptr || dst_len < 2) {
        return 0;
    }

    const size_t max_copy = (dst_len - 2) / 2;
    const size_t written = src_len < max_copy ? src_len : max_copy;
    for (size_t i = 0; i < written; i++) {
        dst[i * 2] = src[i];
        dst[(i * 2) + 1] = 0;
    }
    dst[written * 2] = 0;
    dst[(written * 2) + 1] = 0;

    return written;
}

inline void ExpandUTF16ToUTF32InPlace(SQLTCHAR* buf, size_t src_chars, size_t buf_slots) {
    if (buf == nullptr || src_chars == 0) {
        return;
    }

    const size_t max_chars = (buf_slots >= 2) ? (buf_slots - 2) / 2 : 0;
    const size_t chars = src_chars < max_chars ? src_chars : max_chars;

    // Work backwards to avoid overwriting source data
    for (size_t i = chars; i > 0; --i) {
        buf[((i - 1) * 2) + 1] = 0;
        buf[(i - 1) * 2] = buf[i - 1];
    }
    buf[chars * 2] = 0;
    buf[(chars * 2) + 1] = 0;
}

inline std::string Convert4ByteSqlWChar(
    SQLTCHAR *     InputStr,
    SQLINTEGER     BufferLength
    ) {
    std::vector<SQLTCHAR> conn_in_vector;
    int i = 0;
    while (true) {
        if (BufferLength > 0 && (i / 2) >= BufferLength) {
            break;
        }
        if (InputStr[i] == 0 && InputStr[i + 1] == 0) {
            break;
        }
        conn_in_vector.push_back(InputStr[i]);
        i += 2;
    }
    conn_in_vector.push_back(0);

    const std::string str_utf8_w = ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(conn_in_vector.data()));
    return str_utf8_w;
}

inline std::string ConvertUserAppToUTF8(bool user_4_byte, SQLTCHAR* in, SQLINTEGER in_length) {
    if (user_4_byte) {
        size_t length = GetLenOfSqltcharArray(in, in_length, user_4_byte);
        return Convert4ByteSqlWChar(in, length);
    }
    return ConvertUTF16ToUTF8(reinterpret_cast<uint16_t*>(in));
}

inline void ConvertUTF8ToDriver(bool driver_4_byte, std::string input, SQLTCHAR* out, SQLSMALLINT out_length) {
    if (out_length <= 0) {
        return;
    }
    if (driver_4_byte) {
        std::wstring w_input = ConvertUTF8ToWString(input);
        size_t copy_size = static_cast<size_t>(out_length) > w_input.length()
            ? w_input.length()
            : static_cast<size_t>(out_length) - 1; // For null terminating character
        std::copy(w_input.begin(), w_input.begin() + copy_size, out);
        out[copy_size] = 0;
    } else {
        CopyUTF8ToUTF16Buffer(reinterpret_cast<uint16_t*>(out), out_length, input);
    }
}

inline std::vector<SQLTCHAR> ConvertUserAppInputToBaseDriver(bool user_4_byte, bool driver_4_byte, SQLTCHAR* in, SQLINTEGER in_length) {
    // nullptr is valid ODBC input
    if (in == nullptr) {
        return std::vector<SQLTCHAR>();
    }

    const std::string utf8 = ConvertUserAppToUTF8(user_4_byte, in, in_length);
    if (driver_4_byte) {
        std::vector<uint16_t> utf16 = ConvertUTF8ToUTF16(utf8);
        size_t utf16_len = utf16.size() > 0 ? utf16.size() - 1 : 0;

        size_t size;
        if (in_length == SQL_NTS || in_length < 0) {
            size = utf16_len;
        } else {
            size = static_cast<size_t>(in_length) < utf16_len
                ? static_cast<size_t>(in_length)
                : utf16_len;
        }

        size_t size_converted = size * 2 + 2; // Each char expands to 2 SQLTCHAR + null pair
        SQLTCHAR* wide_converted_4byte = new SQLTCHAR[size_converted];
        ConvertUTF16ToUTF32(reinterpret_cast<const SQLTCHAR*>(utf16.data()), wide_converted_4byte, size, size_converted);
        std::vector<SQLTCHAR> result(wide_converted_4byte, wide_converted_4byte + size_converted);
        delete[] wide_converted_4byte;
        return result;
    } else {
        std::vector<uint16_t> utf16 = ConvertUTF8ToUTF16(utf8);
        return std::vector<SQLTCHAR>(
            reinterpret_cast<SQLTCHAR*>(utf16.data()),
            reinterpret_cast<SQLTCHAR*>(utf16.data() + utf16.size()));
    }
}
#endif

#ifdef UNICODE
    #define AS_SQLTCHAR(str) const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(ConvertUTF8ToUTF16(str).data()))
    #define AS_UTF8_CSTR(str) ConvertUTF16ToUTF8(reinterpret_cast<uint16_t *>(str)).data()
    #define RDS_TSTR(str) ConvertUTF8ToWString(str)
#else
    #define AS_SQLTCHAR(str) const_cast<SQLTCHAR *>(reinterpret_cast<const SQLTCHAR *>(str.data()))
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

inline void Convert4To2ByteString(bool use_4_bytes, SQLTCHAR *in, SQLTCHAR *out, size_t len) {
    if (in != nullptr && len > 0) {
        for (int i = 0; i < len - 1; i++) {
            if (use_4_bytes) {
                if (out != nullptr) {
                    out[i] = in[i * 2];
                } else {
                    in[i] = in[i * 2];
                }
            } else {
                if (out != nullptr) {
                    out[i] = in[i];
                }
            }
        }
        if (out != nullptr) {
            out[len - 1] = '\0';
        } else {
            in[len - 1] = '\0';
        }
    }
}

#endif // RDS_STRINGS_H_
