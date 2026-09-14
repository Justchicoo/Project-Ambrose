/*
 * Project Ambrose by Imjustchico
 * The pending result of an async query and a chain of callbacks that run on whichever thread polls it, where each step may queue the next query and continue the chain.
 */

#ifndef AMBROSE_QUERYCALLBACK_H
#define AMBROSE_QUERYCALLBACK_H

#include "DatabaseEnvFwd.h"

#include <deque>
#include <functional>
#include <future>
#include <variant>

class QueryCallback
{
public:
    using TextStep = std::function<void(QueryCallback&, QueryResult)>;
    using PreparedStep = std::function<void(QueryCallback&, PreparedQueryResult)>;

    explicit QueryCallback(std::future<QueryResult>&& result);
    explicit QueryCallback(std::future<PreparedQueryResult>&& result);

    QueryCallback(QueryCallback&&) noexcept = default;
    QueryCallback& operator=(QueryCallback&&) noexcept = default;
    QueryCallback(QueryCallback const&) = delete;
    QueryCallback& operator=(QueryCallback const&) = delete;

    QueryCallback&& WithCallback(std::function<void(QueryResult)>&& callback);
    QueryCallback&& WithPreparedCallback(std::function<void(PreparedQueryResult)>&& callback);
    QueryCallback&& WithChainingCallback(TextStep&& callback);
    QueryCallback&& WithChainingPreparedCallback(PreparedStep&& callback);

    void SetNextQuery(QueryCallback&& next);

    bool IsReady() const;
    bool IsPrepared() const noexcept { return _result.index() == 1; }
    bool InvokeIfReady();

private:
    std::variant<std::future<QueryResult>, std::future<PreparedQueryResult>> _result;
    std::deque<std::variant<TextStep, PreparedStep>> _steps;
    bool _nextQuerySet = false;
    bool _finished = false;
};

#endif
