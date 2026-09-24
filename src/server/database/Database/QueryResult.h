/*
 * Project Ambrose by Imjustchico
 * Query results: a text-protocol result over the connector's stored rows, and a binary-protocol result that copies every row of a prepared statement.
 */

#ifndef AMBROSE_QUERYRESULT_H
#define AMBROSE_QUERYRESULT_H

#include "DatabaseEnvFwd.h"
#include "Field.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct st_mysql_res;
struct st_mysql_stmt;

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

class PreparedResultSet
{
public:
    static PreparedQueryResult Load(st_mysql_stmt* statement, uint32& errorCode, std::string& errorText);

    uint64 GetRowCount() const noexcept { return _rowCount; }
    uint32 GetFieldCount() const noexcept { return _fieldCount; }
    std::vector<FieldMetadata> const& GetFieldMetadata() const noexcept { return _metadata; }

    bool NextRow();
    Field const* Fetch() const noexcept { return _fields.data() + _row * _fieldCount; }
    Field const& operator[](std::size_t index) const;
    Field const* FindField(std::string_view name) const noexcept;

private:
    PreparedResultSet() = default;

    uint64 _rowCount = 0;
    uint32 _fieldCount = 0;
    uint64 _row = 0;
    std::vector<FieldMetadata> _metadata;
    std::vector<std::vector<char>> _rowData;
    std::vector<Field> _fields;
};

#endif
