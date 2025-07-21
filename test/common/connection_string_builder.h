#ifndef CONNECTION_STRING_BUILDER_H_
#define CONNECTION_STRING_BUILDER_H_

#include <string>

#define AS_SQLTCHAR(str) (const_cast<SQLTCHAR*>(reinterpret_cast<const SQLTCHAR*>(str)))

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

    std::string getString() const { return conn_in; }

private:
    char conn_in[4096] = "\0";
    int length = 0;
};

#endif  // CONNECTION_STRING_BUILDER_H_
