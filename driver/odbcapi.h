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

#include "util/rds_lib_loader.h"

/* Function Pointer Headers */
typedef SQLRETURN (*RDS_SQLAllocConnect)(
     SQLHENV        EnvironmentHandle,
     SQLHDBC *      ConnectionHandle);

typedef SQLRETURN (*RDS_SQLAllocEnv)(
     SQLHENV *      EnvironmentHandle);

typedef SQLRETURN (*RDS_SQLAllocHandle)(
     SQLSMALLINT    HandleType,
     SQLHANDLE      InputHandle,
     SQLHANDLE *    OutputHandlePtr);

typedef SQLRETURN (*RDS_SQLAllocStmt)(
     SQLHDBC        ConnectionHandle,
     SQLHSTMT *     StatementHandle);

typedef SQLRETURN (*RDS_SQLBindCol)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   ColumnNumber,
     SQLSMALLINT    TargetType,
     SQLPOINTER     TargetValuePtr,
     SQLLEN         BufferLength,
     SQLLEN *       StrLen_or_IndPtr);

typedef SQLRETURN (*RDS_SQLBindParameter)(
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

typedef SQLRETURN (*RDS_SQLBrowseConnect)(
     SQLHDBC        ConnectionHandle,
     SQLCHAR *      InConnectionString,
     SQLSMALLINT    StringLength1,
     SQLCHAR *      OutConnectionString,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  StringLength2Ptr);

typedef SQLRETURN (*RDS_SQLBulkOperations)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   Operation);

typedef SQLRETURN (*RDS_SQLCancel)(
     SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_SQLCancelHandle)(
     SQLSMALLINT    HandleType,
     SQLHANDLE      Handle);

typedef SQLRETURN (*RDS_SQLCloseCursor)(
     SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_SQLColAttribute)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   ColumnNumber,
     SQLUSMALLINT   FieldIdentifier,
     SQLPOINTER     CharacterAttributePtr,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  StringLengthPtr,
     SQLLEN *       NumericAttributePtr);

typedef SQLRETURN (*RDS_SQLColAttributes)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   ColumnNumber,
     SQLUSMALLINT   FieldIdentifier,
     SQLPOINTER     CharacterAttributePtr,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  StringLengthPtr,
     SQLLEN *       NumericAttributePtr);

typedef SQLRETURN (*RDS_SQLColumnPrivileges)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      TableName,
     SQLSMALLINT    NameLength3,
     SQLCHAR *      ColumnName,
     SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_SQLColumns)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      TableName,
     SQLSMALLINT    NameLength3,
     SQLCHAR *      ColumnName,
     SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_SQLCompleteAsync)(
      SQLSMALLINT   HandleType,
      SQLHANDLE     Handle,
      RETCODE *     AsyncRetCodePtr);

typedef SQLRETURN (*RDS_SQLConnect)(
     SQLHDBC        ConnectionHandle,
     SQLCHAR *      ServerName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      UserName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      Authentication,
     SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_SQLCopyDesc)(
     SQLHDESC       SourceDescHandle,
     SQLHDESC       TargetDescHandle);

typedef SQLRETURN (*RDS_SQLDataSources)(
     SQLHENV        EnvironmentHandle,
     SQLUSMALLINT   Direction,
     SQLCHAR *      ServerName,
     SQLSMALLINT    BufferLength1,
     SQLSMALLINT *  NameLength1Ptr,
     SQLCHAR *      Description,
     SQLSMALLINT    BufferLength2,
     SQLSMALLINT *  NameLength2Ptr);

typedef SQLRETURN (*RDS_SQLDescribeCol)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   ColumnNumber,
     SQLCHAR *      ColumnName,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  NameLengthPtr,
     SQLSMALLINT *  DataTypePtr,
     SQLULEN *      ColumnSizePtr,
     SQLSMALLINT *  DecimalDigitsPtr,
     SQLSMALLINT *  NullablePtr);

typedef SQLRETURN (*RDS_SQLDescribeParam)(
      SQLHSTMT      StatementHandle,
      SQLUSMALLINT  ParameterNumber,
      SQLSMALLINT * DataTypePtr,
      SQLULEN *     ParameterSizePtr,
      SQLSMALLINT * DecimalDigitsPtr,
      SQLSMALLINT * NullablePtr);

typedef SQLRETURN (*RDS_SQLDisconnect)(
     SQLHDBC        ConnectionHandle);

typedef SQLRETURN (*RDS_SQLDriverConnect)(
     SQLHDBC        ConnectionHandle,
     SQLHWND        WindowHandle,
     SQLCHAR *      InConnectionString,
     SQLSMALLINT    StringLength1,
     SQLCHAR *      OutConnectionString,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  StringLength2Ptr,
     SQLUSMALLINT   DriverCompletion);

typedef SQLRETURN (*RDS_SQLDrivers)(
     SQLHENV        EnvironmentHandle,
     SQLUSMALLINT   Direction,
     SQLCHAR *      DriverDescription,
     SQLSMALLINT    BufferLength1,
     SQLSMALLINT *  DescriptionLengthPtr,
     SQLCHAR *      DriverAttributes,
     SQLSMALLINT    BufferLength2,
     SQLSMALLINT *  AttributesLengthPtr);

typedef SQLRETURN (*RDS_SQLEndTran)(
     SQLSMALLINT    HandleType,
     SQLHANDLE      Handle,
     SQLSMALLINT    CompletionType);

typedef SQLRETURN (*RDS_SQLError)(
     SQLHENV        EnvironmentHandle,
     SQLHDBC        ConnectionHandle,
     SQLHSTMT       StatementHandle,
     SQLCHAR *      SQLState,
     SQLINTEGER *   NativeErrorPtr,
     SQLCHAR *      MessageText,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  TextLengthPtr);

typedef SQLRETURN (*RDS_SQLExecDirect)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      StatementText,
     SQLINTEGER     TextLength);

typedef SQLRETURN (*RDS_SQLExecute)(
     SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_SQLExtendedFetch)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   FetchOrientation,
     SQLLEN         FetchOffset,
     SQLULEN *      RowCountPtr,
     SQLUSMALLINT * RowStatusArray);

typedef SQLRETURN (*RDS_SQLFetch)(
     SQLHSTMT        StatementHandle);

typedef SQLRETURN (*RDS_SQLFetchScroll)(
     SQLHSTMT       StatementHandle,
     SQLSMALLINT    FetchOrientation,
     SQLLEN         FetchOffset);

typedef SQLRETURN (*RDS_SQLForeignKeys)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      PKCatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      PKSchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      PKTableName,
     SQLSMALLINT    NameLength3,
     SQLCHAR *      FKCatalogName,
     SQLSMALLINT    NameLength4,
     SQLCHAR *      FKSchemaName,
     SQLSMALLINT    NameLength5,
     SQLCHAR *      FKTableName,
     SQLSMALLINT    NameLength6);

typedef SQLRETURN (*RDS_SQLFreeConnect)(
     SQLHDBC        ConnectionHandle);

typedef SQLRETURN (*RDS_SQLFreeEnv)(
     SQLHENV        EnvironmentHandle);

typedef SQLRETURN (*RDS_SQLFreeHandle)(
     SQLSMALLINT    HandleType,
     SQLHANDLE      Handle);

typedef SQLRETURN (*RDS_SQLFreeStmt)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   Option);

typedef SQLRETURN (*RDS_SQLGetConnectAttr)(
     SQLHDBC        ConnectionHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     BufferLength,
     SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_SQLGetConnectOption)(
     SQLHDBC        ConnectionHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr);

typedef SQLRETURN (*RDS_SQLGetCursorName)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CursorName,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  NameLengthPtr);

typedef SQLRETURN (*RDS_SQLGetData)(
      SQLHSTMT      StatementHandle,
      SQLUSMALLINT  Col_or_Param_Num,
      SQLSMALLINT   TargetType,
      SQLPOINTER    TargetValuePtr,
      SQLLEN        BufferLength,
      SQLLEN *      StrLen_or_IndPtr);

typedef SQLRETURN (*RDS_SQLGetDescField)(
     SQLHDESC       DescriptorHandle,
     SQLSMALLINT    RecNumber,
     SQLSMALLINT    FieldIdentifier,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     BufferLength,
     SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_SQLGetDescRec)(
     SQLHDESC       DescriptorHandle,
     SQLSMALLINT    RecNumber,
     SQLCHAR *      Name,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  StringLengthPtr,
     SQLSMALLINT *  TypePtr,
     SQLSMALLINT *  SubTypePtr,
     SQLLEN *       LengthPtr,
     SQLSMALLINT *  PrecisionPtr,
     SQLSMALLINT *  ScalePtr,
     SQLSMALLINT *  NullablePtr);

typedef SQLRETURN (*RDS_SQLGetDiagField)(
     SQLSMALLINT    HandleType,
     SQLHANDLE      Handle,
     SQLSMALLINT    RecNumber,
     SQLSMALLINT    DiagIdentifier,
     SQLPOINTER     DiagInfoPtr,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  StringLengthPtr);

typedef SQLRETURN (*RDS_SQLGetDiagRec)(
     SQLSMALLINT    HandleType,
     SQLHANDLE      Handle,
     SQLSMALLINT    RecNumber,
     SQLCHAR *      SQLState,
     SQLINTEGER *   NativeErrorPtr,
     SQLCHAR *      MessageText,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  TextLengthPtr);

typedef SQLRETURN (*RDS_SQLGetEnvAttr)(
     SQLHENV        EnvironmentHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     BufferLength,
     SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_SQLGetFunctions)(
     SQLHDBC        ConnectionHandle,
     SQLUSMALLINT   FunctionId,
     SQLUSMALLINT * SupportedPtr);

typedef SQLRETURN (*RDS_SQLGetInfo)(
     SQLHDBC        ConnectionHandle,
     SQLUSMALLINT   InfoType,
     SQLPOINTER     InfoValuePtr,
     SQLSMALLINT    BufferLength,
     SQLSMALLINT *  StringLengthPtr);

typedef SQLRETURN (*RDS_SQLGetStmtAttr)(
     SQLHSTMT       StatementHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     BufferLength,
     SQLINTEGER *   StringLengthPtr);

typedef SQLRETURN (*RDS_SQLGetStmtOption)(
     SQLHSTMT       StatementHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr);

typedef SQLRETURN (*RDS_SQLGetTypeInfo)(
     SQLHSTMT       StatementHandle,
     SQLSMALLINT    DataType);

typedef SQLRETURN (*RDS_SQLMoreResults)(
     SQLHSTMT       StatementHandle);

typedef SQLRETURN (*RDS_SQLNativeSql)(
     SQLHDBC        ConnectionHandle,
     SQLCHAR *      InStatementText,
     SQLINTEGER     TextLength1,
     SQLCHAR *      OutStatementText,
     SQLINTEGER     BufferLength,
     SQLINTEGER *   TextLength2Ptr);

typedef SQLRETURN (*RDS_SQLNumParams)(
     SQLHSTMT       StatementHandle,
     SQLSMALLINT *  ParameterCountPtr);

typedef SQLRETURN (*RDS_SQLNumResultCols)(
     SQLHSTMT       StatementHandle,
     SQLSMALLINT *  ColumnCountPtr);

typedef SQLRETURN (*RDS_SQLParamData)(
     SQLHSTMT       StatementHandle,
     SQLPOINTER *   ValuePtrPtr);

typedef SQLRETURN (*RDS_SQLParamOptions)(
     SQLHSTMT       StatementHandle,
     SQLINTEGER     Crow,
     SQLINTEGER *   FetchOffsetPtr);

typedef SQLRETURN (*RDS_SQLPrepare)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      StatementText,
     SQLINTEGER     TextLength);

typedef SQLRETURN (*RDS_SQLPrimaryKeys)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      TableName,
     SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_SQLProcedureColumns)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      ProcName,
     SQLSMALLINT    NameLength3,
     SQLCHAR *      ColumnName,
     SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_SQLProcedures)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      ProcName,
     SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_SQLPutData)(
     SQLHSTMT       StatementHandle,
     SQLPOINTER     DataPtr,
     SQLLEN         StrLen_or_Ind);

typedef SQLRETURN (*RDS_SQLRowCount)(
     SQLHSTMT       StatementHandle,
     SQLLEN *       RowCountPtr);

typedef SQLRETURN (*RDS_SQLSetConnectAttr)(
     SQLHDBC        ConnectionHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     StringLength);

typedef SQLRETURN (*RDS_SQLSetConnectOption)(
     SQLHDBC        ConnectionHandle,
     SQLSMALLINT    Option,
     SQLPOINTER     Param);

typedef SQLRETURN (*RDS_SQLSetCursorName)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CursorName,
     SQLSMALLINT    NameLength);

typedef SQLRETURN (*RDS_SQLSetDescField)(
     SQLHDESC       DescriptorHandle,
     SQLSMALLINT    RecNumber,
     SQLSMALLINT    FieldIdentifier,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     BufferLength);

typedef SQLRETURN (*RDS_SQLSetDescRec)(
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

typedef SQLRETURN (*RDS_SQLSetEnvAttr)(
     SQLHENV        EnvironmentHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     StringLength);

typedef SQLRETURN (*RDS_SQLSetParam)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   ParameterNumber,
     SQLSMALLINT    ValueType,
     SQLSMALLINT    ParameterType,
     SQLULEN        ColumnSize,
     SQLSMALLINT    DecimalDigits,
     SQLPOINTER     ParameterValuePtr,
     SQLLEN *       StrLen_or_IndPtr);

typedef SQLRETURN (*RDS_SQLSetPos)(
     SQLHSTMT       StatementHandle,
     SQLSETPOSIROW  RowNumber,
     SQLUSMALLINT   Operation,
     SQLUSMALLINT   LockType);

typedef SQLRETURN (*RDS_SQLSetScrollOptions)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   Concurrency,
     SQLLEN         KeysetSize,
     SQLUSMALLINT   RowsetSize);

typedef SQLRETURN (*RDS_SQLSetStmtAttr)(
     SQLHSTMT       StatementHandle,
     SQLINTEGER     Attribute,
     SQLPOINTER     ValuePtr,
     SQLINTEGER     StringLength);

typedef SQLRETURN (*RDS_SQLSetStmtOption)(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   Option,
     SQLULEN        Param);

typedef SQLRETURN (*RDS_SQLSpecialColumns)(
     SQLHSTMT       StatementHandle,
     SQLSMALLINT    IdentifierType,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      TableName,
     SQLSMALLINT    NameLength3,
     SQLSMALLINT    Scope,
     SQLSMALLINT    Nullable);

typedef SQLRETURN (*RDS_SQLStatistics)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      TableName,
     SQLSMALLINT    NameLength3,
     SQLUSMALLINT   Unique,
     SQLUSMALLINT   Reserved);

typedef SQLRETURN (*RDS_SQLTablePrivileges)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      TableName,
     SQLSMALLINT    NameLength3);

typedef SQLRETURN (*RDS_SQLTables)(
     SQLHSTMT       StatementHandle,
     SQLCHAR *      CatalogName,
     SQLSMALLINT    NameLength1,
     SQLCHAR *      SchemaName,
     SQLSMALLINT    NameLength2,
     SQLCHAR *      TableName,
     SQLSMALLINT    NameLength3,
     SQLCHAR *      TableType,
     SQLSMALLINT    NameLength4);

typedef SQLRETURN (*RDS_SQLTransact)(
     SQLHENV        EnvironmentHandle,
     SQLHDBC        ConnectionHandle,
     SQLUSMALLINT   CompletionType);

#endif // ODBCAPI_H_
