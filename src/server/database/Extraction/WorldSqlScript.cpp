/*
 * Project Ambrose by Imjustchico
 * Builds a DELETE and batched multi-row INSERT statements per replaced table, renders numbers as decimals and text as X'..' hex or '' when empty, writes the script wrapped in START TRANSACTION and COMMIT to a temporary file renamed over the target only once it is complete, commits the statements through the transaction task, telling a refused commit from one whose reply was lost, probes each table with an empty select, and names dbimport when a table does not exist.
 */

#include "WorldSqlScript.h"
#include "DBUpdater.h"
#include "Transaction.h"

#include <fmt/format.h>

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace
{
    constexpr std::string_view HexDigits = "0123456789ABCDEF";
    constexpr std::string_view ScriptHeader = "-- Written by the Project Ambrose extractor from your own Wizard101 install. It holds client text, so keep it out of the repository.\n";
}

void WorldSqlScript::ReplaceTable(std::string_view table, std::vector<std::string_view> const& columns, std::vector<Row> const& rows)
{
    std::string const quotedTable = DBUpdater::QuoteIdentifier(table);
    _statements.push_back(fmt::format("DELETE FROM {}", quotedTable));
    std::string columnList;
    for (std::string_view const column : columns)
        columnList += (columnList.empty() ? "" : ", ") + DBUpdater::QuoteIdentifier(column);
    for (std::size_t first = 0; first < rows.size(); first += RowsPerStatement)
    {
        std::string statement = fmt::format("INSERT INTO {} ({}) VALUES ", quotedTable, columnList);
        std::size_t const end = std::min(rows.size(), first + RowsPerStatement);
        for (std::size_t index = first; index < end; ++index)
        {
            statement += index == first ? "(" : ", (";
            for (std::size_t column = 0; column < rows[index].size(); ++column)
            {
                if (column != 0)
                    statement += ", ";
                statement += Literal(rows[index][column]);
            }
            statement += ')';
        }
        _statements.push_back(std::move(statement));
    }
}

std::string WorldSqlScript::ToText() const
{
    std::string text(ScriptHeader);
    text += "START TRANSACTION;\n";
    for (std::string const& statement : _statements)
    {
        text += statement;
        text += ";\n";
    }
    text += "COMMIT;\n";
    return text;
}

bool WorldSqlScript::WriteFile(std::filesystem::path const& path, std::string& error) const
{
    std::filesystem::path temporary = path;
    temporary += ".partial";
    std::string const text = ToText();
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = fmt::format("cannot create the temporary file beside it: {}", std::error_code(errno, std::generic_category()).message());
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.close();
        if (!stream)
        {
            error = fmt::format("cannot write the whole script: {}", std::error_code(errno, std::generic_category()).message());
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    std::error_code renamed;
    std::filesystem::rename(temporary, path, renamed);
    if (renamed)
    {
        error = fmt::format("cannot move the finished script into place: {}", renamed.message());
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

bool WorldSqlScript::Apply(MySQLConnectionInfo const& info, std::string& error) const
{
    MySQLConnectionSettings settings;
    settings.Flags = ConnectionFlags::Sync;
    MySQLConnection connection(info, settings);
    if (connection.Open() != 0)
    {
        error = fmt::format("cannot connect to {}: {}", info.ToLogString(), connection.GetLastErrorText());
        return false;
    }
    TransactionBase transaction;
    for (std::string const& statement : _statements)
        transaction.Append(statement);
    TransactionResult outcome;
    if (!TransactionTask::Commit(connection, transaction, TransactionTask::DeadlockRetryLimit, &outcome))
    {
        if (outcome.CommitSent && MySQLConnection::IsConnectionLost(outcome.Code, connection.IsMariaDB()))
            error = fmt::format("the connection to {} was lost after COMMIT was sent, so whether the tables were replaced is unknown; run the extractor again: [{}] {}", info.ToLogString(), outcome.Code, connection.GetLastErrorText());
        else
            error = fmt::format("{} refused the rows, and nothing was changed: [{}] {}{}", info.ToLogString(), outcome.Code, connection.GetLastErrorText(),
                outcome.Code == NoSuchTableError ? "; run dbimport to create the world tables first" : "");
        return false;
    }
    return true;
}

bool WorldSqlScript::CheckTables(MySQLConnectionInfo const& info, std::vector<std::string_view> const& tables, std::string& error)
{
    MySQLConnectionSettings settings;
    settings.Flags = ConnectionFlags::Sync;
    MySQLConnection connection(info, settings);
    if (connection.Open() != 0)
    {
        error = fmt::format("cannot connect to {}: {}", info.ToLogString(), connection.GetLastErrorText());
        return false;
    }
    for (std::string_view const table : tables)
    {
        if (!connection.Query(fmt::format("SELECT 1 FROM {} LIMIT 0", DBUpdater::QuoteIdentifier(table))) && connection.GetLastErrorCode() != 0)
        {
            error = fmt::format("{} cannot read {}: [{}] {}{}", info.ToLogString(), table, connection.GetLastErrorCode(), connection.GetLastErrorText(),
                connection.GetLastErrorCode() == NoSuchTableError ? "; run dbimport to create the world tables first" : "");
            return false;
        }
    }
    return true;
}

std::string WorldSqlScript::Literal(Value const& value)
{
    if (uint64 const* const number = std::get_if<uint64>(&value))
        return fmt::format("{}", *number);
    std::string const& text = std::get<std::string>(value);
    if (text.empty())
        return "''";
    std::string literal;
    literal.reserve(text.size() * 2 + 3);
    literal += "X'";
    for (char const c : text)
    {
        uint8 const byte = static_cast<uint8>(c);
        literal += HexDigits[byte >> 4];
        literal += HexDigits[byte & 0x0F];
    }
    literal += '\'';
    return literal;
}
