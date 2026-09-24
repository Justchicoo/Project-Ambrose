/*
 * Project Ambrose by Imjustchico
 * Maps MariaDB Connector/C column type codes to field types and printable names, so tests can build fields without a server.
 */

#ifndef AMBROSE_DATABASETYPES_H
#define AMBROSE_DATABASETYPES_H

#include "Field.h"

#include <string_view>

namespace DatabaseTypes
{
    DatabaseFieldType FromConnectorType(int connectorType) noexcept;
    std::string_view GetConnectorTypeName(int connectorType) noexcept;
    std::string_view GetFullTypeName(int connectorType, bool isUnsigned) noexcept;
    FieldMetadata MakeMetadata(std::string name, std::string_view sqlTypeName, bool isUnsigned);
}

#endif
