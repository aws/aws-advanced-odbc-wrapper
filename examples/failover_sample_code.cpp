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

#ifdef WIN32
    #include <windows.h>
#endif

#include <cstdint>
#include <sql.h>
#include <sqlext.h>

#include <iostream>

#define MAX_MSG_LEN 4096
#define QUERY_BUFFER_SIZE 256
#define SQL_STATE_LENGTH 6

/**
 * Print the error message if the previous ODBC command failed.
 */
void print_error(SQLRETURN rc, SQLHANDLE connection_handle, SQLHANDLE statement_handle) {
    if (SQL_SUCCEEDED(rc)) {
        return;
    }

    // Check what kind of error has occurred
    SQLSMALLINT stmt_length;
    SQLINTEGER native_error;

    SQLTCHAR sqlstate[SQL_STATE_LENGTH], message[QUERY_BUFFER_SIZE];
    SQLRETURN err_rc = SQLError(nullptr,
                                connection_handle,
                                statement_handle,
                                sqlstate,
                                &native_error,
                                message,
                                SQL_MAX_MESSAGE_LENGTH - 1,
                                &stmt_length);

    if (SQL_SUCCEEDED(err_rc)) {
        std::cout << sqlstate << ": " << message << std::endl;
    }
    throw std::runtime_error("An error has occurred while running this sample code.");
}

void setInitialSessionState(SQLHDBC dbc, SQLHSTMT handle) {
    SQLCHAR* set_timezone_query = reinterpret_cast<SQLCHAR*>("SET TIME ZONE 'UTC");
    SQLExecDirect(handle, set_timezone_query, SQL_NTS);
}

void query_current_instance(SQLHDBC dbc) {
    SQLHSTMT stmt = SQL_NULL_HANDLE;
    SQLTCHAR instance_id[QUERY_BUFFER_SIZE];

    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    SQLCHAR* query = reinterpret_cast<SQLCHAR*>("SELECT aurora_db_instance_identifier()");
    // Check the connected instance.
    rc = SQLExecDirect(stmt, query, SQL_NTS);
    print_error(rc, nullptr, stmt);

    rc = SQLBindCol(stmt, 1, SQL_C_CHAR, instance_id, sizeof(instance_id), nullptr);
    rc = SQLFetch(stmt);
    print_error(rc, nullptr, stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    std::cout << "Current instance: " << instance_id << std::endl;
}

bool execute_update_query(SQLHDBC dbc, SQLCHAR* query) {
    constexpr int MAX_RETRIES = 5;
    SQLHSTMT handle;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle);

    int retries = 0;
    bool success = false;
    setInitialSessionState(dbc, handle);

    while (true) {
        if (const SQLRETURN ret = SQLExecDirect(handle, query, SQL_NTS); SQL_SUCCEEDED(ret)) {
            success = true;
            break;
        }

        SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
        SQLCHAR sqlstate[SQL_STATE_LENGTH];
        if (retries > MAX_RETRIES) {
            break;
        }

        // Check what kind of error has occurred
        SQLSMALLINT stmt_length;
        SQLINTEGER native_error;
        SQLError(nullptr, nullptr, handle, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);

        // Failover has occurred and the driver has failed over to another instance successfully
        if (const std::string state = reinterpret_cast<char*>(sqlstate); state == "08S02") {
            // Reconfigure the connection
            setInitialSessionState(dbc, handle);
            // Re-execute that query again
            retries++;
        } else {
            // If other exceptions occur.
            break;
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, handle);
    return success;
}

int main() {
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLSMALLINT len;
    SQLCHAR conn_out[MAX_MSG_LEN];
    SQLTCHAR instance_id[QUERY_BUFFER_SIZE];

    SQLCHAR* conn_str = reinterpret_cast<SQLCHAR*>(
        // Regular Connection Info
        "DSN=my_dsn;"
        "DATABASE=my_database;"
        "SERVER=my-cluster-name.cluster-abc123.us-west-2.rds.amazonaws.com;"
        "PORT=1234;"
        "UID=my_db_user;"
        // Failover Configuration
        "ENABLE_CLUSTER_FAILOVER=1;"            // Failover is disabled by default, and must be enabled via ENABLE_CLUSTER_FAILOVER
        // Optional
        "FAILOVER_MODE=STRICT_READER;"          // Select which nodes the connection type may Failover to
        "HOST_PATTERN=?.cluster-abc123.us-west-2.rds.amazonaws.com;" // If not using a cluster endpoint, Host Pattern is used to build new instance hosts
        "IGNORE_TOPOLOGY_REQUEST_MS=1000;"      // Token cache expiration in milliseconds
        "TOPOLOGY_HIGH_REFRESH_RATE_MS=100;"    // Specify how fast in milliseconds topology should be fetched when Failover occurs
        "TOPOLOGY_REFRESH_RATE_MS=10000;"       // Specify how fast in milliseconds topology should be fetched under normal operations
        "FAILOVER_TIMEOUT_MS=30000;"            // Specify how long in milliseconds Failover can reconnect to new connection
        "CLUSTER_ID=my-cluster-id;"             // Sets a custom identifier to the cluster, only use if using same clusters on different connections
    );

    // Setup
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    // Connect
    SQLRETURN rc = SQLDriverConnect(dbc, nullptr, conn_str, SQL_NTS, conn_out, MAX_MSG_LEN, &len, SQL_DRIVER_NOPROMPT);
    print_error(rc, dbc, nullptr);

    // Check the instance ID to see which instance we are connected to.
    query_current_instance(dbc);

    // In this sample code we will use a sleep query to simulate running a long query or an update query.
    // You can trigger cluster failover using either the AWS Console or the AWS CLI while this query is executing the test the driver failover behaviour.
    // To learn more about manually triggering a cluster failover, see https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/aurora-failover.html
    execute_update_query(dbc, (SQLCHAR*)"SELECT PG_SLEEP(60)");

    // Check the instance ID again to ensure connection has swapped to a new instance after failover.
    query_current_instance(dbc);

    // Cleanup
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}
