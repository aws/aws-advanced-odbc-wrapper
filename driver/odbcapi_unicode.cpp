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

#include "odbcapi.h"
#include "odbcapi_rds_helper.h"

#include "util/logger_wrapper.h"

SQLRETURN SQL_API SQLBrowseConnectW(
    SQLHDBC        ConnectionHandle,
    SQLWCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLWCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr)
{
    LOG(INFO) << "Entering SQLBrowseConnectW";
    return RDS_SQLBrowseConnect(
        ConnectionHandle,
        InConnectionString,
        StringLength1,
        OutConnectionString,
        BufferLength,
        StringLength2Ptr
    );
}

SQLRETURN SQL_API SQLColAttributeW(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr)
{
    LOG(INFO) << "Entering SQLColAttributeW";
    return RDS_SQLColAttribute(
        StatementHandle,
        ColumnNumber,
        FieldIdentifier,
        CharacterAttributePtr,
        BufferLength,
        StringLengthPtr,
        NumericAttributePtr
    );
}

SQLRETURN SQL_API SQLColAttributesW(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr)
{
    LOG(INFO) << "Entering SQLColAttributesW";
    return RDS_SQLColAttribute(
        StatementHandle,
        ColumnNumber,
        FieldIdentifier,
        CharacterAttributePtr,
        BufferLength,
        StringLengthPtr,
        NumericAttributePtr
    );
}

SQLRETURN SQL_API SQLColumnPrivilegesW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLWCHAR *     ColumnName,
    SQLSMALLINT    NameLength4)
{
    LOG(INFO) << "Entering SQLColumnPrivilegesW";
    return RDS_SQLColumnPrivileges(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        TableName,
        NameLength3,
        ColumnName,
        NameLength4
    );
}

SQLRETURN SQL_API SQLColumnsW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLWCHAR *     ColumnName,
    SQLSMALLINT    NameLength4)
{
    LOG(INFO) << "Entering SQLColumnsW";
    return RDS_SQLColumns(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        TableName,
        NameLength3,
        ColumnName,
        NameLength4
    );
}

SQLRETURN SQL_API SQLConnectW(
    SQLHDBC        ConnectionHandle,
    SQLWCHAR *     ServerName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     UserName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     Authentication,
    SQLSMALLINT    NameLength3)
{
    LOG(INFO) << "Entering SQLConnectW";
    return RDS_SQLConnect(
        ConnectionHandle,
        ServerName,
        NameLength1,
        UserName,
        NameLength2,
        Authentication,
        NameLength3
    );
}

SQLRETURN SQL_API SQLDataSourcesW(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLWCHAR *     ServerName,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  NameLength1Ptr,
    SQLWCHAR *     Description,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  NameLength2Ptr)
{
    LOG(INFO) << "Entering SQLDataSourcesW";
    return RDS_SQLDataSources(
        EnvironmentHandle,
        Direction,
        ServerName,
        BufferLength1,
        NameLength1Ptr,
        Description,
        BufferLength2,
        NameLength2Ptr
    );
}

SQLRETURN SQL_API SQLDescribeColW(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLWCHAR *     ColumnName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr,
    SQLSMALLINT *  DataTypePtr,
    SQLULEN *      ColumnSizePtr,
    SQLSMALLINT *  DecimalDigitsPtr,
    SQLSMALLINT *  NullablePtr)
{
    LOG(INFO) << "Entering SQLDescribeColW";
    return RDS_SQLDescribeCol(
        StatementHandle,
        ColumnNumber,
        ColumnName,
        BufferLength,
        NameLengthPtr,
        DataTypePtr,
        ColumnSizePtr,
        DecimalDigitsPtr,
        NullablePtr
    );
}

SQLRETURN SQL_API SQLDriverConnectW(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLWCHAR *     InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLWCHAR *     OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr,
    SQLUSMALLINT   DriverCompletion)
{
    LOG(INFO) << "Entering SQLDriverConnectW";
    return RDS_SQLDriverConnect(
        ConnectionHandle,
        WindowHandle,
        InConnectionString,
        StringLength1,
        OutConnectionString,
        BufferLength,
        StringLength2Ptr,
        DriverCompletion
    );
}

SQLRETURN SQL_API SQLDriversW(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLWCHAR *     DriverDescription,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  DescriptionLengthPtr,
    SQLWCHAR *     DriverAttributes,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  AttributesLengthPtr)
{
    LOG(INFO) << "Entering SQLDriversW";
    return RDS_SQLDrivers(
        EnvironmentHandle,
        Direction,
        DriverDescription,
        BufferLength1,
        DescriptionLengthPtr,
        DriverAttributes,
        BufferLength2,
        AttributesLengthPtr
    );
}

SQLRETURN SQL_API SQLErrorW(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLWCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
    LOG(INFO) << "Entering SQLErrorW";
    return RDS_SQLError(
        EnvironmentHandle,
        ConnectionHandle,
        StatementHandle,
        SQLState,
        NativeErrorPtr,
        MessageText,
        BufferLength,
        TextLengthPtr
    );
}

SQLRETURN SQL_API SQLExecDirectW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    LOG(INFO) << "Entering SQLExecDirectW";
    return RDS_SQLExecDirect(
        StatementHandle,
        StatementText,
        TextLength
    );
}

SQLRETURN SQL_API SQLForeignKeysW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     PKCatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     PKSchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     PKTableName,
    SQLSMALLINT    NameLength3,
    SQLWCHAR *     FKCatalogName,
    SQLSMALLINT    NameLength4,
    SQLWCHAR *     FKSchemaName,
    SQLSMALLINT    NameLength5,
    SQLWCHAR *     FKTableName,
    SQLSMALLINT    NameLength6)
{
    LOG(INFO) << "Entering SQLForeignKeysW";
    return RDS_SQLForeignKeys(
        StatementHandle,
        PKCatalogName,
        NameLength1,
        PKSchemaName,
        NameLength2,
        PKTableName,
        NameLength3,
        FKCatalogName,
        NameLength4,
        FKSchemaName,
        NameLength5,
        FKTableName,
        NameLength6
    );
}

SQLRETURN SQL_API SQLGetConnectAttrW(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    LOG(INFO) << "Entering SQLGetConnectAttrW";
    return RDS_GetConnectAttr(
        ConnectionHandle,
        Attribute,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetConnectOptionW(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   Attribute,
    SQLPOINTER     ValuePtr)
{
    LOG(INFO) << "Entering SQLGetConnectOptionW";
    return RDS_SQLGetConnectOption(
        ConnectionHandle,
        Attribute,
        ValuePtr
    );
}

SQLRETURN SQL_API SQLGetCursorNameW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CursorName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr)
{
    LOG(INFO) << "Entering SQLGetCursorNameW";
    return RDS_SQLGetCursorName(
        StatementHandle,
        CursorName,
        BufferLength,
        NameLengthPtr
    );
}

SQLRETURN SQL_API SQLGetDescFieldW(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    LOG(INFO) << "Entering SQLGetDescFieldW";
    return RDS_SQLGetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetDescRecW(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLWCHAR *     Name,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLSMALLINT *  TypePtr,
    SQLSMALLINT *  SubTypePtr,
    SQLLEN *       LengthPtr,
    SQLSMALLINT *  PrecisionPtr,
    SQLSMALLINT *  ScalePtr,
    SQLSMALLINT *  NullablePtr)
{
    LOG(INFO) << "Entering SQLGetDescRecW";
    return RDS_SQLGetDescRec(
        DescriptorHandle,
        RecNumber,
        Name,
        BufferLength,
        StringLengthPtr,
        TypePtr,
        SubTypePtr,
        LengthPtr,
        PrecisionPtr,
        ScalePtr,
        NullablePtr
    );
}

SQLRETURN SQL_API SQLGetDiagFieldW(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    DiagIdentifier,
    SQLPOINTER     DiagInfoPtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr)
{
    LOG(INFO) << "Entering SQLGetDiagFieldW";
    return RDS_SQLGetDiagField(
        HandleType,
        Handle,
        RecNumber,
        DiagIdentifier,
        DiagInfoPtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetDiagRecW(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLWCHAR *     SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLWCHAR *     MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
    LOG(INFO) << "Entering SQLGetDiagRecW";
    return RDS_SQLGetDiagRec(
        HandleType,
        Handle,
        RecNumber,
        SQLState,
        NativeErrorPtr,
        MessageText,
        BufferLength,
        TextLengthPtr
    );
}

SQLRETURN SQL_API SQLGetInfoW(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   InfoType,
    SQLPOINTER     InfoValuePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr)
{
    LOG(INFO) << "Entering SQLGetInfoW";
    return RDS_SQLGetInfo(
        ConnectionHandle,
        InfoType,
        InfoValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetStmtAttrW(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    LOG(INFO) << "Entering SQLGetStmtAttrW";
    return RDS_SQLGetStmtAttr(
        StatementHandle,
        Attribute,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetTypeInfoW(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    DataType)
{
    LOG(INFO) << "Entering SQLGetTypeInfoW";
    return RDS_SQLGetTypeInfo(
        StatementHandle,
        DataType
    );
}

SQLRETURN SQL_API SQLNativeSqlW(
    SQLHDBC        ConnectionHandle,
    SQLWCHAR *     InStatementText,
    SQLINTEGER     TextLength1,
    SQLWCHAR *     OutStatementText,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   TextLength2Ptr)
{
    LOG(INFO) << "Entering SQLNativeSqlW";
    return RDS_SQLNativeSql(
        ConnectionHandle,
        InStatementText,
        TextLength1,
        OutStatementText,
        BufferLength,
        TextLength2Ptr
    );
}

SQLRETURN SQL_API SQLPrepareW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     StatementText,
    SQLINTEGER     TextLength)
{
    LOG(INFO) << "Entering SQLPrepareW";
    return RDS_SQLPrepare(
        StatementHandle,
        StatementText,
        TextLength
    );
}

SQLRETURN SQL_API SQLPrimaryKeysW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     TableName,
    SQLSMALLINT    NameLength3)
{
    LOG(INFO) << "Entering SQLPrimaryKeysW";
    return RDS_SQLPrimaryKeys(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        TableName,
        NameLength3
    );
}

SQLRETURN SQL_API SQLProcedureColumnsW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     ProcName,
    SQLSMALLINT    NameLength3,
    SQLWCHAR *     ColumnName,
    SQLSMALLINT    NameLength4)
{
    LOG(INFO) << "Entering SQLProcedureColumnsW";
    return RDS_SQLProcedureColumns(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        ProcName,
        NameLength3,
        ColumnName,
        NameLength4
    );
}

SQLRETURN SQL_API SQLProceduresW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     ProcName,
    SQLSMALLINT    NameLength3)
{
    LOG(INFO) << "Entering SQLProceduresW";
    return RDS_SQLProcedures(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        ProcName,
        NameLength3
    );
}

SQLRETURN SQL_API SQLSetConnectAttrW(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    LOG(INFO) << "Entering SQLSetConnectAttrW";
    return RDS_SQLSetConnectAttr(
        ConnectionHandle,
        Attribute,
        ValuePtr,
        StringLength
    );
}

SQLRETURN SQL_API SQLSetConnectOptionW(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   Option,
    SQLULEN        Param)
{
    LOG(INFO) << "Entering SQLSetConnectOptionW";
    return RDS_SQLSetConnectOption(
        ConnectionHandle,
        Option,
        Param
    );
}

SQLRETURN SQL_API SQLSetCursorNameW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CursorName,
    SQLSMALLINT    NameLength)
{
    LOG(INFO) << "Entering SQLSetCursorNameW";
    return RDS_SQLSetCursorName(
        StatementHandle,
        CursorName,
        NameLength
    );
}

SQLRETURN SQL_API SQLSetDescFieldW(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength)
{
    LOG(INFO) << "Entering SQLSetDescFieldW";
    return RDS_SQLSetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength
    );
}

SQLRETURN SQL_API SQLSetStmtAttrW(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    LOG(INFO) << "Entering SQLSetStmtAttrW";
    return RDS_SQLSetStmtAttr(
        StatementHandle,
        Attribute,
        ValuePtr,
        StringLength
    );
}

SQLRETURN SQL_API SQLSpecialColumnsW(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   IdentifierType,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Scope,
    SQLUSMALLINT   Nullable)
{
    LOG(INFO) << "Entering SQLSpecialColumnsW";
    return RDS_SQLSpecialColumns(
        StatementHandle,
        IdentifierType,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        TableName,
        NameLength3,
        Scope,
        Nullable
    );
}

SQLRETURN SQL_API SQLStatisticsW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Unique,
    SQLUSMALLINT   Reserved)
{
    LOG(INFO) << "Entering SQLStatisticsW";
    return RDS_SQLStatistics(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        TableName,
        NameLength3,
        Unique,
        Reserved
    );
}

SQLRETURN SQL_API SQLTablePrivilegesW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     TableName,
    SQLSMALLINT    NameLength3)
{
    LOG(INFO) << "Entering SQLTablePrivilegesW";
    return RDS_SQLTablePrivileges(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        TableName,
        NameLength3
    );
}

SQLRETURN SQL_API SQLTablesW(
    SQLHSTMT       StatementHandle,
    SQLWCHAR *     CatalogName,
    SQLSMALLINT    NameLength1,
    SQLWCHAR *     SchemaName,
    SQLSMALLINT    NameLength2,
    SQLWCHAR *     TableName,
    SQLSMALLINT    NameLength3,
    SQLWCHAR *     TableType,
    SQLSMALLINT    NameLength4)
{
    LOG(INFO) << "Entering SQLTablesW";
    return RDS_SQLTables(
        StatementHandle,
        CatalogName,
        NameLength1,
        SchemaName,
        NameLength2,
        TableName,
        NameLength3,
        TableType,
        NameLength4
    );
}
