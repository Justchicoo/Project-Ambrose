/*
 * Project Ambrose by Imjustchico
 * Steps through stored text results in place, and loads every binary row of a prepared statement into owned buffers with native numbers and full-length strings.
 */

#include "QueryResult.h"
#include "DatabaseTypes.h"

#include <mysql.h>

#include <algorithm>
#include <cstring>
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

namespace
{
    void ReadMetadata(MYSQL_FIELD const* field, uint32 index, FieldMetadata& metadata)
    {
        metadata.TableName = field->org_table ? field->org_table : "";
        metadata.TableAlias = field->table ? field->table : "";
        metadata.Name = field->org_name ? field->org_name : "";
        metadata.Alias = field->name ? field->name : "";
        metadata.Type = DatabaseTypes::FromConnectorType(field->type);
        metadata.Unsigned = (field->flags & UNSIGNED_FLAG) != 0;
        metadata.TypeName = std::string(DatabaseTypes::GetFullTypeName(field->type, metadata.Unsigned));
        metadata.Index = index;
    }

    std::size_t NativeSize(DatabaseFieldType type) noexcept
    {
        switch (type)
        {
            case DatabaseFieldType::Int8: return 1;
            case DatabaseFieldType::Int16: return 2;
            case DatabaseFieldType::Int32: return 4;
            case DatabaseFieldType::Int64: return 8;
            case DatabaseFieldType::Float: return 4;
            case DatabaseFieldType::Double: return 8;
            default: return 0;
        }
    }

    enum_field_types NativeBufferType(DatabaseFieldType type) noexcept
    {
        switch (type)
        {
            case DatabaseFieldType::Int8: return MYSQL_TYPE_TINY;
            case DatabaseFieldType::Int16: return MYSQL_TYPE_SHORT;
            case DatabaseFieldType::Int32: return MYSQL_TYPE_LONG;
            case DatabaseFieldType::Int64: return MYSQL_TYPE_LONGLONG;
            case DatabaseFieldType::Float: return MYSQL_TYPE_FLOAT;
            case DatabaseFieldType::Double: return MYSQL_TYPE_DOUBLE;
            default: return MYSQL_TYPE_STRING;
        }
    }
}

PreparedQueryResult PreparedResultSet::Load(st_mysql_stmt* statement, uint32& errorCode, std::string& errorText)
{
    auto fail = [&]() -> PreparedQueryResult
    {
        errorCode = mysql_stmt_errno(statement);
        errorText = mysql_stmt_error(statement);
        mysql_stmt_reset(statement);
        return nullptr;
    };

    errorCode = 0;
    errorText.clear();
    my_bool const updateMaxLength = 1;
    mysql_stmt_attr_set(statement, STMT_ATTR_UPDATE_MAX_LENGTH, &updateMaxLength);
    if (mysql_stmt_store_result(statement))
        return fail();
    uint32 const fieldCount = mysql_stmt_field_count(statement);
    if (fieldCount == 0)
    {
        mysql_stmt_free_result(statement);
        return nullptr;
    }

    std::shared_ptr<PreparedResultSet> set(new PreparedResultSet());
    set->_fieldCount = fieldCount;
    set->_rowCount = mysql_stmt_num_rows(statement);
    set->_metadata.resize(fieldCount);
    MYSQL_FIELD const* const fields = mariadb_stmt_fetch_fields(statement);
    for (uint32 i = 0; i < fieldCount; ++i)
        ReadMetadata(&fields[i], i, set->_metadata[i]);

    std::vector<MYSQL_BIND> binds(fieldCount);
    std::vector<std::vector<char>> buffers(fieldCount);
    std::vector<unsigned long> lengths(fieldCount);
    std::vector<my_bool> nulls(fieldCount);
    std::vector<my_bool> truncated(fieldCount);
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        std::size_t const native = NativeSize(set->_metadata[i].Type);
        std::size_t const size = native ? native : std::max<std::size_t>(fields[i].max_length, 1);
        buffers[i].resize(size);
        std::memset(&binds[i], 0, sizeof(MYSQL_BIND));
        binds[i].buffer_type = NativeBufferType(set->_metadata[i].Type);
        binds[i].buffer = buffers[i].data();
        binds[i].buffer_length = static_cast<unsigned long>(size);
        binds[i].length = &lengths[i];
        binds[i].is_null = &nulls[i];
        binds[i].error = &truncated[i];
        binds[i].is_unsigned = set->_metadata[i].Unsigned;
    }
    if (mysql_stmt_bind_result(statement, binds.data()))
        return fail();

    set->_rowData.reserve(static_cast<std::size_t>(set->_rowCount));
    set->_fields.resize(static_cast<std::size_t>(set->_rowCount) * fieldCount);
    std::vector<std::size_t> offsets(fieldCount);
    std::vector<std::vector<char>> overflow(fieldCount);
    std::vector<bool> usesOverflow(fieldCount);
    for (uint64 row = 0;; ++row)
    {
        int const status = mysql_stmt_fetch(statement);
        if (status == MYSQL_NO_DATA)
            break;
        if (status == 1)
            return fail();
        if (row >= set->_rowCount)
            break;
        std::fill(usesOverflow.begin(), usesOverflow.end(), false);
        if (status == MYSQL_DATA_TRUNCATED)
        {
            for (uint32 i = 0; i < fieldCount; ++i)
            {
                if (nulls[i] || NativeSize(set->_metadata[i].Type) || lengths[i] <= buffers[i].size())
                    continue;
                overflow[i].resize(lengths[i]);
                MYSQL_BIND column = binds[i];
                column.buffer = overflow[i].data();
                column.buffer_length = lengths[i];
                if (mysql_stmt_fetch_column(statement, &column, i, 0))
                    return fail();
                usesOverflow[i] = true;
            }
        }

        std::size_t total = 0;
        for (uint32 i = 0; i < fieldCount; ++i)
        {
            offsets[i] = total;
            if (!nulls[i])
                total += NativeSize(set->_metadata[i].Type) ? NativeSize(set->_metadata[i].Type) : lengths[i];
        }
        std::vector<char>& data = set->_rowData.emplace_back(total ? total : 1);
        Field* const rowFields = set->_fields.data() + row * fieldCount;
        for (uint32 i = 0; i < fieldCount; ++i)
        {
            if (nulls[i])
            {
                rowFields[i].SetText(nullptr, 0, &set->_metadata[i]);
                continue;
            }
            std::size_t const native = NativeSize(set->_metadata[i].Type);
            std::vector<char> const& source = usesOverflow[i] ? overflow[i] : buffers[i];
            std::size_t const length = native ? native : std::min<std::size_t>(lengths[i], source.size());
            std::memcpy(data.data() + offsets[i], source.data(), length);
            if (native)
                rowFields[i].SetBinary(data.data() + offsets[i], length, &set->_metadata[i]);
            else
                rowFields[i].SetText(data.data() + offsets[i], length, &set->_metadata[i]);
        }
    }
    mysql_stmt_data_seek(statement, 0);
    mysql_stmt_free_result(statement);
    set->_rowCount = set->_rowData.size();
    set->_fields.resize(static_cast<std::size_t>(set->_rowCount) * fieldCount);
    if (set->_rowCount == 0)
        return nullptr;
    return set;
}

bool PreparedResultSet::NextRow()
{
    if (_row + 1 >= _rowCount)
        return false;
    ++_row;
    return true;
}

Field const& PreparedResultSet::operator[](std::size_t index) const
{
    if (index >= _fieldCount)
        throw std::out_of_range("PreparedResultSet field index out of range");
    return Fetch()[index];
}

Field const* PreparedResultSet::FindField(std::string_view name) const noexcept
{
    for (uint32 i = 0; i < _fieldCount; ++i)
        if (_metadata[i].Alias == name || _metadata[i].Name == name)
            return Fetch() + i;
    return nullptr;
}
