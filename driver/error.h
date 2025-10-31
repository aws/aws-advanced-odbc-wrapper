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

#ifndef ERROR_H_
#define ERROR_H_

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqltypes.h>

#include <cstring>
#include <string>
#include <map>

typedef enum {
    /* ODBC SQL States*/
    WARN_GENERAL_WARNING,
    WARN_CURSOR_OPERATION_CONFLICT,
    WARN_DISCONNECT_ERROR,
    WARN_NULL_VALUE_ELIMINATED_IN_SET_FUNCTION,
    WARN_STRING_DATA_RIGHT_TRUNCATED,
    WARN_PRIVILEGE_NOT_REVOKED,
    WARN_PRIVILEGE_NOT_GRANTED,
    WARN_INVALID_CONNECTION_STRING_ATTRIBUTE,
    WARN_ERROR_IN_ROW,
    WARN_OPTION_VALUE_CHANGED,
    WARN_ATTEMPT_TO_FETCH_BEFORE_THE_RESULT_SET_RETURNED_THE_FIRST_ROWSET,
    WARN_FRACTIONAL_TRUNCATION,
    WARN_ERROR_SAVING_FILE_DSN,
    WARN_INVALID_KEYWORD,
    ERR_WRONG_NUMBER_OF_PARAMETERS,
    ERR_COUNT_FIELD_INCORRECT,
    ERR_PREPARED_STATEMENT_NOT_A_CURSOR_SPECIFICATION,
    ERR_RESTRICTED_DATA_TYPE_ATTRIBUTE_VIOLATION,
    ERR_INVALID_DESCRIPTOR_INDEX,
    ERR_INVALID_USE_OF_DEFAULT_PARAMETER,
    ERR_CLIENT_UNABLE_TO_ESTABLISH_CONNECTION,
    ERR_CONNECTION_NAME_IN_USE,
    ERR_CONNECTION_NOT_OPEN,
    ERR_SERVER_REJECTED_THE_CONNECTION,
    ERR_CONNECTION_FAILURE_DURING_TRANSACTION,
    ERR_COMMUNICATION_LINK_FAILURE,
    ERR_INSERT_VALUE_LIST_DOES_NOT_MATCH_COLUMN_LIST,
    ERR_DEGREE_OF_DERIVED_TABLE_DOES_NOT_MATCH_COLUMN_LIST,
    ERR_STRING_DATA_RIGHT_TRUNCATED,
    ERR_INDICATOR_VARIABLE_REQUIRED_BUT_NOT_SUPPLIED,
    ERR_NUMERIC_VALUE_OUT_OF_RANGE,
    ERR_INVALID_DATETIME_FORMAT,
    ERR_DATETIME_FIELD_OVERFLOW,
    ERR_DIVISION_BY_ZERO,
    ERR_INTERVAL_FIELD_OVERFLOW,
    ERR_INVALID_CHARACTER_VALUE_FOR_CAST_SPECIFICATION,
    ERR_INVALID_ESCAPE_CHARACTER,
    ERR_INVALID_ESCAPE_SEQUENCE,
    ERR_STRING_DATA_LENGTH_MISMATCH,
    ERR_INTEGRITY_CONSTRAINT_VIOLATION,
    ERR_INVALID_CURSOR_STATE,
    ERR_INVALID_TRANSACTION_STATE,
    ERR_TRANSACTION_STATE,
    ERR_TRANSACTION_IS_STILL_ACTIVE,
    ERR_TRANSACTION_IS_ROLLED_BACK,
    ERR_INVALID_AUTHORIZATION_SPECIFICATION,
    ERR_INVALID_CURSOR_NAME,
    ERR_DUPLICATE_CURSOR_NAME,
    ERR_INVALID_CATALOG_NAME,
    ERR_INVALID_SCHEMA_NAME,
    ERR_SERIALIZATION_FAILURE,
    ERR_TRANSACTION_INTEGRITY_CONSTRAINT_VIOLATION,
    ERR_STATEMENT_COMPLETION_UNKNOWN,
    ERR_SYNTAX_ERROR_OR_ACCESS_VIOLATION,
    ERR_BASE_TABLE_OR_VIEW_ALREADY_EXISTS,
    ERR_BASE_TABLE_OR_VIEW_NOT_FOUND,
    ERR_INDEX_ALREADY_EXISTS,
    ERR_INDEX_NOT_FOUND,
    ERR_COLUMN_ALREADY_EXISTS,
    ERR_COLUMN_NOT_FOUND,
    ERR_WITH_CHECK_OPTION_VIOLATION,
    ERR_GENERAL_ERROR,
    ERR_MEMORY_ALLOCATION_ERROR,
    ERR_INVALID_APPLICATION_BUFFER_TYPE,
    ERR_INVALID_SQL_DATA_TYPE,
    ERR_ASSOCIATED_STATEMENT_IS_NOT_PREPARED,
    ERR_OPERATION_CANCELED,
    ERR_INVALID_USE_OF_NULL_POINTER,
    ERR_FUNCTION_SEQUENCE_ERROR,
    ERR_ATTRIBUTE_CANNOT_BE_SET_NOW,
    ERR_INVALID_TRANSACTION_OPERATION_CODE,
    ERR_MEMORY_MANAGEMENT_ERROR,
    ERR_LIMIT_ON_THE_NUMBER_OF_HANDLES_EXCEEDED,
    ERR_NO_CURSOR_NAME_AVAILABLE,
    ERR_CANNOT_MODIFY_AN_IMPLEMENTATION_ROW_DESCRIPTOR,
    ERR_INVALID_USE_OF_AN_AUTOMATICALLY_ALLOCATED_DESCRIPTOR_HANDLE,
    ERR_SERVER_DECLINED_CANCEL_REQUEST,
    ERR_NON_CHARACTER_AND_NON_BINARY_DATA_SENT_IN_PIECES,
    ERR_ATTEMPT_TO_CONCATENATE_A_NULL_VALUE,
    ERR_INCONSISTENT_DESCRIPTOR_INFORMATION,
    ERR_INVALID_ATTRIBUTE_VALUE,
    ERR_INVALID_STRING_OR_BUFFER_LENGTH,
    ERR_INVALID_DESCRIPTOR_FIELD_IDENTIFIER,
    ERR_INVALID_ATTRIBUTE_OPTION_IDENTIFIER,
    ERR_FUNCTION_TYPE_OUT_OF_RANGE,
    ERR_INVALID_INFORMATION_TYPE,
    ERR_COLUMN_TYPE_OUT_OF_RANGE,
    ERR_SCOPE_TYPE_OUT_OF_RANGE,
    ERR_NULLABLE_TYPE_OUT_OF_RANGE,
    ERR_UNIQUENESS_OPTION_TYPE_OUT_OF_RANGE,
    ERR_ACCURACY_OPTION_TYPE_OUT_OF_RANGE,
    ERR_INVALID_RETRIEVAL_CODE,
    ERR_INVALID_PRECISION_OR_SCALE_VALUE,
    ERR_INVALID_PARAMETER_TYPE,
    ERR_FETCH_TYPE_OUT_OF_RANGE,
    ERR_ROW_VALUE_OUT_OF_RANGE,
    ERR_INVALID_CURSOR_POSITION,
    ERR_INVALID_DRIVER_COMPLETION,
    ERR_INVALID_BOOKMARK_VALUE,
    ERR_OPTIONAL_FEATURE_NOT_IMPLEMENTED,
    ERR_TIMEOUT_EXPIRED,
    ERR_CONNECTION_TIMEOUT_EXPIRED,
    ERR_DRIVER_DOES_NOT_SUPPORT_THIS_FUNCTION,
    ERR_DATA_SOURCE_NAME_NOT_FOUND_AND_NO_DEFAULT_DRIVER_SPECIFIED,
    ERR_SPECIFIED_DRIVER_COULD_NOT_BE_LOADED,
    ERR_SQLALLOCHANDLE_ON_SQL_HANDLE_ENV_FAILED,
    ERR_SQLALLOCHANDLE_ON_SQL_HANDLE_DBC_FAILED,
    ERR_SQLSETCONNECTATTR_FAILED,
    ERR_NO_DATA_SOURCE_OR_DRIVER_SPECIFIED,
    ERR_DIALOG_FAILED,
    ERR_UNABLE_TO_LOAD_TRANSLATION_DLL,
    ERR_DATA_SOURCE_NAME_TOO_LONG,
    ERR_DRIVER_NAME_TOO_LONG,
    ERR_DRIVER_KEYWORD_SYNTAX_ERROR,
    ERR_TRACE_FILE_ERROR,
    ERR_INVALID_NAME_OF_FILE_DSN,
    ERR_CORRUPT_FILE_DATA_SOURCE,
    /* Wrapper Specific */
    /* Dynamic Library Related */
    ERR_NO_UNDER_LYING_DRIVER,
    ERR_NO_UNDER_LYING_FUNCTION,
    ERR_DIFF_ENV_UNDERLYING_DRIVER,
    ERR_UNDERLYING_HANDLE_NULL,
    /* Failover Related */
    ERR_FAILOVER_FAILED,
    ERR_FAILOVER_SUCCESS,
    ERR_FAILOVER_UNKNOWN_TRANSACTION_STATE,

    /* End Error, used for sizing */
    INVALID_ERR
} SQL_STATE_CODE;

const std::string ODBC_STATE_MAP[] = {
    /* ODBC SQL States*/
    "01000", "01001", "01002", "01003", "01004", "01006", "01007", "01S00", "01S01", "01S02", "01S06",
    "01S07", "01S08", "01S09", "07001", "07002", "07005", "07006", "07009", "07S01", "08001", "08002",
    "08003", "08004", "08007", "08S01", "21S01", "21S02", "22001", "22002", "22003", "22007", "22008",
    "22012", "22015", "22018", "22019", "22025", "22026", "23000", "24000", "25000", "25S01", "25S02",
    "25S03", "28000", "34000", "3C000", "3D000", "3F000", "40001", "40002", "40003", "42000", "42S01",
    "42S02", "42S11", "42S12", "42S21", "42S22", "44000", "HY000", "HY001", "HY003", "HY004", "HY007",
    "HY008", "HY009", "HY010", "HY011", "HY012", "HY013", "HY014", "HY015", "HY016", "HY017", "HY018",
    "HY019", "HY020", "HY021", "HY024", "HY090", "HY091", "HY092", "HY095", "HY096", "HY097", "HY098",
    "HY099", "HY100", "HY101", "HY103", "HY104", "HY105", "HY106", "HY107", "HY109", "HY110", "HY111",
    "HYC00", "HYT00", "HYT01", "IM001", "IM002", "IM003", "IM004", "IM005", "IM006", "IM007", "IM008",
    "IM009", "IM010", "IM011", "IM012", "IM013", "IM014", "IM015",
    /* Wrapper Specific */
    /* Dynamic Library Related */
    "LD001", "LD002", "LD003", "LD004",
    /* Failover Related */
    "08S01", "08S02", "08007",
    /* END */
    "ERROR"
};

const std::string ODBC_3_SUBCLASS[] = {
    "01S00", "01S01", "01S02", "01S06", "01S07", "07S01",
    "08S01", "08S02", "08007", "21S01", "21S02", "25S01",
    "25S02", "25S03", "42S01", "42S02", "42S11", "42S12",
    "42S21", "42S22", "HY095", "HY097", "HY098", "HY099",
    "HY100", "HY101", "HY105", "HY107", "HY109", "HY110",
    "HY111", "HYT00", "HYT01", "IM001", "IM002", "IM003",
    "IM004", "IM005", "IM006", "IM007", "IM008", "IM010",
    "IM011", "IM012"
};

struct ERR_INFO {
    SQLRETURN   ret_code            = SQL_SUCCESS;
    char*       error_msg           = nullptr;
    int         native_err          = 0;
    char*       sqlstate            = nullptr;
    bool        is_odbc3_subclass   = false;

    ERR_INFO(const char *msg, SQL_STATE_CODE sql_state) {
        if (msg) {
            error_msg = strdup(msg);
        }
        if (sql_state >= 0 && sql_state <= INVALID_ERR) {
            std::string str_sql_state = ODBC_STATE_MAP[sql_state];
            is_odbc3_subclass = IsOdbc3Subclass(str_sql_state);
            sqlstate = strdup(str_sql_state.c_str());
        }
        native_err = sql_state;
        ret_code = SQL_SUCCESS;
        if (sqlstate) {
            if (strncmp(sqlstate, "01", 2) == 0) {
                ret_code = SQL_SUCCESS_WITH_INFO;
            } else if (strncmp(sqlstate, "00", 2) != 0) {
                ret_code = SQL_ERROR;
            }
        }
    }

    ERR_INFO(const ERR_INFO &source) {
        if (source.error_msg) {
            error_msg = strdup(source.error_msg);
        }
        if (source.sqlstate) {
            sqlstate = strdup(source.sqlstate);
        }
        native_err = source.native_err;
        ret_code = source.ret_code;
    }

    ~ERR_INFO() {
        if (error_msg != nullptr) {
            free(error_msg);
        }
        if (sqlstate != nullptr) {
            free(sqlstate);
        }
    }

    bool IsOdbc3Subclass(std::string str_sql_state) {
        if (str_sql_state.empty()) {
            return false;
        }

        for (size_t i = 0; i < sizeof(ODBC_3_SUBCLASS) / sizeof(ODBC_3_SUBCLASS[0]); i++) {
            if (str_sql_state.compare(ODBC_3_SUBCLASS[i]) == 0) {
                return true;
            }
        }
        return false;
    }

}; // ERR_INFO

#endif // ERROR_H_
