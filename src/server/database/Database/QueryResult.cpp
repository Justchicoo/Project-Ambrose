/*
 * Project Ambrose by Imjustchico
 * Steps through a stored result, pointing each field at the connector's row buffer, which stays valid until the result set is destroyed.
 */

#include "QueryResult.h"
#include "DatabaseTypes.h"

#include <mysql.h>

#include <stdexcept>

ResultSet::ResultSet(st_mysql_res* result, uint64 rowCount, uint32 fieldCount)
    : _result(result), _rowCount(rowCount), _fieldCount(fieldCount), _metadata(fieldCount), _fields(fieldCount)
{
    MYSQL_FIELD const* const fields = mysql_fetch_fields(_result);
    for (uint32 i = 0; i < _fieldCount; ++i)
    {
        FieldMetadata& metadata = _metadata[i];
        metadata.TableName = fields[i].org_table ? fields[i].org_table : "";
        metadata.TableAlias = fields[i].table ? fields[i].table : "";
        metadata.Name = fields[i].org_name ? fields[i].org_name : "";
        metadata.Alias = fields[i].name ? fields[i].name : "";
        metadata.Type = DatabaseTypes::FromConnectorType(fields[i].type);
        metadata.Unsigned = (fields[i].flags & UNSIGNED_FLAG) != 0;
        metadata.TypeName = std::string(DatabaseTypes::GetFullTypeName(fields[i].type, metadata.Unsigned));
        metadata.Index = i;
    }
}

ResultSet::~ResultSet()
{
    if (_result)
        mysql_free_result(_result);
}

bool ResultSet::NextRow()
{
    if (!_result)
        return false;
    MYSQL_ROW const row = mysql_fetch_row(_result);
    if (!row)
        return false;
    unsigned long const* const lengths = mysql_fetch_lengths(_result);
    for (uint32 i = 0; i < _fieldCount; ++i)
        _fields[i].SetText(row[i], row[i] ? lengths[i] : 0, &_metadata[i]);
    return true;
}

Field const& ResultSet::operator[](std::size_t index) const
{
    if (index >= _fields.size())
        throw std::out_of_range("ResultSet field index out of range");
    return _fields[index];
}

Field const* ResultSet::FindField(std::string_view name) const noexcept
{
    for (uint32 i = 0; i < _fieldCount; ++i)
        if (_metadata[i].Alias == name || _metadata[i].Name == name)
            return &_fields[i];
    return nullptr;
}
