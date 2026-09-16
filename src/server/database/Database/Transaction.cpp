/*
 * Project Ambrose by Imjustchico
 * Appends transaction entries until submitted, commits them on one connection and retries deadlocks, lock timeouts and reconnected losses before COMMIT with backoff until a time limit, signals the completion handler once settled, and runs completion callbacks when polled.
 */

#include "Transaction.h"
#include "Log.h"
#include "MySQLConnection.h"

#include <algorithm>
#include <thread>

namespace
{
    constexpr uint32 ErrorLockWaitTimeout = 1205;
    constexpr uint32 ErrorLockDeadlock = 1213;
}

bool TransactionBase::CheckEditable()
{
    if (!_locked.load())
        return true;
    LOG_ERROR("sql.sql", "A transaction was changed after it was submitted; the change is ignored and the transaction is refused");
    _valid = false;
    return false;
}

void TransactionBase::Append(std::string sql)
{
    if (CheckEditable())
        _entries.emplace_back(std::move(sql));
}

void TransactionBase::AppendStatement(std::unique_ptr<PreparedStatementBase> statement)
{
    if (!CheckEditable())
        return;
    if (!statement)
    {
        LOG_ERROR("sql.sql", "An empty prepared statement was appended to a transaction, so the whole transaction will be refused");
        _valid = false;
        return;
    }
    _entries.emplace_back(std::move(statement));
}

TransactionTask::TransactionTask(std::shared_ptr<TransactionBase> transaction) : _transaction(std::move(transaction))
{
}

bool TransactionTask::Commit(MySQLConnection& connection, TransactionBase const& transaction, std::chrono::milliseconds retryLimit)
{
    auto const start = std::chrono::steady_clock::now();
    std::chrono::milliseconds delay{ 25 };
    for (uint32 attempt = 1;; ++attempt)
    {
        TransactionResult const outcome = connection.ExecuteTransaction(transaction);
        if (outcome.Code == 0)
        {
            if (attempt > 1)
                LOG_INFO("sql.sql", "Transaction on {} committed after {} attempt(s)", connection.GetInfo().ToLogString(), attempt);
            return true;
        }
        bool const lostBeforeCommit = MySQLConnection::IsConnectionLost(outcome.Code, connection.IsMariaDB()) && !outcome.CommitSent && connection.IsOpen();
        bool const retryable = outcome.Code == ErrorLockDeadlock || outcome.Code == ErrorLockWaitTimeout || lostBeforeCommit;
        if (!retryable || std::chrono::steady_clock::now() - start + delay >= retryLimit)
        {
            LOG_ERROR("sql.sql", "Transaction of {} entries on {} failed after {} attempt(s): [{}] {}", transaction.GetSize(), connection.GetInfo().ToLogString(), attempt, outcome.Code, connection.GetLastErrorText());
            return false;
        }
        LOG_WARN("sql.sql", "Transaction on {} was rolled back by [{}] {}; retrying in {} ms, attempt {}", connection.GetInfo().ToLogString(), outcome.Code, connection.GetLastErrorText(), delay.count(), attempt + 1);
        std::this_thread::sleep_for(delay);
        delay = std::min(delay * 2, std::chrono::milliseconds(1000));
    }
}

void TransactionTask::Execute(MySQLConnection& connection)
{
    try
    {
        _result.set_value(_transaction ? Commit(connection, *_transaction) : false);
    }
    catch (...)
    {
        LOG_ERROR("sql.sql", "A queued transaction threw an exception on {}", connection.GetInfo().ToLogString());
        _result.set_exception(std::current_exception());
    }
    NotifyCompleted();
}

void TransactionTask::Cancel()
{
    _result.set_value(false);
    NotifyCompleted();
}

TransactionCallback::TransactionCallback(std::future<bool>&& result) : _result(std::move(result))
{
}

TransactionCallback&& TransactionCallback::AfterComplete(std::function<void(bool)>&& callback)
{
    _callback = std::move(callback);
    return std::move(*this);
}

bool TransactionCallback::IsReady() const
{
    return _result.valid() && _result.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

bool TransactionCallback::InvokeIfReady()
{
    if (_finished)
        return true;
    if (!IsReady())
        return false;
    _finished = true;
    bool committed = false;
    try
    {
        committed = _result.get();
    }
    catch (std::exception const& exception)
    {
        LOG_ERROR("sql.sql", "An async transaction failed: {}", exception.what());
    }
    if (_callback)
        _callback(committed);
    return true;
}
