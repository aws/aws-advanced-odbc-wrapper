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

#ifndef ODBCAPI_H_
#define ODBCAPI_H_

#ifndef ODBCVER
    #define ODBCVER 0x0380
#endif

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include <map>

#include "util/rds_strings.h"

/* Function Name */
/* Common */
#define RDS_STR_SQLAllocConnect AS_RDS_STR(TEXT("SQLAllocConnect"))
#define RDS_STR_SQLAllocEnv AS_RDS_STR(TEXT("SQLAllocEnv"))
#define RDS_STR_SQLAllocHandle AS_RDS_STR(TEXT("SQLAllocHandle"))
#define RDS_STR_SQLAllocStmt AS_RDS_STR(TEXT("SQLAllocStmt"))
#define RDS_STR_SQLBindCol AS_RDS_STR(TEXT("SQLBindCol"))
#define RDS_STR_SQLBindParameter AS_RDS_STR(TEXT("SQLBindParameter"))
#define RDS_STR_SQLBulkOperations AS_RDS_STR(TEXT("SQLBulkOperations"))
#define RDS_STR_SQLCancel AS_RDS_STR(TEXT("SQLCancel"))
#define RDS_STR_SQLCancelHandle AS_RDS_STR(TEXT("SQLCancelHandle"))
#define RDS_STR_SQLCloseCursor AS_RDS_STR(TEXT("SQLCloseCursor"))
#define RDS_STR_SQLCompleteAsync AS_RDS_STR(TEXT("SQLCompleteAsync"))
#define RDS_STR_SQLCopyDesc AS_RDS_STR(TEXT("SQLCopyDesc"))
#define RDS_STR_SQLDescribeParam AS_RDS_STR(TEXT("SQLDescribeParam"))
#define RDS_STR_SQLDisconnect AS_RDS_STR(TEXT("SQLDisconnect"))
#define RDS_STR_SQLEndTran AS_RDS_STR(TEXT("SQLEndTran"))
#define RDS_STR_SQLExecute AS_RDS_STR(TEXT("SQLExecute"))
#define RDS_STR_SQLExtendedFetch AS_RDS_STR(TEXT("SQLExtendedFetch"))
#define RDS_STR_SQLFetch AS_RDS_STR(TEXT("SQLFetch"))
#define RDS_STR_SQLFetchScroll AS_RDS_STR(TEXT("SQLFetchScroll"))
#define RDS_STR_SQLFreeConnect AS_RDS_STR(TEXT("SQLFreeConnect"))
#define RDS_STR_SQLFreeEnv AS_RDS_STR(TEXT("SQLFreeEnv"))
#define RDS_STR_SQLFreeHandle AS_RDS_STR(TEXT("SQLFreeHandle"))
#define RDS_STR_SQLFreeStmt AS_RDS_STR(TEXT("SQLFreeStmt"))
#define RDS_STR_SQLGetData AS_RDS_STR(TEXT("SQLGetData"))
#define RDS_STR_SQLGetEnvAttr AS_RDS_STR(TEXT("SQLGetEnvAttr"))
#define RDS_STR_SQLGetFunctions AS_RDS_STR(TEXT("SQLGetFunctions"))
#define RDS_STR_SQLGetStmtOption AS_RDS_STR(TEXT("SQLGetStmtOption"))
#define RDS_STR_SQLMoreResults AS_RDS_STR(TEXT("SQLMoreResults"))
#define RDS_STR_SQLNumParams AS_RDS_STR(TEXT("SQLNumParams"))
#define RDS_STR_SQLNumResultCols AS_RDS_STR(TEXT("SQLNumResultCols"))
#define RDS_STR_SQLParamData AS_RDS_STR(TEXT("SQLParamData"))
#define RDS_STR_SQLParamOptions AS_RDS_STR(TEXT("SQLParamOptions"))
#define RDS_STR_SQLPutData AS_RDS_STR(TEXT("SQLPutData"))
#define RDS_STR_SQLRowCount AS_RDS_STR(TEXT("SQLRowCount"))
#define RDS_STR_SQLSetDescRec AS_RDS_STR(TEXT("SQLSetDescRec"))
#define RDS_STR_SQLSetEnvAttr AS_RDS_STR(TEXT("SQLSetEnvAttr"))
#define RDS_STR_SQLSetParam AS_RDS_STR(TEXT("SQLSetParam"))
#define RDS_STR_SQLSetPos AS_RDS_STR(TEXT("SQLSetPos"))
#define RDS_STR_SQLSetScrollOptions AS_RDS_STR(TEXT("SQLSetScrollOptions"))
#define RDS_STR_SQLSetStmtOption AS_RDS_STR(TEXT("SQLSetStmtOption"))
#define RDS_STR_SQLTransact AS_RDS_STR(TEXT("SQLTransact"))

/* Unicode */
#ifdef UNICODE
#define RDS_STR_SQLBrowseConnect AS_RDS_STR(TEXT("SQLBrowseConnectW"))
#define RDS_STR_SQLColAttribute AS_RDS_STR(TEXT("SQLColAttributeW"))
#define RDS_STR_SQLColAttributes AS_RDS_STR(TEXT("SQLColAttributesW"))
#define RDS_STR_SQLColumnPrivileges AS_RDS_STR(TEXT("SQLColumnPrivilegesW"))
#define RDS_STR_SQLColumns AS_RDS_STR(TEXT("SQLColumnsW"))
#define RDS_STR_SQLConnect AS_RDS_STR(TEXT("SQLConnectW"))
#define RDS_STR_SQLDataSources AS_RDS_STR(TEXT("SQLDataSourcesW"))
#define RDS_STR_SQLDescribeCol AS_RDS_STR(TEXT("SQLDescribeColW"))
#define RDS_STR_SQLDriverConnect AS_RDS_STR(TEXT("SQLDriverConnectW"))
#define RDS_STR_SQLDrivers AS_RDS_STR(TEXT("SQLDriversW"))
#define RDS_STR_SQLError AS_RDS_STR(TEXT("SQLErrorW"))
#define RDS_STR_SQLExecDirect AS_RDS_STR(TEXT("SQLExecDirectW"))
#define RDS_STR_SQLForeignKeys AS_RDS_STR(TEXT("SQLForeignKeysW"))
#define RDS_STR_SQLGetConnectAttr AS_RDS_STR(TEXT("SQLGetConnectAttrW"))
#define RDS_STR_SQLGetConnectOption AS_RDS_STR(TEXT("SQLGetConnectOptionW"))
#define RDS_STR_SQLGetCursorName AS_RDS_STR(TEXT("SQLGetCursorNameW"))
#define RDS_STR_SQLGetDescField AS_RDS_STR(TEXT("SQLGetDescFieldW"))
#define RDS_STR_SQLGetDescRec AS_RDS_STR(TEXT("SQLGetDescRecW"))
#define RDS_STR_SQLGetDiagField AS_RDS_STR(TEXT("SQLGetDiagFieldW"))
#define RDS_STR_SQLGetDiagRec AS_RDS_STR(TEXT("SQLGetDiagRecW"))
#define RDS_STR_SQLGetInfo AS_RDS_STR(TEXT("SQLGetInfoW"))
#define RDS_STR_SQLGetStmtAttr AS_RDS_STR(TEXT("SQLGetStmtAttrW"))
#define RDS_STR_SQLGetTypeInfo AS_RDS_STR(TEXT("SQLGetTypeInfoW"))
#define RDS_STR_SQLNativeSql AS_RDS_STR(TEXT("SQLNativeSqlW"))
#define RDS_STR_SQLPrepare AS_RDS_STR(TEXT("SQLPrepareW"))
#define RDS_STR_SQLPrimaryKeys AS_RDS_STR(TEXT("SQLPrimaryKeysW"))
#define RDS_STR_SQLProcedureColumns AS_RDS_STR(TEXT("SQLProcedureColumnsW"))
#define RDS_STR_SQLProcedures AS_RDS_STR(TEXT("SQLProceduresW"))
#define RDS_STR_SQLSetConnectAttr AS_RDS_STR(TEXT("SQLSetConnectAttrW"))
#define RDS_STR_SQLSetConnectOption AS_RDS_STR(TEXT("SQLSetConnectOptionW"))
#define RDS_STR_SQLSetCursorName AS_RDS_STR(TEXT("SQLSetCursorNameW"))
#define RDS_STR_SQLSetDescField AS_RDS_STR(TEXT("SQLSetDescFieldW"))
#define RDS_STR_SQLSetStmtAttr AS_RDS_STR(TEXT("SQLSetStmtAttrW"))
#define RDS_STR_SQLSpecialColumns AS_RDS_STR(TEXT("SQLSpecialColumnsW"))
#define RDS_STR_SQLStatistics AS_RDS_STR(TEXT("SQLStatisticsW"))
#define RDS_STR_SQLTablePrivileges AS_RDS_STR(TEXT("SQLTablePrivilegesW"))
#define RDS_STR_SQLTables AS_RDS_STR(TEXT("SQLTablesW"))
#else /* Ansi */
#define RDS_STR_SQLBrowseConnect AS_RDS_STR(TEXT("SQLBrowseConnect"))
#define RDS_STR_SQLColAttribute AS_RDS_STR(TEXT("SQLColAttribute"))
#define RDS_STR_SQLColAttributes AS_RDS_STR(TEXT("SQLColAttributes"))
#define RDS_STR_SQLColumnPrivileges AS_RDS_STR(TEXT("SQLColumnPrivileges"))
#define RDS_STR_SQLColumns AS_RDS_STR(TEXT("SQLColumns"))
#define RDS_STR_SQLConnect AS_RDS_STR(TEXT("SQLConnect"))
#define RDS_STR_SQLDataSources AS_RDS_STR(TEXT("SQLDataSources"))
#define RDS_STR_SQLDescribeCol AS_RDS_STR(TEXT("SQLDescribeCol"))
#define RDS_STR_SQLDriverConnect AS_RDS_STR(TEXT("SQLDriverConnect"))
#define RDS_STR_SQLDrivers AS_RDS_STR(TEXT("SQLDrivers"))
#define RDS_STR_SQLError AS_RDS_STR(TEXT("SQLError"))
#define RDS_STR_SQLExecDirect AS_RDS_STR(TEXT("SQLExecDirect"))
#define RDS_STR_SQLForeignKeys AS_RDS_STR(TEXT("SQLForeignKeys"))
#define RDS_STR_SQLGetConnectAttr AS_RDS_STR(TEXT("SQLGetConnectAttr"))
#define RDS_STR_SQLGetConnectOption AS_RDS_STR(TEXT("SQLGetConnectOption"))
#define RDS_STR_SQLGetCursorName AS_RDS_STR(TEXT("SQLGetCursorName"))
#define RDS_STR_SQLGetDescField AS_RDS_STR(TEXT("SQLGetDescField"))
#define RDS_STR_SQLGetDescRec AS_RDS_STR(TEXT("SQLGetDescRec"))
#define RDS_STR_SQLGetDiagField AS_RDS_STR(TEXT("SQLGetDiagField"))
#define RDS_STR_SQLGetDiagRec AS_RDS_STR(TEXT("SQLGetDiagRec"))
#define RDS_STR_SQLGetInfo AS_RDS_STR(TEXT("SQLGetInfo"))
#define RDS_STR_SQLGetStmtAttr AS_RDS_STR(TEXT("SQLGetStmtAttr"))
#define RDS_STR_SQLGetTypeInfo AS_RDS_STR(TEXT("SQLGetTypeInfo"))
#define RDS_STR_SQLNativeSql AS_RDS_STR(TEXT("SQLNativeSql"))
#define RDS_STR_SQLPrepare AS_RDS_STR(TEXT("SQLPrepare"))
#define RDS_STR_SQLPrimaryKeys AS_RDS_STR(TEXT("SQLPrimaryKeys"))
#define RDS_STR_SQLProcedureColumns AS_RDS_STR(TEXT("SQLProcedureColumns"))
#define RDS_STR_SQLProcedures AS_RDS_STR(TEXT("SQLProcedures"))
#define RDS_STR_SQLSetConnectAttr AS_RDS_STR(TEXT("SQLSetConnectAttr"))
#define RDS_STR_SQLSetConnectOption AS_RDS_STR(TEXT("SQLSetConnectOption"))
#define RDS_STR_SQLSetCursorName AS_RDS_STR(TEXT("SQLSetCursorName"))
#define RDS_STR_SQLSetDescField AS_RDS_STR(TEXT("SQLSetDescField"))
#define RDS_STR_SQLSetStmtAttr AS_RDS_STR(TEXT("SQLSetStmtAttr"))
#define RDS_STR_SQLSpecialColumns AS_RDS_STR(TEXT("SQLSpecialColumns"))
#define RDS_STR_SQLStatistics AS_RDS_STR(TEXT("SQLStatistics"))
#define RDS_STR_SQLTablePrivileges AS_RDS_STR(TEXT("SQLTablePrivileges"))
#define RDS_STR_SQLTables AS_RDS_STR(TEXT("SQLTables"))
#endif

/* Function Pointer Headers */
typedef SQLRETURN (*RDS_FP_SQLAllocConnect)(
    SQLHENV        EnvironmentHandle,
    SQLHDBC *      ConnectionHandle);

typedef SQLRETURN (*RDS_FP_SQLAllocEnv)(
    SQLHENV *      EnvironmentHandle);

typedef SQLRETURN (*RDS_FP_SQLAllocHandle)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      InputHandle,
    SQLHANDLE *    OutputHandlePtr);

typedef SQLRETURN (*RDS_FP_SQLAllocStmt)(
    SQLHDBC        ConnectionHandle,
    SQLHSTMT *     StatementHandle);

typedef SQLRETURN (*RDS_FP_SQLBindCol)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLSMALLINT    TargetType,
    SQLPOINTER     TargetValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_IndPtr);

typedef SQLRETURN (*RDS_FP_SQLBindParameter)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ParameterNumber,
    SQLSMALLINT    InputOutputType,
    SQLSMALLINT    ValueType,
    SQLSMALLINT    ParameterType,
    SQLULEN        ColumnSize,
    SQLSMALLINT    DecimalDigits,
    SQLPOINTER     ParameterValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_IndPtr);

typedef SQLRETURN (*RDS_FP_SQLBrowseConnect)(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr);

typedef SQLRETURN (*RDS_FP_SQLBulkOperations)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Operation);

typedef SQLRETURN (*RDS_FP_SQLCancel)(
    SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_FP_SQLCancelHandle)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle);

typedef SQLRETURN (*RDS_FP_SQLCloseCursor)(
    SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_FP_SQLColAttribute)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr);

typedef SQLRETURN (*RDS_FP_SQLColAttributes)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr);

typedef SQLRETURN (*RDS_FP_SQLColumnPrivileges)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_FP_SQLColumns)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_FP_SQLCompleteAsync)(
    SQLSMALLINT   HandleType,
    SQLHANDLE     Handle,
    RETCODE *     AsyncRetCodePtr);

typedef SQLRETURN (*RDS_FP_SQLConnect)(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     ServerName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     UserName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     Authentication,
    SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_FP_SQLCopyDesc)(
    SQLHDESC       SourceDescHandle,
    SQLHDESC       TargetDescHandle);

typedef SQLRETURN (*RDS_FP_SQLDataSources)(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLTCHAR *     ServerName,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  NameLength1Ptr,
    SQLTCHAR *     Description,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  NameLength2Ptr);

typedef SQLRETURN (*RDS_FP_SQLDescribeCol)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr,
    SQLSMALLINT *  DataTypePtr,
    SQLULEN *      ColumnSizePtr,
    SQLSMALLINT *  DecimalDigitsPtr,
    SQLSMALLINT *  NullablePtr);

typedef SQLRETURN (*RDS_FP_SQLDescribeParam)(
    SQLHSTMT      StatementHandle,
    SQLUSMALLINT  ParameterNumber,
    SQLSMALLINT * DataTypePtr,
    SQLULEN *     ParameterSizePtr,
    SQLSMALLINT * DecimalDigitsPtr,
    SQLSMALLINT * NullablePtr);

typedef SQLRETURN (*RDS_FP_SQLDisconnect)(
    SQLHDBC        ConnectionHandle);

typedef SQLRETURN (*RDS_FP_SQLDriverConnect)(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr,
    SQLUSMALLINT   DriverCompletion);

typedef SQLRETURN (*RDS_FP_SQLDrivers)(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLTCHAR *     DriverDescription,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  DescriptionLengthPtr,
    SQLTCHAR *     DriverAttributes,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  AttributesLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLEndTran)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    CompletionType);

typedef SQLRETURN (*RDS_FP_SQLError)(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLTCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLExecDirect)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength);

typedef SQLRETURN (*RDS_FP_SQLExecute)(
    SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_FP_SQLExtendedFetch)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   FetchOrientation,
    SQLLEN         FetchOffset,
    SQLULEN *      RowCountPtr,
    SQLUSMALLINT * RowStatusArray);

typedef SQLRETURN (*RDS_FP_SQLFetch)(
    SQLHSTMT        StatementHandle);

typedef SQLRETURN (*RDS_FP_SQLFetchScroll)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    FetchOrientation,
    SQLLEN         FetchOffset);

typedef SQLRETURN (*RDS_FP_SQLForeignKeys)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     PKCatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     PKSchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     PKTableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     FKCatalogName,
    SQLSMALLINT    NameLength4,
    SQLTCHAR *     FKSchemaName,
    SQLSMALLINT    NameLength5,
    SQLTCHAR *     FKTableName,
    SQLSMALLINT    NameLength6);

typedef SQLRETURN (*RDS_FP_SQLFreeConnect)(
    SQLHDBC        ConnectionHandle);

typedef SQLRETURN (*RDS_FP_SQLFreeEnv)(
    SQLHENV        EnvironmentHandle);

typedef SQLRETURN (*RDS_FP_SQLFreeHandle)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle);

typedef SQLRETURN (*RDS_FP_SQLFreeStmt)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option);

typedef SQLRETURN (*RDS_FP_SQLGetConnectAttr)(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetConnectOption)(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr);

typedef SQLRETURN (*RDS_FP_SQLGetCursorName)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetData)(
    SQLHSTMT      StatementHandle,
    SQLUSMALLINT  Col_or_Param_Num,
    SQLSMALLINT   TargetType,
    SQLPOINTER    TargetValuePtr,
    SQLLEN        BufferLength,
    SQLLEN *      StrLen_or_IndPtr);

typedef SQLRETURN (*RDS_FP_SQLGetDescField)(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetDescRec)(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLTCHAR *     Name,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLSMALLINT *  TypePtr,
    SQLSMALLINT *  SubTypePtr,
    SQLLEN *       LengthPtr,
    SQLSMALLINT *  PrecisionPtr,
    SQLSMALLINT *  ScalePtr,
    SQLSMALLINT *  NullablePtr);

typedef SQLRETURN (*RDS_FP_SQLGetDiagField)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    DiagIdentifier,
    SQLPOINTER     DiagInfoPtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetDiagRec)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLTCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLTCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetEnvAttr)(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetFunctions)(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   FunctionId,
    SQLUSMALLINT * SupportedPtr);

typedef SQLRETURN (*RDS_FP_SQLGetInfo)(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   InfoType,
    SQLPOINTER     InfoValuePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetStmtAttr)(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_FP_SQLGetStmtOption)(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr);

typedef SQLRETURN (*RDS_FP_SQLGetTypeInfo)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    DataType);

typedef SQLRETURN (*RDS_FP_SQLMoreResults)(
    SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_FP_SQLNativeSql)(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     InStatementText,
    SQLINTEGER     TextLength1,
    SQLTCHAR *     OutStatementText,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   TextLength2Ptr);

typedef SQLRETURN (*RDS_FP_SQLNumParams)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ParameterCountPtr);

typedef SQLRETURN (*RDS_FP_SQLNumResultCols)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ColumnCountPtr);

typedef SQLRETURN (*RDS_FP_SQLParamData)(
    SQLHSTMT       StatementHandle,
    SQLPOINTER *   ValuePtrPtr);

typedef SQLRETURN (*RDS_FP_SQLParamOptions)(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Crow,
    SQLINTEGER *   FetchOffsetPtr);

typedef SQLRETURN (*RDS_FP_SQLPrepare)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength);

typedef SQLRETURN (*RDS_FP_SQLPrimaryKeys)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_FP_SQLProcedureColumns)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     ProcName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_FP_SQLProcedures)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     ProcName,
    SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_FP_SQLPutData)(
    SQLHSTMT       StatementHandle,
    SQLPOINTER     DataPtr,
    SQLLEN         StrLen_or_Ind);

typedef SQLRETURN (*RDS_FP_SQLRowCount)(
    SQLHSTMT       StatementHandle,
    SQLLEN *       RowCountPtr);

typedef SQLRETURN (*RDS_FP_SQLSetConnectAttr)(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength);

typedef SQLRETURN (*RDS_FP_SQLSetConnectOption)(
    SQLHDBC        ConnectionHandle,
    SQLSMALLINT    Option,
    SQLPOINTER     Param);

typedef SQLRETURN (*RDS_FP_SQLSetCursorName)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    NameLength);

typedef SQLRETURN (*RDS_FP_SQLSetDescField)(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength);

typedef SQLRETURN (*RDS_FP_SQLSetDescRec)(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   Type,
    SQLSMALLINT   SubType,
    SQLLEN        Length,
    SQLSMALLINT   Precision,
    SQLSMALLINT   Scale,
    SQLPOINTER    DataPtr,
    SQLLEN *      StringLengthPtr,
    SQLLEN *      IndicatorPtr);

typedef SQLRETURN (*RDS_FP_SQLSetEnvAttr)(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength);

typedef SQLRETURN (*RDS_FP_SQLSetParam)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ParameterNumber,
    SQLSMALLINT    ValueType,
    SQLSMALLINT    ParameterType,
    SQLULEN        ColumnSize,
    SQLSMALLINT    DecimalDigits,
    SQLPOINTER     ParameterValuePtr,
    SQLLEN *       StrLen_or_IndPtr);

typedef SQLRETURN (*RDS_FP_SQLSetPos)(
    SQLHSTMT       StatementHandle,
    SQLSETPOSIROW  RowNumber,
    SQLUSMALLINT   Operation,
    SQLUSMALLINT   LockType);

typedef SQLRETURN (*RDS_FP_SQLSetScrollOptions)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Concurrency,
    SQLLEN         KeysetSize,
    SQLUSMALLINT   RowsetSize);

typedef SQLRETURN (*RDS_FP_SQLSetStmtAttr)(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength);

typedef SQLRETURN (*RDS_FP_SQLSetStmtOption)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option,
    SQLULEN        Param);

typedef SQLRETURN (*RDS_FP_SQLSpecialColumns)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    IdentifierType,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLSMALLINT    Scope,
    SQLSMALLINT    Nullable);

typedef SQLRETURN (*RDS_FP_SQLStatistics)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Unique,
    SQLUSMALLINT   Reserved);

typedef SQLRETURN (*RDS_FP_SQLTablePrivileges)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_FP_SQLTables)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     TableType,
    SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_FP_SQLTransact)(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   CompletionType);

#endif // ODBCAPI_H_
