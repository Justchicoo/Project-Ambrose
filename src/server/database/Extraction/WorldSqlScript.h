/*
 * Project Ambrose by Imjustchico
 * A script of world database statements that replaces whole tables with extracted rows, or only the rows a column marks as extracted, adds rows to a table, and can take in another script's statements after its own, whose text values travel as hex literals so no sql_mode or escaping can change them, and a value that holds nothing is NULL; it renders as one transaction written whole to a file or not at all, commits its statements in one transaction on a connection and says when a lost connection leaves the commit's outcome unknown, and can check that a database holds the tables it would write.
 */

#ifndef AMBROSE_WORLDSQLSCRIPT_H
#define AMBROSE_WORLDSQLSCRIPT_H

#include "MySQLConnection.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class WorldSqlScript
{
public:
    using Value = std::variant<uint64, std::string, int64, double, std::monostate>;
    using Row = std::vector<Value>;

    static constexpr std::size_t RowsPerStatement = 256;
    static constexpr uint32 NoSuchTableError = 1146;

    void ReplaceTable(std::string_view table, std::vector<std::string_view> const& columns, std::vector<Row> const& rows);
    void ReplaceRows(std::string_view table, std::string_view column, Value const& value, std::vector<std::string_view> const& columns, std::vector<Row> const& rows);
    void InsertRows(std::string_view table, std::vector<std::string_view> const& columns, std::vector<Row> const& rows);
    void Append(WorldSqlScript const& other);

    std::vector<std::string> const& GetStatements() const noexcept { return _statements; }
    std::string ToText() const;
    bool WriteFile(std::filesystem::path const& path, std::string& error) const;
    bool Apply(MySQLConnectionInfo const& info, std::string& error) const;

    static bool CheckTables(MySQLConnectionInfo const& info, std::vector<std::string_view> const& tables, std::string& error);

    static std::string Literal(Value const& value);

private:
    std::vector<std::string> _statements;
};

#endif
