; Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

LIBRARY
    @COMPILE_FILE_NAME@

EXPORTS
; Setup GUI
    ConfigDSN
    ConfigDriver

; ODBC APIs
; https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/odbc-api-reference
    SQLAllocConnect
    SQLAllocEnv
    SQLAllocHandle
    SQLAllocStmt
    SQLBindCol
    SQLBindParameter
    SQLBrowseConnect@CHAR_ENCODING@
    SQLBulkOperations
    SQLCancel
    SQLCancelHandle
    SQLCloseCursor
    SQLColAttribute@CHAR_ENCODING@
    SQLColAttributes@CHAR_ENCODING@
    SQLColumnPrivileges@CHAR_ENCODING@
    SQLColumns@CHAR_ENCODING@
    SQLCompleteAsync
    SQLConnect@CHAR_ENCODING@
    SQLCopyDesc
    SQLDataSources@CHAR_ENCODING@
    SQLDescribeCol@CHAR_ENCODING@
    SQLDescribeParam
    SQLDisconnect
    SQLDriverConnect@CHAR_ENCODING@
    SQLDrivers@CHAR_ENCODING@
    SQLEndTran
    SQLError@CHAR_ENCODING@
    SQLExecDirect@CHAR_ENCODING@
    SQLExecute
    SQLExtendedFetch
    SQLFetch
    SQLFetchScroll
    SQLForeignKeys@CHAR_ENCODING@
    SQLFreeConnect
    SQLFreeEnv
    SQLFreeHandle
    SQLFreeStmt
    SQLGetConnectAttr@CHAR_ENCODING@
    SQLGetConnectOption@CHAR_ENCODING@
    SQLGetCursorName@CHAR_ENCODING@
    SQLGetData
    SQLGetDescField@CHAR_ENCODING@
    SQLGetDescRec@CHAR_ENCODING@
    SQLGetDiagField@CHAR_ENCODING@
    SQLGetDiagRec@CHAR_ENCODING@
    SQLGetEnvAttr
    SQLGetFunctions
    SQLGetInfo@CHAR_ENCODING@
    SQLGetStmtAttr@CHAR_ENCODING@
    SQLGetStmtOption
    SQLGetTypeInfo@CHAR_ENCODING@
    SQLMoreResults
    SQLNativeSql@CHAR_ENCODING@
    SQLNumParams
    SQLNumResultCols
    SQLParamData
    SQLParamOptions
    SQLPrepare@CHAR_ENCODING@
    SQLPrimaryKeys@CHAR_ENCODING@
    SQLProcedureColumns@CHAR_ENCODING@
    SQLProcedures@CHAR_ENCODING@
    SQLPutData
    SQLRowCount
    SQLSetConnectAttr@CHAR_ENCODING@
    SQLSetConnectOption@CHAR_ENCODING@
    SQLSetCursorName@CHAR_ENCODING@
    SQLSetDescField@CHAR_ENCODING@
    SQLSetDescRec
    SQLSetEnvAttr
    SQLSetParam
    SQLSetPos
    SQLSetScrollOptions
    SQLSetStmtAttr@CHAR_ENCODING@
    SQLSetStmtOption
    SQLSpecialColumns@CHAR_ENCODING@
    SQLStatistics@CHAR_ENCODING@
    SQLTablePrivileges@CHAR_ENCODING@
    SQLTables@CHAR_ENCODING@
    SQLTransact
