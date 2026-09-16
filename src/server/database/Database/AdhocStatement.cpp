/*
 * Project Ambrose by Imjustchico
 * Runs queued SQL text on the worker's connection and settles its promise with the result, empty, or the exception that escaped, then signals its completion handler.
 */

#include "AdhocStatement.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "QueryResult.h"

AdhocStatementTask::AdhocStatementTask(std::string sql, bool hasResult) : _sql(std::move(sql)), _hasResult(hasResult)
{
}

void AdhocStatementTask::Execute(MySQLConnection& connection)
{
    try
    {
        QueryResult result;
        if (_hasResult)
            result = connection.Query(_sql);
        else
            connection.Execute(_sql);
        _result.set_value(std::move(result));
    }
    catch (...)
    {
        LOG_ERROR("sql.sql", "Queued SQL threw an exception on {}", connection.GetInfo().ToLogString());
        _result.set_exception(std::current_exception());
    }
    NotifyCompleted();
}

void AdhocStatementTask::Cancel()
{
    _result.set_value(nullptr);
    NotifyCompleted();
}
