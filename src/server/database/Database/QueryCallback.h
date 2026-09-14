/*
 * Project Ambrose by Imjustchico
 * The pending result of an async query and the callback that runs once, on whichever thread polls it, after the result arrives.
 */

#ifndef AMBROSE_QUERYCALLBACK_H
#define AMBROSE_QUERYCALLBACK_H

#include "DatabaseEnvFwd.h"

#include <functional>
#include <future>
#include <variant>

class QueryCallback
{
public:
    explicit QueryCallback(std::future<QueryResult>&& result);
    explicit QueryCallback(std::future<PreparedQueryResult>&& result);

    QueryCallback(QueryCallback&&) noexcept = default;
    QueryCallback& operator=(QueryCallback&&) noexcept = default;
    QueryCallback(QueryCallback const&) = delete;
    QueryCallback& operator=(QueryCallback const&) = delete;

    QueryCallback&& WithCallback(std::function<void(QueryResult)>&& callback);
    QueryCallback&& WithPreparedCallback(std::function<void(PreparedQueryResult)>&& callback);

    bool IsReady() const;
    bool IsPrepared() const noexcept { return _result.index() == 1; }
    bool InvokeIfReady();

private:
    std::variant<std::future<QueryResult>, std::future<PreparedQueryResult>> _result;
    std::function<void(QueryResult)> _callback;
    std::function<void(PreparedQueryResult)> _preparedCallback;
    bool _finished = false;
};

#endif
