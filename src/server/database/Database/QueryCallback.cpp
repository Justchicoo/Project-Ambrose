/*
 * Project Ambrose by Imjustchico
 * Polls an async query's future without blocking, runs the next callback in the chain when it is ready, and continues only when that callback queued another query, whose own callbacks run first.
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
    return WithChainingCallback([callback = std::move(callback)](QueryCallback&, QueryResult result) { if (callback) callback(std::move(result)); });
}

QueryCallback&& QueryCallback::WithPreparedCallback(std::function<void(PreparedQueryResult)>&& callback)
{
    return WithChainingPreparedCallback([callback = std::move(callback)](QueryCallback&, PreparedQueryResult result) { if (callback) callback(std::move(result)); });
}

QueryCallback&& QueryCallback::WithChainingCallback(TextStep&& callback)
{
    _steps.emplace_back(std::move(callback));
    return std::move(*this);
}

QueryCallback&& QueryCallback::WithChainingPreparedCallback(PreparedStep&& callback)
{
    _steps.emplace_back(std::move(callback));
    return std::move(*this);
}

void QueryCallback::SetNextQuery(QueryCallback&& next)
{
    if (&next == this)
    {
        LOG_ERROR("sql.sql", "A query callback cannot continue with itself");
        return;
    }
    _result = std::move(next._result);
    for (auto step = next._steps.rbegin(); step != next._steps.rend(); ++step)
        _steps.push_front(std::move(*step));
    next._steps.clear();
    _nextQuerySet = true;
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
    while (!_finished)
    {
        if (!IsReady())
            return false;
        if (_steps.empty())
        {
            _finished = true;
            return true;
        }
        auto step = std::move(_steps.front());
        _steps.pop_front();
        _nextQuerySet = false;
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
            if (std::holds_alternative<PreparedStep>(step))
                std::get<PreparedStep>(step)(*this, std::move(result));
            else
                LOG_ERROR("sql.sql", "A text query callback was chained after a prepared query and did not run");
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
            if (std::holds_alternative<TextStep>(step))
                std::get<TextStep>(step)(*this, std::move(result));
            else
                LOG_ERROR("sql.sql", "A prepared query callback was chained after a text query and did not run");
        }
        if (!_nextQuerySet)
            _finished = true;
    }
    return true;
}
