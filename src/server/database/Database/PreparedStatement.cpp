/*
 * Project Ambrose by Imjustchico
 * Stores parameter values by index, rejects indexes past the statement's placeholder count, describes values for logs without string or blob contents, and runs queued statements.
 */

#include "PreparedStatement.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "QueryResult.h"

#include <fmt/format.h>

PreparedStatementBase::PreparedStatementBase(uint32 index, std::size_t parameterCount) : _index(index), _values(parameterCount)
{
}

void PreparedStatementBase::SetData(std::size_t index, char const* value)
{
    if (!value)
        SetData(index, nullptr);
    else
        SetData(index, std::string(value));
}

void PreparedStatementBase::SetData(std::size_t index, std::string_view value)
{
    SetData(index, std::string(value));
}

void PreparedStatementBase::Clear()
{
    for (PreparedStatementValue& value : _values)
        value = std::monostate();
}

bool PreparedStatementBase::CheckIndex(std::size_t index) const
{
    if (index < _values.size())
        return true;
    LOG_ERROR("sql.sql", "Statement {} has {} parameter(s), so parameter {} cannot be set", _index, _values.size(), index + 1);
    return false;
}

std::string PreparedStatementBase::DescribeValues() const
{
    std::string text;
    for (std::size_t i = 0; i < _values.size(); ++i)
    {
        if (i)
            text += ", ";
        std::visit([&text](auto const& value)
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                text += "<unset>";
            else if constexpr (std::is_same_v<T, std::nullptr_t>)
                text += "NULL";
            else if constexpr (std::is_same_v<T, std::string>)
                text += fmt::format("<{}-byte string>", value.size());
            else if constexpr (std::is_same_v<T, std::vector<uint8>>)
                text += fmt::format("<{} bytes>", value.size());
            else if constexpr (std::is_same_v<T, bool>)
                text += value ? "true" : "false";
            else if constexpr (std::is_same_v<T, uint8> || std::is_same_v<T, int8>)
                text += fmt::format("{}", static_cast<int>(value));
            else
                text += fmt::format("{}", value);
        }, _values[i]);
    }
    return text;
}

PreparedStatementTask::PreparedStatementTask(std::unique_ptr<PreparedStatementBase> statement, bool hasResult) : _statement(std::move(statement)), _hasResult(hasResult)
{
}

void PreparedStatementTask::Execute(MySQLConnection& connection)
{
    try
    {
        PreparedQueryResult result;
        if (!_statement)
            LOG_ERROR("sql.sql", "A queued prepared statement was empty and did not run");
        else if (_hasResult)
            result = connection.Query(*_statement);
        else
            connection.Execute(*_statement);
        _result.set_value(std::move(result));
    }
    catch (...)
    {
        LOG_ERROR("sql.sql", "Queued statement {} threw an exception on {}", _statement ? _statement->GetIndex() : 0, connection.GetInfo().ToLogString());
        _result.set_exception(std::current_exception());
    }
}

void PreparedStatementTask::Cancel()
{
    _result.set_value(nullptr);
}
