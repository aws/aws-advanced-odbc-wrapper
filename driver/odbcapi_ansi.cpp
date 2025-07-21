#include "odbcapi.h"
#include "odbcapi_rds_helper.h"

SQLRETURN SQL_API SQLBrowseConnect(
    SQLHDBC        ConnectionHandle,
    SQLCHAR *      InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr)
{
    return RDS_SQLBrowseConnect(
        ConnectionHandle,
        InConnectionString,
        StringLength1,
        OutConnectionString,
        BufferLength,
        StringLength2Ptr
    );
}

SQLRETURN SQL_API SQLColAttribute(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr)
{
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

SQLRETURN SQL_API SQLColAttributes(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLUSMALLINT   FieldIdentifier,
    SQLPOINTER     CharacterAttributePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr,
    SQLLEN *       NumericAttributePtr)
{
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

SQLRETURN SQL_API SQLColumnPrivileges(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      ColumnName,
    SQLSMALLINT    NameLength4)
{
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

SQLRETURN SQL_API SQLColumns(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      ColumnName,
    SQLSMALLINT    NameLength4)
{
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

SQLRETURN SQL_API SQLConnect(
    SQLHDBC        ConnectionHandle,
    SQLCHAR *      ServerName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      UserName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      Authentication,
    SQLSMALLINT    NameLength3)
{
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

SQLRETURN SQL_API SQLDataSources(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLCHAR *      ServerName,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  NameLength1Ptr,
    SQLCHAR *      Description,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  NameLength2Ptr)
{
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

SQLRETURN SQL_API SQLDescribeCol(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLCHAR *      ColumnName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr,
    SQLSMALLINT *  DataTypePtr,
    SQLULEN *      ColumnSizePtr,
    SQLSMALLINT *  DecimalDigitsPtr,
    SQLSMALLINT *  NullablePtr)
{
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

SQLRETURN SQL_API SQLDriverConnect(
    SQLHDBC        ConnectionHandle,
    SQLHWND        WindowHandle,
    SQLCHAR *      InConnectionString,
    SQLSMALLINT    StringLength1,
    SQLCHAR *      OutConnectionString,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLength2Ptr,
    SQLUSMALLINT   DriverCompletion)
{
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

SQLRETURN SQL_API SQLDrivers(
    SQLHENV        EnvironmentHandle,
    SQLUSMALLINT   Direction,
    SQLCHAR *      DriverDescription,
    SQLSMALLINT    BufferLength1,
    SQLSMALLINT *  DescriptionLengthPtr,
    SQLCHAR *      DriverAttributes,
    SQLSMALLINT    BufferLength2,
    SQLSMALLINT *  AttributesLengthPtr)
{
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

SQLRETURN SQL_API SQLError(
    SQLHENV        EnvironmentHandle,
    SQLHDBC        ConnectionHandle,
    SQLHSTMT       StatementHandle,
    SQLCHAR *      SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLCHAR *      MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
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

SQLRETURN SQL_API SQLExecDirect(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      StatementText,
    SQLINTEGER     TextLength)
{
    return RDS_SQLExecDirect(
        StatementHandle,
        StatementText,
        TextLength
    );
}

SQLRETURN SQL_API SQLForeignKeys(
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
    SQLSMALLINT    NameLength6)
{
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

SQLRETURN SQL_API SQLGetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    return RDS_GetConnectAttr(
        ConnectionHandle,
        Attribute,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetConnectOption(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr)
{
    return RDS_SQLGetConnectOption(
        ConnectionHandle,
        Attribute,
        ValuePtr
    );
}

SQLRETURN SQL_API SQLGetCursorName(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CursorName,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  NameLengthPtr)
{
    return RDS_SQLGetCursorName(
        StatementHandle,
        CursorName,
        BufferLength,
        NameLengthPtr
    );
}

SQLRETURN SQL_API SQLGetDescField(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    return RDS_SQLGetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetDescRec(
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
    SQLSMALLINT *  NullablePtr)
{
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

SQLRETURN SQL_API SQLGetDiagField(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    DiagIdentifier,
    SQLPOINTER     DiagInfoPtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr)
{
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

SQLRETURN SQL_API SQLGetDiagRec(
    SQLSMALLINT    HandleType,
    SQLHANDLE      Handle,
    SQLSMALLINT    RecNumber,
    SQLCHAR *      SQLState,
    SQLINTEGER *   NativeErrorPtr,
    SQLCHAR *      MessageText,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  TextLengthPtr)
{
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

SQLRETURN SQL_API SQLGetInfo(
    SQLHDBC        ConnectionHandle,
    SQLUSMALLINT   InfoType,
    SQLPOINTER     InfoValuePtr,
    SQLSMALLINT    BufferLength,
    SQLSMALLINT *  StringLengthPtr)
{
    return RDS_SQLGetInfo(
        ConnectionHandle,
        InfoType,
        InfoValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetStmtAttr(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   StringLengthPtr)
{
    return RDS_SQLGetStmtAttr(
        StatementHandle,
        Attribute,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API SQLGetTypeInfo(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    DataType)
{
    return RDS_SQLGetTypeInfo(
        StatementHandle,
        DataType
    );
}

SQLRETURN SQL_API SQLNativeSql(
    SQLHDBC        ConnectionHandle,
    SQLCHAR *      InStatementText,
    SQLINTEGER     TextLength1,
    SQLCHAR *      OutStatementText,
    SQLINTEGER     BufferLength,
    SQLINTEGER *   TextLength2Ptr)
{
    return RDS_SQLNativeSql(
        ConnectionHandle,
        InStatementText,
        TextLength1,
        OutStatementText,
        BufferLength,
        TextLength2Ptr
    );
}

SQLRETURN SQL_API SQLPrepare(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      StatementText,
    SQLINTEGER     TextLength)
{
    return RDS_SQLPrepare(
        StatementHandle,
        StatementText,
        TextLength
    );
}

SQLRETURN SQL_API SQLPrimaryKeys(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3)
{
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

SQLRETURN SQL_API SQLProcedureColumns(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      ProcName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      ColumnName,
    SQLSMALLINT    NameLength4)
{
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

SQLRETURN SQL_API SQLProcedures(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      ProcName,
    SQLSMALLINT    NameLength3)
{
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

SQLRETURN SQL_API SQLSetConnectAttr(
    SQLHDBC        ConnectionHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    return RDS_SQLSetConnectAttr(
        ConnectionHandle,
        Attribute,
        ValuePtr,
        StringLength
    );
}

SQLRETURN SQL_API SQLSetConnectOption(
    SQLHDBC        ConnectionHandle,
    SQLSMALLINT    Option,
    SQLPOINTER     Param)
{
    return RDS_SQLSetConnectOption(
        ConnectionHandle,
        Option,
        Param
    );
}

SQLRETURN SQL_API SQLSetCursorName(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CursorName,
    SQLSMALLINT    NameLength)
{
    return RDS_SQLSetCursorName(
        StatementHandle,
        CursorName,
        NameLength
    );
}

SQLRETURN SQL_API SQLSetDescField(
    SQLHDESC       DescriptorHandle,
    SQLSMALLINT    RecNumber,
    SQLSMALLINT    FieldIdentifier,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     BufferLength)
{
    return RDS_SQLSetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength
    );
}

SQLRETURN SQL_API SQLSetStmtAttr(
    SQLHSTMT       StatementHandle,
    SQLINTEGER     Attribute,
    SQLPOINTER     ValuePtr,
    SQLINTEGER     StringLength)
{
    return RDS_SQLSetStmtAttr(
        StatementHandle,
        Attribute,
        ValuePtr,
        StringLength
    );
}

SQLRETURN SQL_API SQLSpecialColumns(
    SQLHSTMT       StatementHandle,
    SQLSMALLINT    IdentifierType,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLSMALLINT    Scope,
    SQLSMALLINT    Nullable)
{
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

SQLRETURN SQL_API SQLStatistics(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLUSMALLINT   Unique,
    SQLUSMALLINT   Reserved)
{
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

SQLRETURN SQL_API SQLTablePrivileges(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3)
{
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

SQLRETURN SQL_API SQLTables(
    SQLHSTMT       StatementHandle,
    SQLCHAR *      CatalogName,
    SQLSMALLINT    NameLength1,
    SQLCHAR *      SchemaName,
    SQLSMALLINT    NameLength2,
    SQLCHAR *      TableName,
    SQLSMALLINT    NameLength3,
    SQLCHAR *      TableType,
    SQLSMALLINT    NameLength4)
{
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
