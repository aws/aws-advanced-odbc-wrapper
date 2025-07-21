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

#define AS_SQLTCHAR(str) (const_cast<SQLTCHAR*>(reinterpret_cast<const SQLTCHAR*>(str)))
#define AS_CHAR(str) (reinterpret_cast<char*>(str))
#define AS_CONST_CHAR(str) (reinterpret_cast<const char*>(str))
#define AS_WCHAR(str) (reinterpret_cast<wchar_t*>(str))
#define AS_CONST_WCHAR(str) (reinterpret_cast<const wchar_t*>(str))
#define CONCATENATE(e1, e2) e1 ## e2

#ifdef UNICODE
    #define CONSTRUCT_SQLSTR(s) CONCATENATE(L, s) // Return L"s"
    typedef std::wstring RDS_STR;
    typedef std::wostringstream RDS_STR_STREAM;
    typedef wchar_t RDS_CHAR;
    #define TO_UPPER(c) std::towupper(c)
    typedef std::wregex RDS_REGEX;
    typedef std::wsmatch RDS_MATCH;
#else
    #define CONSTRUCT_SQLSTR(s) s
    typedef std::string RDS_STR;
    typedef std::ostringstream RDS_STR_STREAM;
    typedef char RDS_CHAR;
    #define TO_UPPER(c) std::toupper(c)
    typedef std::regex RDS_REGEX;
    typedef std::smatch RDS_MATCH;
#endif

#define AS_RDS_STR(s) RDS_STR((RDS_CHAR *) s)

#endif // RDS_STRINGS_H_
