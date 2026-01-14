#ifdef _WIN32
    #include "windows.h"
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include <chrono>
#include <thread>
#include <iostream>

#define MAX_SQL_STATE_LEN 6
#define MAX_MSG_LEN 1024

// Helper Functions, see below main() for implementation
void AnsiToUnicode(const char* in, SQLTCHAR* out);
char* SqltcharToAnsi(SQLTCHAR* in);
void PrintError(SQLHANDLE handle, int32_t handle_type);

// Our entry point for the sample application
int main() {
    SQLRETURN ret = 0;

    /*
        ODBC works by using handles to store information about the connection.
        There are 4 different types of handles:
            SQLHENV / Environment to store information like ODBC Version, connection pooling, etc
            SQLHDBC / Connection to store information such as your host, username, connection status / socket
            SQLSTMT / Statement to store things like the query, results, etc
            SQLDESC / Descriptor to store things like metadata on datatypes, lists of table, and table metadata

        Handles are often dependant of one another.
            SQLSTMT and SQLDESC rely on SQLHDBC
            SQLHDBC rely on SQLHENV
            SQLHENV relies on no one. Strong independent handle.

        Allocating handles are just a way for the driver to allocate memory space to store information later.
        The handles by themselves will not do anything without calling other functions using said handles.
        This is the same as a program calling `new`, and as such, we will later need to clean these up with a `SQLFreeHandle`

        Handles are allocated using the ODBC function, SQLAllocHandle(..)
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlallochandle-function?view=sql-server-ver17

        All ODBC functions return a SQLRETURN, which is really just a short. These will typically be
            SQL_SUCCESS                 0
            SQL_SUCCESS_WITH_INFO       1
            SQL_NO_DATA                 100
            SQL_ERROR                   -1
            SQL_INVALID_HANDLE          -2

        Below are examples of allocation for Environments (ENV) & the Connection (DBC)
    */
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv);
    ret = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    ret = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

    /*
        After allocating an Environment and Connection, you can set additional settings
        such as above where we set the ODBC version on the environment handle.

        Now that the driver has allocated space for the handle, we can now use that handle to connect to a database.
        There are several ways to connect,
        either using a DSN (Data Source Name) which already holds information needed to connect to a server
        or a connection string, which a user can specify each key and value, such as the server's host, username, password, etc.

        DSNs and connection strings can be used in conjunction,
        where most drivers should use the connection string key-values as priority to the DSN.

        e.g. Connection strings are usually delimited by semicolons, ';'
        SQLTCHAR* connection_string = "DSN=my-dsn;Server=my-host.com;Port=1234;UID=my-user;PWD;my-password";
    */

    /*
        NOTE: C++ lets you do multi-line strings.
            The below will appear as a single, no line-break string when compiled
            i.e. remember to add the connection string delimiter, ';'
    */
    std::string connection_str = (
        // Some basic / commonly used keys
        "DSN=my-dsn;"
        "SERVER=my-db-host.com;"
        "PORT=1234;"
        "DATABASE=my-database-name;"
        "UID=my-username;"
        "PWD=my-password;"
    );

    /*
        Space required will be the size of the string PLUS the terminating character
        NOTE: Depending on the function used to calculate the length of a string,
            it may or may NOT include terminating character in the returned size.
            e.g. if using strlen(..), this will NOT include the terminating character in its count
    */
    SQLTCHAR* conn_in = new SQLTCHAR[connection_str.length()];
    AnsiToUnicode(connection_str.c_str(), conn_in);

    /*
        You can connect using your allocated handles in a couple of ways.
        SQLBrowseConnect(..) uses a connection string, but iteratively enumerates through the key-values required to connect
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbrowseconnect-function?view=sql-server-ver17
        SQLConnect(..) uses only your DSN, username, and password
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlconnect-function?view=sql-server-ver17
        SQLDriverConnect(..) uses a connection string, where you can specify your options including the DSN
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldriverconnect-function?view=sql-server-ver17

        The below will use Connection Strings / SQLDriverConnect.
        SQLDriverConnect has multiple parameters and options within said parameters.
        The below will be a bare-bones example to get a simplest connection in a C++ application.
    */
    ret = SQLDriverConnect(hdbc, nullptr, conn_in, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    delete conn_in;

    /*
        SQL_SUCCEEDED is a macro from the driver managers that allow you to quickly check
        if an ODBC function succeeded or not.
        Basically a test for SQL_SUCCESS and SQL_SUCCESS_WITH_INFO
    */
    if (!SQL_SUCCEEDED(ret)) {
        PrintError(hdbc, SQL_HANDLE_DBC);
    }

    /*
        Similar to DBCs, we need to allocate a handle for the statement.
        This handle will hold the query and the results
    */
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt);

    std::string query = (
        "SELECT 1;"
    );
    SQLTCHAR* query_in = new SQLTCHAR[connection_str.length()];
    AnsiToUnicode(query.c_str(), query_in);

    /*
        For execution, ODBC provides a couple of functions with the most basic SQLExecDirect(..) being shown below
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlexecdirect-function?view=sql-server-ver17
        This provides a static query, e.g., e.g. INSERT INTO table_name (bogos) VALUES (binted)
        To use parameterized queries, e.g. INSERT INTO table_name (bogos)  VALUES (?)
        SQLPrepare(..) and SQLExecute(..) can be used
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlprepare-function?view=sql-server-ver17
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlexecute-function?view=sql-server-ver17

        When executing, a cursor is opened within the statement handle. Cursors are used to iterate through the result set.
        In general, these are only forward-only cursors, meaning it will only move forward in the result set.
        To review past rows in the result set, the cursor must be closed and reopened.
    */
    ret = SQLExecDirect(stmt, query_in, SQL_NTS);
    delete query_in;

    /*
        Calling an execute sends the query to the database and stores the result in the handle.
        Retrieving data from the handle is a two-step process.
        The first being SQLBindCol(..) to understand what datatypes to convert and retrieve
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindcol-function?view=sql-server-ver17
        Bind has many parameters which work on different datatypes
            https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver17
    */
    SQLINTEGER result = 0;
    SQLLEN returned_data_size = 0; // Used mainly for variable-length datatypes like strings
    ret = SQLBindCol(stmt, 1, SQL_C_SLONG, &result, sizeof(result), &returned_data_size);

    /*
        Once bond, the second step in retrieval will be
        SQLFetch(..) to actually copy the values over to the passed reference in the bind step
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfetch-function?view=sql-server-ver17

        Since there may be multiple rows per query execution, SQLFetch(..) can be called in a loop to retrieve each row
    */
    while (SQL_SUCCEEDED(SQLFetch(stmt))) {
        std::cout << "Query result: " << result;
    }

    /*
        To reuse a handle the cursor must be at the end of the result set.
        If the result set was not fully read, such as if SQLFetch(..) was called once on a result set with multiple rows
        SQLCloseCursor(..) can be used to terminate the cursor
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlclosecursor-function?view=sql-server-ver17
    */
    SQLCloseCursor(stmt);

    /*
        Once everything is done, similar to other C++ programs, memory needs to be cleaned up or it will be lost.
        Since allocating handles are similar to the 'malloc' / 'new' operator, a similar 'free' / 'delete' will be needed
        For each of the handle made above, a SQLFreeHandle(..) should be called to free up the memory
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfreehandle-function?view=sql-server-ver17
        With connections having an extra step to disconnect from the server
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldisconnect-function?view=sql-server-ver17
    */
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, henv);

    /*
        Finish
    */
    return 0;
}

void AnsiToUnicode(const char* in, SQLTCHAR* out) {
    int i;
    for (i = 0; in[i]; i++) {
        out[i] = in[i];
    }
    out[i] = 0;
}

char* SqltcharToAnsi(SQLTCHAR* in) {
    char* ansi = (char*) in;
    int i;
    for (i = 0; in[i]; i ++ ) {
        ansi[i] = in[i] & 0x00ff;
    }
    ansi[i] = 0;
    return ansi;
}

void PrintError(SQLHANDLE handle, int32_t handle_type) {
    /*
        There are a couple of ways to get error information out of a handle,
        the one below shows the more modern, ODBC 3.x way, SQLGetDiagRec(..)
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagrec-function?view=sql-server-ver17

        The other way is a deprecated ODBC 1.x function, SQLError(..), which is not recommended to be used
            https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlerror-function?view=sql-server-ver17
    */

    SQLTCHAR    sqlstate[MAX_SQL_STATE_LEN] = { 0 };
    SQLTCHAR    message[MAX_MSG_LEN] = { 0 };
    SQLINTEGER  native_error = 0;
    SQLSMALLINT textlen = 0;
    SQLRETURN   ret = SQL_ERROR;
    SQLSMALLINT recno = 0;

    do {
        recno++;
        ret = SQLGetDiagRec(handle_type, handle, recno, sqlstate, &native_error, message, sizeof(message), &textlen);
        if (ret == SQL_INVALID_HANDLE) {
            std::cerr << "Invalid handle" << std::endl;
        } else if (SQL_SUCCEEDED(ret)) {
            std::cerr << SqltcharToAnsi(sqlstate) << ": " << SqltcharToAnsi(message) << std::endl;
        }
    } while (ret == SQL_SUCCESS);
}
