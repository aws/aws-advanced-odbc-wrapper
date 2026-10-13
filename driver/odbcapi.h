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
#include <string_view>

#include "util/rds_strings.h"

// NOLINTBEGIN(readability-identifier-naming)
namespace RdsFuncNames {
/* Common */
constexpr std::string_view AllocConnect = "SQLAllocConnect";
constexpr std::string_view AllocEnv = "SQLAllocEnv";
constexpr std::string_view AllocHandle = "SQLAllocHandle";
constexpr std::string_view AllocStmt = "SQLAllocStmt";
constexpr std::string_view BindCol = "SQLBindCol";
constexpr std::string_view BindParameter = "SQLBindParameter";
constexpr std::string_view BulkOperations = "SQLBulkOperations";
constexpr std::string_view Cancel = "SQLCancel";
constexpr std::string_view CancelHandle = "SQLCancelHandle";
constexpr std::string_view CloseCursor = "SQLCloseCursor";
constexpr std::string_view CompleteAsync = "SQLCompleteAsync";
constexpr std::string_view CopyDesc = "SQLCopyDesc";
constexpr std::string_view DescribeParam = "SQLDescribeParam";
constexpr std::string_view Disconnect = "SQLDisconnect";
constexpr std::string_view EndTran = "SQLEndTran";
constexpr std::string_view Execute = "SQLExecute";
constexpr std::string_view ExtendedFetch = "SQLExtendedFetch";
constexpr std::string_view Fetch = "SQLFetch";
constexpr std::string_view FetchScroll = "SQLFetchScroll";
constexpr std::string_view FreeConnect = "SQLFreeConnect";
constexpr std::string_view FreeEnv = "SQLFreeEnv";
constexpr std::string_view FreeHandle = "SQLFreeHandle";
constexpr std::string_view FreeStmt = "SQLFreeStmt";
constexpr std::string_view GetData = "SQLGetData";
constexpr std::string_view GetEnvAttr = "SQLGetEnvAttr";
constexpr std::string_view GetFunctions = "SQLGetFunctions";
constexpr std::string_view GetStmtOption = "SQLGetStmtOption";
constexpr std::string_view MoreResults = "SQLMoreResults";
constexpr std::string_view NumParams = "SQLNumParams";
constexpr std::string_view NumResultCols = "SQLNumResultCols";
constexpr std::string_view ParamData = "SQLParamData";
constexpr std::string_view ParamOptions = "SQLParamOptions";
constexpr std::string_view PutData = "SQLPutData";
constexpr std::string_view RowCount = "SQLRowCount";
constexpr std::string_view SetDescRec = "SQLSetDescRec";
constexpr std::string_view SetEnvAttr = "SQLSetEnvAttr";
constexpr std::string_view SetParam = "SQLSetParam";
constexpr std::string_view SetPos = "SQLSetPos";
constexpr std::string_view SetScrollOptions = "SQLSetScrollOptions";
constexpr std::string_view SetStmtOption = "SQLSetStmtOption";
constexpr std::string_view Transact = "SQLTransact";

/* Unicode */
#ifdef UNICODE
constexpr std::string_view BrowseConnect = "SQLBrowseConnectW";
constexpr std::string_view ColAttribute = "SQLColAttributeW";
constexpr std::string_view ColAttributes = "SQLColAttributesW";
constexpr std::string_view ColumnPrivileges = "SQLColumnPrivilegesW";
constexpr std::string_view Columns = "SQLColumnsW";
constexpr std::string_view Connect = "SQLConnectW";
constexpr std::string_view DataSources = "SQLDataSourcesW";
constexpr std::string_view DescribeCol = "SQLDescribeColW";
constexpr std::string_view DriverConnect = "SQLDriverConnectW";
constexpr std::string_view Drivers = "SQLDriversW";
constexpr std::string_view Error = "SQLErrorW";
constexpr std::string_view ExecDirect = "SQLExecDirectW";
constexpr std::string_view ForeignKeys = "SQLForeignKeysW";
constexpr std::string_view GetConnectAttr = "SQLGetConnectAttrW";
constexpr std::string_view GetConnectOption = "SQLGetConnectOptionW";
constexpr std::string_view GetCursorName = "SQLGetCursorNameW";
constexpr std::string_view GetDescField = "SQLGetDescFieldW";
constexpr std::string_view GetDescRec = "SQLGetDescRecW";
constexpr std::string_view GetDiagField = "SQLGetDiagFieldW";
constexpr std::string_view GetDiagRec = "SQLGetDiagRecW";
constexpr std::string_view GetInfo = "SQLGetInfoW";
constexpr std::string_view GetStmtAttr = "SQLGetStmtAttrW";
constexpr std::string_view GetTypeInfo = "SQLGetTypeInfoW";
constexpr std::string_view NativeSql = "SQLNativeSqlW";
constexpr std::string_view Prepare = "SQLPrepareW";
constexpr std::string_view PrimaryKeys = "SQLPrimaryKeysW";
constexpr std::string_view ProcedureColumns = "SQLProcedureColumnsW";
constexpr std::string_view Procedures = "SQLProceduresW";
constexpr std::string_view SetConnectAttr = "SQLSetConnectAttrW";
constexpr std::string_view SetConnectOption = "SQLSetConnectOptionW";
constexpr std::string_view SetCursorName = "SQLSetCursorNameW";
constexpr std::string_view SetDescField = "SQLSetDescFieldW";
constexpr std::string_view SetStmtAttr = "SQLSetStmtAttrW";
constexpr std::string_view SpecialColumns = "SQLSpecialColumnsW";
constexpr std::string_view Statistics = "SQLStatisticsW";
constexpr std::string_view TablePrivileges = "SQLTablePrivilegesW";
constexpr std::string_view Tables = "SQLTablesW";
#else /* Ansi */
constexpr std::string_view BrowseConnect = "SQLBrowseConnect";
constexpr std::string_view ColAttribute = "SQLColAttribute";
constexpr std::string_view ColAttributes = "SQLColAttributes";
constexpr std::string_view ColumnPrivileges = "SQLColumnPrivileges";
constexpr std::string_view Columns = "SQLColumns";
constexpr std::string_view Connect = "SQLConnect";
constexpr std::string_view DataSources = "SQLDataSources";
constexpr std::string_view DescribeCol = "SQLDescribeCol";
constexpr std::string_view DriverConnect = "SQLDriverConnect";
constexpr std::string_view Drivers = "SQLDrivers";
constexpr std::string_view Error = "SQLError";
constexpr std::string_view ExecDirect = "SQLExecDirect";
constexpr std::string_view ForeignKeys = "SQLForeignKeys";
constexpr std::string_view GetConnectAttr = "SQLGetConnectAttr";
constexpr std::string_view GetConnectOption = "SQLGetConnectOption";
constexpr std::string_view GetCursorName = "SQLGetCursorName";
constexpr std::string_view GetDescField = "SQLGetDescField";
constexpr std::string_view GetDescRec = "SQLGetDescRec";
constexpr std::string_view GetDiagField = "SQLGetDiagField";
constexpr std::string_view GetDiagRec = "SQLGetDiagRec";
constexpr std::string_view GetInfo = "SQLGetInfo";
constexpr std::string_view GetStmtAttr = "SQLGetStmtAttr";
constexpr std::string_view GetTypeInfo = "SQLGetTypeInfo";
constexpr std::string_view NativeSql = "SQLNativeSql";
constexpr std::string_view Prepare = "SQLPrepare";
constexpr std::string_view PrimaryKeys = "SQLPrimaryKeys";
constexpr std::string_view ProcedureColumns = "SQLProcedureColumns";
constexpr std::string_view Procedures = "SQLProcedures";
constexpr std::string_view SetConnectAttr = "SQLSetConnectAttr";
constexpr std::string_view SetConnectOption = "SQLSetConnectOption";
constexpr std::string_view SetCursorName = "SQLSetCursorName";
constexpr std::string_view SetDescField = "SQLSetDescField";
constexpr std::string_view SetStmtAttr = "SQLSetStmtAttr";
constexpr std::string_view SpecialColumns = "SQLSpecialColumns";
constexpr std::string_view Statistics = "SQLStatistics";
constexpr std::string_view TablePrivileges = "SQLTablePrivileges";
constexpr std::string_view Tables = "SQLTables";
#endif
}  // namespace RdsFuncNames
// NOLINTEND(readability-identifier-naming)

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
