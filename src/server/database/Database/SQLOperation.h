/*
 * Project Ambrose by Imjustchico
 * The interface of queued database work: run on a worker's connection, or settle empty when the pool closes or cancels it first, calling an optional completion handler exactly once after its result is settled, even if the work is dropped unrun.
 */

#ifndef AMBROSE_SQLOPERATION_H
#define AMBROSE_SQLOPERATION_H

#include <functional>

class MySQLConnection;

class SQLOperation
{
public:
    using CompletionHandler = std::function<void()>;

    virtual ~SQLOperation();

    virtual void Execute(MySQLConnection& connection) = 0;
    virtual void Cancel() = 0;

    void SetCompletionHandler(CompletionHandler handler) { _completion = std::move(handler); }

protected:
    void NotifyCompleted() noexcept;

private:
    CompletionHandler _completion;
    bool _notified = false;
};

#endif
