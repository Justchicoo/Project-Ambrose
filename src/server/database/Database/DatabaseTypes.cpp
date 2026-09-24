/*
 * Project Ambrose by Imjustchico
 * Translates connector column type codes into field types and SQL type names.
 */

#include "DatabaseTypes.h"

#include <mysql.h>

DatabaseFieldType DatabaseTypes::FromConnectorType(int connectorType) noexcept
{
    switch (static_cast<enum_field_types>(connectorType))
    {
        case MYSQL_TYPE_NULL:
            return DatabaseFieldType::Null;
        case MYSQL_TYPE_TINY:
            return DatabaseFieldType::Int8;
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_YEAR:
            return DatabaseFieldType::Int16;
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            return DatabaseFieldType::Int32;
        case MYSQL_TYPE_LONGLONG:
            return DatabaseFieldType::Int64;
        case MYSQL_TYPE_BIT:
            return DatabaseFieldType::Bit;
        case MYSQL_TYPE_FLOAT:
            return DatabaseFieldType::Float;
        case MYSQL_TYPE_DOUBLE:
            return DatabaseFieldType::Double;
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return DatabaseFieldType::Decimal;
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_NEWDATE:
            return DatabaseFieldType::Date;
        default:
            return DatabaseFieldType::Binary;
    }
}

std::string_view DatabaseTypes::GetConnectorTypeName(int connectorType) noexcept
{
    switch (static_cast<enum_field_types>(connectorType))
    {
        case MYSQL_TYPE_NULL: return "NULL";
        case MYSQL_TYPE_TINY: return "TINYINT";
        case MYSQL_TYPE_SHORT: return "SMALLINT";
        case MYSQL_TYPE_YEAR: return "YEAR";
        case MYSQL_TYPE_INT24: return "MEDIUMINT";
        case MYSQL_TYPE_LONG: return "INT";
        case MYSQL_TYPE_LONGLONG: return "BIGINT";
        case MYSQL_TYPE_BIT: return "BIT";
        case MYSQL_TYPE_FLOAT: return "FLOAT";
        case MYSQL_TYPE_DOUBLE: return "DOUBLE";
        case MYSQL_TYPE_DECIMAL: return "DECIMAL";
        case MYSQL_TYPE_NEWDECIMAL: return "DECIMAL";
        case MYSQL_TYPE_TIMESTAMP: return "TIMESTAMP";
        case MYSQL_TYPE_DATE: return "DATE";
        case MYSQL_TYPE_NEWDATE: return "DATE";
        case MYSQL_TYPE_TIME: return "TIME";
        case MYSQL_TYPE_DATETIME: return "DATETIME";
        case MYSQL_TYPE_VARCHAR: return "VARCHAR";
        case MYSQL_TYPE_VAR_STRING: return "VARCHAR";
        case MYSQL_TYPE_STRING: return "CHAR";
        case MYSQL_TYPE_JSON: return "JSON";
        case MYSQL_TYPE_ENUM: return "ENUM";
        case MYSQL_TYPE_SET: return "SET";
        case MYSQL_TYPE_TINY_BLOB: return "TINYBLOB";
        case MYSQL_TYPE_MEDIUM_BLOB: return "MEDIUMBLOB";
        case MYSQL_TYPE_LONG_BLOB: return "LONGBLOB";
        case MYSQL_TYPE_BLOB: return "BLOB";
        case MYSQL_TYPE_GEOMETRY: return "GEOMETRY";
        default: return "UNKNOWN";
    }
}

std::string_view DatabaseTypes::GetFullTypeName(int connectorType, bool isUnsigned) noexcept
{
    if (!isUnsigned)
        return GetConnectorTypeName(connectorType);
    switch (static_cast<enum_field_types>(connectorType))
    {
        case MYSQL_TYPE_TINY: return "TINYINT UNSIGNED";
        case MYSQL_TYPE_SHORT: return "SMALLINT UNSIGNED";
        case MYSQL_TYPE_INT24: return "MEDIUMINT UNSIGNED";
        case MYSQL_TYPE_LONG: return "INT UNSIGNED";
        case MYSQL_TYPE_LONGLONG: return "BIGINT UNSIGNED";
        default: return GetConnectorTypeName(connectorType);
    }
}

FieldMetadata DatabaseTypes::MakeMetadata(std::string name, std::string_view sqlTypeName, bool isUnsigned)
{
    static constexpr int ConnectorTypes[] = { MYSQL_TYPE_NULL, MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT, MYSQL_TYPE_YEAR, MYSQL_TYPE_INT24, MYSQL_TYPE_LONG, MYSQL_TYPE_LONGLONG,
        MYSQL_TYPE_BIT, MYSQL_TYPE_FLOAT, MYSQL_TYPE_DOUBLE, MYSQL_TYPE_NEWDECIMAL, MYSQL_TYPE_TIMESTAMP, MYSQL_TYPE_DATE, MYSQL_TYPE_TIME, MYSQL_TYPE_DATETIME,
        MYSQL_TYPE_VAR_STRING, MYSQL_TYPE_STRING, MYSQL_TYPE_JSON, MYSQL_TYPE_ENUM, MYSQL_TYPE_SET, MYSQL_TYPE_TINY_BLOB, MYSQL_TYPE_MEDIUM_BLOB, MYSQL_TYPE_LONG_BLOB,
        MYSQL_TYPE_BLOB, MYSQL_TYPE_GEOMETRY };
    int connectorType = MYSQL_TYPE_BLOB;
    for (int const candidate : ConnectorTypes)
        if (GetConnectorTypeName(candidate) == sqlTypeName)
        {
            connectorType = candidate;
            break;
        }
    FieldMetadata metadata;
    metadata.Name = name;
    metadata.Alias = std::move(name);
    metadata.Type = FromConnectorType(connectorType);
    metadata.TypeName = std::string(GetFullTypeName(connectorType, isUnsigned));
    metadata.Unsigned = isUnsigned;
    return metadata;
}
