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

#ifndef CONNECTION_STRING_BUILDER_H_
#define CONNECTION_STRING_BUILDER_H_

#include <string>

#define AS_SQLTCHAR(str) (const_cast<SQLTCHAR*>(reinterpret_cast<const SQLTCHAR*>(str)))

#ifdef UNICODE
    #define RDS_STR std::wstring
#else
    #define RDS_STR std::string
#endif

class ConnectionStringBuilder {
public:
    ConnectionStringBuilder(const std::string& dsn, const std::string& server, int port) {
        length += sprintf(conn_in, "DSN=%s;SERVER=%s;PORT=%d;", dsn.c_str(), server.c_str(), port);
    }

    ConnectionStringBuilder(const std::string& str) { length += sprintf(conn_in, "%s", str.c_str()); }

    ConnectionStringBuilder& withUID(const std::string& uid) {
        length += sprintf(conn_in + length, "UID=%s;", uid.c_str());
        return *this;
    }

    ConnectionStringBuilder& withPWD(const std::string& pwd) {
        length += sprintf(conn_in + length, "PWD=%s;", pwd.c_str());
        return *this;
    }

    ConnectionStringBuilder& withDatabase(const std::string& db) {
        length += sprintf(conn_in + length, "DATABASE=%s;", db.c_str());
        return *this;
    }

    ConnectionStringBuilder& withBaseDriver(const std::string& driver) {
        length += sprintf(conn_in + length, "BASE_DRIVER=%s;", driver.c_str());
        return *this;
    }

    ConnectionStringBuilder& withBaseDSN(const std::string& dsn) {
        length += sprintf(conn_in + length, "BASE_DSN=%s;", dsn.c_str());
        return *this;
    }

    RDS_STR getRdsString() const {
        RDS_STR converted;
        #ifdef UNICODE
            wchar_t* buf = new wchar_t[strlen(conn_in) * 2 + 2];
            swprintf(buf, L"%S", conn_in);
            converted = buf;
            delete[] buf;
        #else
            converted = conn_in;
        #endif
        return converted;
    }

private:
    char conn_in[4096] = {0};
    int length = 0;
};

#endif  // CONNECTION_STRING_BUILDER_H_
