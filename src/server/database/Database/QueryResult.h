/*
 * Project Ambrose by Imjustchico
 * A text-protocol query result that owns the connector result, its column metadata, and the current row's fields.
 */

#ifndef AMBROSE_QUERYRESULT_H
#define AMBROSE_QUERYRESULT_H

#include "DatabaseEnvFwd.h"
#include "Field.h"

#include <cstddef>
#include <string_view>
#include <vector>

struct st_mysql_res;

class ResultSet
{
public:
    ResultSet(st_mysql_res* result, uint64 rowCount, uint32 fieldCount);
    ~ResultSet();

    ResultSet(ResultSet const&) = delete;
    ResultSet& operator=(ResultSet const&) = delete;

    uint64 GetRowCount() const noexcept { return _rowCount; }
    uint32 GetFieldCount() const noexcept { return _fieldCount; }
    std::vector<FieldMetadata> const& GetFieldMetadata() const noexcept { return _metadata; }

    bool NextRow();
    Field const* Fetch() const noexcept { return _fields.data(); }
    Field const& operator[](std::size_t index) const;
    Field const* FindField(std::string_view name) const noexcept;

private:
    st_mysql_res* _result;
    uint64 _rowCount;
    uint32 _fieldCount;
    std::vector<FieldMetadata> _metadata;
    std::vector<Field> _fields;
};

#endif
