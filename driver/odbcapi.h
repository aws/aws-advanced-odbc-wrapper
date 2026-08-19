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

#include "util/windows_headers.h"

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#include <map>

#include "util/rds_strings.h"

/* Function Name */
/* Common */
#define RDS_STR_SQLAllocConnect "SQLAllocConnect"
#define RDS_STR_SQLAllocEnv "SQLAllocEnv"
#define RDS_STR_SQLAllocHandle "SQLAllocHandle"
#define RDS_STR_SQLAllocStmt "SQLAllocStmt"
#define RDS_STR_SQLBindCol "SQLBindCol"
#define RDS_STR_SQLBindParameter "SQLBindParameter"
#define RDS_STR_SQLBulkOperations "SQLBulkOperations"
#define RDS_STR_SQLCancel "SQLCancel"
#define RDS_STR_SQLCancelHandle "SQLCancelHandle"
#define RDS_STR_SQLCloseCursor "SQLCloseCursor"
#define RDS_STR_SQLCompleteAsync "SQLCompleteAsync"
#define RDS_STR_SQLCopyDesc "SQLCopyDesc"
#define RDS_STR_SQLDescribeParam "SQLDescribeParam"
#define RDS_STR_SQLDisconnect "SQLDisconnect"
#define RDS_STR_SQLEndTran "SQLEndTran"
#define RDS_STR_SQLExecute "SQLExecute"
#define RDS_STR_SQLExtendedFetch "SQLExtendedFetch"
#define RDS_STR_SQLFetch "SQLFetch"
#define RDS_STR_SQLFetchScroll "SQLFetchScroll"
#define RDS_STR_SQLFreeConnect "SQLFreeConnect"
#define RDS_STR_SQLFreeEnv "SQLFreeEnv"
#define RDS_STR_SQLFreeHandle "SQLFreeHandle"
#define RDS_STR_SQLFreeStmt "SQLFreeStmt"
#define RDS_STR_SQLGetData "SQLGetData"
#define RDS_STR_SQLGetEnvAttr "SQLGetEnvAttr"
#define RDS_STR_SQLGetFunctions "SQLGetFunctions"
#define RDS_STR_SQLGetStmtOption "SQLGetStmtOption"
#define RDS_STR_SQLMoreResults "SQLMoreResults"
#define RDS_STR_SQLNumParams "SQLNumParams"
#define RDS_STR_SQLNumResultCols "SQLNumResultCols"
#define RDS_STR_SQLParamData "SQLParamData"
#define RDS_STR_SQLParamOptions "SQLParamOptions"
#define RDS_STR_SQLPutData "SQLPutData"
#define RDS_STR_SQLRowCount "SQLRowCount"
#define RDS_STR_SQLSetDescRec "SQLSetDescRec"
#define RDS_STR_SQLSetEnvAttr "SQLSetEnvAttr"
#define RDS_STR_SQLSetParam "SQLSetParam"
#define RDS_STR_SQLSetPos "SQLSetPos"
#define RDS_STR_SQLSetScrollOptions "SQLSetScrollOptions"
#define RDS_STR_SQLSetStmtOption "SQLSetStmtOption"
#define RDS_STR_SQLTransact "SQLTransact"

/* Unicode */
#ifdef UNICODE
#define RDS_STR_SQLBrowseConnect "SQLBrowseConnectW"
#define RDS_STR_SQLColAttribute "SQLColAttributeW"
#define RDS_STR_SQLColAttributes "SQLColAttributesW"
#define RDS_STR_SQLColumnPrivileges "SQLColumnPrivilegesW"
#define RDS_STR_SQLColumns "SQLColumnsW"
#define RDS_STR_SQLConnect "SQLConnectW"
#define RDS_STR_SQLDataSources "SQLDataSourcesW"
#define RDS_STR_SQLDescribeCol "SQLDescribeColW"
#define RDS_STR_SQLDriverConnect "SQLDriverConnectW"
#define RDS_STR_SQLDrivers "SQLDriversW"
#define RDS_STR_SQLError "SQLErrorW"
#define RDS_STR_SQLExecDirect "SQLExecDirectW"
#define RDS_STR_SQLForeignKeys "SQLForeignKeysW"
#define RDS_STR_SQLGetConnectAttr "SQLGetConnectAttrW"
#define RDS_STR_SQLGetConnectOption "SQLGetConnectOptionW"
#define RDS_STR_SQLGetCursorName "SQLGetCursorNameW"
#define RDS_STR_SQLGetDescField "SQLGetDescFieldW"
#define RDS_STR_SQLGetDescRec "SQLGetDescRecW"
#define RDS_STR_SQLGetDiagField "SQLGetDiagFieldW"
#define RDS_STR_SQLGetDiagRec "SQLGetDiagRecW"
#define RDS_STR_SQLGetInfo "SQLGetInfoW"
#define RDS_STR_SQLGetStmtAttr "SQLGetStmtAttrW"
#define RDS_STR_SQLGetTypeInfo "SQLGetTypeInfoW"
#define RDS_STR_SQLNativeSql "SQLNativeSqlW"
#define RDS_STR_SQLPrepare "SQLPrepareW"
#define RDS_STR_SQLPrimaryKeys "SQLPrimaryKeysW"
#define RDS_STR_SQLProcedureColumns "SQLProcedureColumnsW"
#define RDS_STR_SQLProcedures "SQLProceduresW"
#define RDS_STR_SQLSetConnectAttr "SQLSetConnectAttrW"
#define RDS_STR_SQLSetConnectOption "SQLSetConnectOptionW"
#define RDS_STR_SQLSetCursorName "SQLSetCursorNameW"
#define RDS_STR_SQLSetDescField "SQLSetDescFieldW"
#define RDS_STR_SQLSetStmtAttr "SQLSetStmtAttrW"
#define RDS_STR_SQLSpecialColumns "SQLSpecialColumnsW"
#define RDS_STR_SQLStatistics "SQLStatisticsW"
#define RDS_STR_SQLTablePrivileges "SQLTablePrivilegesW"
#define RDS_STR_SQLTables "SQLTablesW"
#else /* Ansi */
#define RDS_STR_SQLBrowseConnect "SQLBrowseConnect"
#define RDS_STR_SQLColAttribute "SQLColAttribute"
#define RDS_STR_SQLColAttributes "SQLColAttributes"
#define RDS_STR_SQLColumnPrivileges "SQLColumnPrivileges"
#define RDS_STR_SQLColumns "SQLColumns"
#define RDS_STR_SQLConnect "SQLConnect"
#define RDS_STR_SQLDataSources "SQLDataSources"
#define RDS_STR_SQLDescribeCol "SQLDescribeCol"
#define RDS_STR_SQLDriverConnect "SQLDriverConnect"
#define RDS_STR_SQLDrivers "SQLDrivers"
#define RDS_STR_SQLError "SQLError"
#define RDS_STR_SQLExecDirect "SQLExecDirect"
#define RDS_STR_SQLForeignKeys "SQLForeignKeys"
#define RDS_STR_SQLGetConnectAttr "SQLGetConnectAttr"
#define RDS_STR_SQLGetConnectOption "SQLGetConnectOption"
#define RDS_STR_SQLGetCursorName "SQLGetCursorName"
#define RDS_STR_SQLGetDescField "SQLGetDescField"
#define RDS_STR_SQLGetDescRec "SQLGetDescRec"
#define RDS_STR_SQLGetDiagField "SQLGetDiagField"
#define RDS_STR_SQLGetDiagRec "SQLGetDiagRec"
#define RDS_STR_SQLGetInfo "SQLGetInfo"
#define RDS_STR_SQLGetStmtAttr "SQLGetStmtAttr"
#define RDS_STR_SQLGetTypeInfo "SQLGetTypeInfo"
#define RDS_STR_SQLNativeSql "SQLNativeSql"
#define RDS_STR_SQLPrepare "SQLPrepare"
#define RDS_STR_SQLPrimaryKeys "SQLPrimaryKeys"
#define RDS_STR_SQLProcedureColumns "SQLProcedureColumns"
#define RDS_STR_SQLProcedures "SQLProcedures"
#define RDS_STR_SQLSetConnectAttr "SQLSetConnectAttr"
#define RDS_STR_SQLSetConnectOption "SQLSetConnectOption"
#define RDS_STR_SQLSetCursorName "SQLSetCursorName"
#define RDS_STR_SQLSetDescField "SQLSetDescField"
#define RDS_STR_SQLSetStmtAttr "SQLSetStmtAttr"
#define RDS_STR_SQLSpecialColumns "SQLSpecialColumns"
#define RDS_STR_SQLStatistics "SQLStatistics"
#define RDS_STR_SQLTablePrivileges "SQLTablePrivileges"
#define RDS_STR_SQLTables "SQLTables"
#endif

/* Function Pointer Headers */
using RDS_FP_SQLAllocConnect = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle,
    SQLHDBC *      ConnectionHandle);

using RDS_FP_SQLAllocEnv = SQLRETURN (*)(
    SQLHENV *      EnvironmentHandle);

using RDS_FP_SQLAllocHandle = SQLRETURN (*)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      InputHandle,
    SQLHANDLE *    OutputHandlePtr);

using RDS_FP_SQLAllocStmt = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLHSTMT *     StatementHandle);

using RDS_FP_SQLBindCol = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLSMALLINT    TargetType,
    SQLPOINTER     TargetValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_IndPtr);

using RDS_FP_SQLBindParameter = SQLRETURN (*)(
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

using RDS_FP_SQLBrowseConnect = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr);

using RDS_FP_SQLBulkOperations = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    Operation);

using RDS_FP_SQLCancel = SQLRETURN (*)(
    SQLHSTMT       StatementHandle);

using RDS_FP_SQLCancelHandle = SQLRETURN (*)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle);

using RDS_FP_SQLCloseCursor = SQLRETURN (*)(
    SQLHSTMT       StatementHandle);

using RDS_FP_SQLColAttribute = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr);

using RDS_FP_SQLColAttributes = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr);

using RDS_FP_SQLColumnPrivileges = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4);

using RDS_FP_SQLColumns = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4);

using RDS_FP_SQLCompleteAsync = SQLRETURN (*)(
    SQLSMALLINT   HandleType,
    SQLHANDLE     Handle,
    RETCODE *     AsyncRetCodePtr);

using RDS_FP_SQLConnect = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     ServerName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     UserName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     Authentication,
    SQLSMALLINT    NameLength3);

using RDS_FP_SQLCopyDesc = SQLRETURN (*)(
    SQLHDESC       SourceDescHandle,
    SQLHDESC       TargetDescHandle);

using RDS_FP_SQLDataSources = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLTCHAR *     ServerName,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  NameLength1Ptr,
    SQLTCHAR *     Description,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  NameLength2Ptr);

using RDS_FP_SQLDescribeCol = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr,
    SQLSMALLINT *  DataTypePtr,
    SQLULEN *      ColumnSizePtr,
    SQLSMALLINT *  DecimalDigitsPtr,
    SQLSMALLINT *  NullablePtr);

using RDS_FP_SQLDescribeParam = SQLRETURN (*)(
    SQLHSTMT      StatementHandle,
    SQLUSMALLINT  ParameterNumber,
    SQLSMALLINT * DataTypePtr,
    SQLULEN *     ParameterSizePtr,
    SQLSMALLINT * DecimalDigitsPtr,
    SQLSMALLINT * NullablePtr);

using RDS_FP_SQLDisconnect = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle);

using RDS_FP_SQLDriverConnect = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLTCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLTCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr,
    SQLUSMALLINT   DriverCompletion);

using RDS_FP_SQLDrivers = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLTCHAR *     DriverDescription,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  DescriptionLengthPtr,
    SQLTCHAR *     DriverAttributes,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  AttributesLengthPtr);

using RDS_FP_SQLEndTran = SQLRETURN (*)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    CompletionType);

using RDS_FP_SQLError = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLTCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr);

using RDS_FP_SQLExecDirect = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength);

using RDS_FP_SQLExecute = SQLRETURN (*)(
    SQLHSTMT       StatementHandle);

using RDS_FP_SQLExtendedFetch = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   FetchOrientation,
    SQLLEN         FetchOffset,
    SQLULEN *      RowCountPtr,
    SQLUSMALLINT * RowStatusArray);

using RDS_FP_SQLFetch = SQLRETURN (*)(
    SQLHSTMT        StatementHandle);

using RDS_FP_SQLFetchScroll = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    FetchOrientation,
    SQLLEN         FetchOffset);

using RDS_FP_SQLForeignKeys = SQLRETURN (*)(
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

using RDS_FP_SQLFreeConnect = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle);

using RDS_FP_SQLFreeEnv = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle);

using RDS_FP_SQLFreeHandle = SQLRETURN (*)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle);

using RDS_FP_SQLFreeStmt = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option);

using RDS_FP_SQLGetConnectAttr = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

using RDS_FP_SQLGetConnectOption = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   Attribute,
    SQLPOINTER     ValuePtr);

using RDS_FP_SQLGetCursorName = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr);

using RDS_FP_SQLGetData = SQLRETURN (*)(
    SQLHSTMT      StatementHandle,
    SQLUSMALLINT  Col_or_Param_Num,
    SQLSMALLINT   TargetType,
    SQLPOINTER    TargetValuePtr,
    SQLLEN        BufferLength,
    SQLLEN *      StrLen_or_IndPtr);

using RDS_FP_SQLGetDescField = SQLRETURN (*)(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

using RDS_FP_SQLGetDescRec = SQLRETURN (*)(
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

using RDS_FP_SQLGetDiagField = SQLRETURN (*)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    DiagIdentifier,
    SQLPOINTER     DiagInfoPtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr);

using RDS_FP_SQLGetDiagRec = SQLRETURN (*)(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLTCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLTCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr);

using RDS_FP_SQLGetEnvAttr = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

using RDS_FP_SQLGetFunctions = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   FunctionId,
    SQLUSMALLINT * SupportedPtr);

using RDS_FP_SQLGetInfo = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   InfoType,
    SQLPOINTER     InfoValuePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr);

using RDS_FP_SQLGetStmtAttr = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr);

using RDS_FP_SQLGetStmtOption = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Attribute,
    SQLPOINTER     ValuePtr);

using RDS_FP_SQLGetTypeInfo = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    DataType);

using RDS_FP_SQLMoreResults = SQLRETURN (*)(
    SQLHSTMT       StatementHandle);

using RDS_FP_SQLNativeSql = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLTCHAR *     InStatementText,
    SQLINTEGER     TextLength1,
    SQLTCHAR *     OutStatementText,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   TextLength2Ptr);

using RDS_FP_SQLNumParams = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ParameterCountPtr);

using RDS_FP_SQLNumResultCols = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT *  ColumnCountPtr);

using RDS_FP_SQLParamData = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLPOINTER *   ValuePtrPtr);

using RDS_FP_SQLParamOptions = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLULEN        Crow,
    SQLULEN *      FetchOffsetPtr);

using RDS_FP_SQLPrepare = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     StatementText,
    SQLINTEGER     TextLength);

using RDS_FP_SQLPrimaryKeys = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3);

using RDS_FP_SQLProcedureColumns = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     ProcName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     ColumnName,
    SQLSMALLINT    NameLength4);

using RDS_FP_SQLProcedures = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     ProcName,
    SQLSMALLINT    NameLength3);

using RDS_FP_SQLPutData = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLPOINTER     DataPtr,
    SQLLEN         StrLen_or_Ind);

using RDS_FP_SQLRowCount = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLLEN *       RowCountPtr);

using RDS_FP_SQLSetConnectAttr = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength);

using RDS_FP_SQLSetConnectOption = SQLRETURN (*)(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   Option,
    SQLULEN        Param);

using RDS_FP_SQLSetCursorName = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CursorName,
    SQLSMALLINT    NameLength);

using RDS_FP_SQLSetDescField = SQLRETURN (*)(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength);

using RDS_FP_SQLSetDescRec = SQLRETURN (*)(
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

using RDS_FP_SQLSetEnvAttr = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength);

using RDS_FP_SQLSetParam = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ParameterNumber,
    SQLSMALLINT    ValueType,
    SQLSMALLINT    ParameterType,
    SQLULEN        ColumnSize,
    SQLSMALLINT    DecimalDigits,
    SQLPOINTER     ParameterValuePtr,
    SQLLEN *       StrLen_or_IndPtr);

using RDS_FP_SQLSetPos = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLSETPOSIROW  RowNumber,
    SQLUSMALLINT   Operation,
    SQLUSMALLINT   LockType);

using RDS_FP_SQLSetScrollOptions = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Concurrency,
    SQLLEN         KeysetSize,
    SQLUSMALLINT   RowsetSize);

using RDS_FP_SQLSetStmtAttr = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength);

using RDS_FP_SQLSetStmtOption = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Option,
    SQLULEN        Param);

using RDS_FP_SQLSpecialColumns = SQLRETURN (*)(
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

using RDS_FP_SQLStatistics = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Unique,
    SQLUSMALLINT   Reserved);

using RDS_FP_SQLTablePrivileges = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3);

using RDS_FP_SQLTables = SQLRETURN (*)(
    SQLHSTMT       StatementHandle,
    SQLTCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLTCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLTCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLTCHAR *     TableType,
    SQLSMALLINT    NameLength4);

using RDS_FP_SQLTransact = SQLRETURN (*)(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   CompletionType);

#endif // ODBCAPI_H_
