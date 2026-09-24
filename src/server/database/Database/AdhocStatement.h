/*
 * Project Ambrose by Imjustchico
 * Queued ad hoc SQL text, run for its effect or its result on an async connection.
 */

#ifndef AMBROSE_ADHOCSTATEMENT_H
#define AMBROSE_ADHOCSTATEMENT_H

#include "DatabaseEnvFwd.h"
#include "SQLOperation.h"

#include <future>
#include <string>

class AdhocStatementTask : public SQLOperation
{
public:
    AdhocStatementTask(std::string sql, bool hasResult);

    std::future<QueryResult> GetFuture() { return _result.get_future(); }

    void Execute(MySQLConnection& connection) override;
    void Cancel() override;

private:
    std::string _sql;
    bool _hasResult;
    std::promise<QueryResult> _result;
};

#endif
