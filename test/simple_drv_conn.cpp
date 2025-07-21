#ifdef _WIN32
#include "windows.h"
#endif

#include <sql.h>
#include <sqlext.h>

#include <iostream>

#define AS_SQLCHAR(str) (const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(str)))
#define MAX_SQL_STATE_LEN 6
#define MAX_MSG_LEN 1024

int main() {
    SQLRETURN ret = 0;
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &henv);
    ret = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    ret = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

    /* Connection */
    // For Windows, use: `DSN=<your-dsn>`
    // For MacOS, can use: `DRIVER=<path-to-shell-driver>`
    // New setting, BASE_DRIVER=<path_to_underlying_driver>;

    // Windows DSN + Base Driver + DB Info
    SQLCHAR* conn_str = AS_SQLCHAR(
        // Regular Connection Info
        "DSN=test-poc;"
        "DATABASE=postgres_limitless;"
        "SERVER=dev-pg-limitless.cluster-cr28trhgdnv7.us-west-2.rds.amazonaws.com;"
        "PORT=5432;"
        "UID=GARBAGE;"
        "PWD=JUNK;"
        // New Stuff
        "BASE_DRIVER=C:\\Program Files\\psqlODBC\\1700\\bin\\podbc30a.dll;" 
        "BASE_DSN=blahblah;" 
        "RDS_AUTH_TYPE=RDS_IAM;"
    );

    // Mac Conn Str + Base Driver
    // SQLCHAR* conn_str = AS_SQLCHAR(
    //     // Regular Connection Info
    //     "DRIVER=/Users/Colin.Yuen/Documents/shell-driver-cpp/build/libpoc-shell-driver.dylib;"
    //     "DATABASE=postgres_limitless;"
    //     "SERVER=dev-pg-limitless.cluster-cr28trhgdnv7.us-west-2.rds.amazonaws.com;"
    //     "PORT=5432;"
    //     "UID=GARBAGE;"
    //     "PWD=JUNK;"
    //     // New Stuff
    //     "BASE_DRIVER=/Users/Colin.Yuen/Documents/Project/aws-pgsql-odbc/.libs/awspsqlodbca.so;
    // );

    ret = SQLDriverConnect(hdbc, nullptr, conn_str, SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
    printf("Drv Conn\n\tRetCode: %d\n", ret);

    /* Get Error, should be empty */
    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;

    SQLTCHAR sqlstate[MAX_SQL_STATE_LEN], message[MAX_MSG_LEN];
    SQLRETURN err_rc =
        SQLError(nullptr, hdbc, nullptr, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    printf("Check Error\n\tState: [%s]\n\tMsg: [%s]\n", sqlstate, message);

    /* Sample Query Execution*/
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    SQLTCHAR test_id[MAX_MSG_LEN] = { 0 };
    SQLLEN test_id_len = 0;
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt);
    SQLCHAR* m_query = AS_SQLCHAR("SELECT aurora_db_instance_identifier()");
    ret = SQLExecDirect(stmt, m_query, SQL_NTS);
    ret = SQLBindCol(stmt, 1, SQL_C_TCHAR, &test_id, sizeof(test_id), &test_id_len);
    ret = SQLFetch(stmt);

    printf("Queried: [%s]\n\tResp: %s\n", (char*) m_query, test_id);

    /* Cleanup 1st Stmt */
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    /* Another Query, but bad */
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt);
    SQLCHAR* b_query = AS_SQLCHAR("GARBAGEGARBAGEGARBAGEGARBAGE");
    ret = SQLExecDirect(stmt, b_query, SQL_NTS);

    /* Get Error */
    err_rc =
        SQLError(nullptr, nullptr, stmt, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
    printf("Queried: [%s]\n\tState: [%s]\n\tMsg: [%s]\n", (char*) b_query, sqlstate, message);

    /* Cleanup Everything */
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, henv);

    /* Finish */
    printf("\nAll Done\n");
    return 0;
}
