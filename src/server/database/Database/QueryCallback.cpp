/*
 * Project Ambrose by Imjustchico
 * Checks an async query's future without blocking and runs the matching text or prepared callback exactly once when it is ready, turning a failed query into an empty result.
 */

#include "QueryCallback.h"
#include "Log.h"
#include "QueryResult.h"

#include <chrono>

QueryCallback::QueryCallback(std::future<QueryResult>&& result) : _result(std::move(result))
{
}

QueryCallback::QueryCallback(std::future<PreparedQueryResult>&& result) : _result(std::move(result))
{
}

QueryCallback&& QueryCallback::WithCallback(std::function<void(QueryResult)>&& callback)
{
    if (IsPrepared())
        LOG_ERROR("sql.sql", "A text query callback was attached to a prepared query and will not run");
    _callback = std::move(callback);
    return std::move(*this);
}

QueryCallback&& QueryCallback::WithPreparedCallback(std::function<void(PreparedQueryResult)>&& callback)
{
    if (!IsPrepared())
        LOG_ERROR("sql.sql", "A prepared query callback was attached to a text query and will not run");
    _preparedCallback = std::move(callback);
    return std::move(*this);
}

bool QueryCallback::IsReady() const
{
    return std::visit([](auto const& future)
    {
        return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }, _result);
}

bool QueryCallback::InvokeIfReady()
{
    if (_finished)
        return true;
    if (!IsReady())
        return false;
    _finished = true;
    if (IsPrepared())
    {
        PreparedQueryResult result;
        try
        {
            result = std::get<1>(_result).get();
        }
        catch (std::exception const& exception)
        {
            LOG_ERROR("sql.sql", "An async prepared query failed: {}", exception.what());
        }
        if (_preparedCallback)
            _preparedCallback(std::move(result));
    }
    else
    {
        QueryResult result;
        try
        {
            result = std::get<0>(_result).get();
        }
        catch (std::exception const& exception)
        {
            LOG_ERROR("sql.sql", "An async query failed: {}", exception.what());
        }
        if (_callback)
            _callback(std::move(result));
    }
    return true;
}
