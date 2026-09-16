/*
 * Project Ambrose by Imjustchico
 * Stores statements by slot, runs every set slot in order on one connection, hands back each slot's result, settles the queued holder task and signals its completion handler, and runs its completion callback when polled.
 */

#include "QueryHolder.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "QueryResult.h"

#include <chrono>

SQLQueryHolderBase::SQLQueryHolderBase(std::size_t slots) : _slots(slots)
{
}

SQLQueryHolderBase::~SQLQueryHolderBase() = default;

bool SQLQueryHolderBase::SetPreparedQueryBase(std::size_t slot, std::unique_ptr<PreparedStatementBase> statement)
{
    if (slot >= _slots.size())
    {
        LOG_ERROR("sql.sql", "Query holder has {} slot(s), so slot {} cannot be set", _slots.size(), slot);
        return false;
    }
    _slots[slot].Statement = std::move(statement);
    _slots[slot].Result = nullptr;
    return true;
}

PreparedQueryResult SQLQueryHolderBase::GetPreparedResult(std::size_t slot) const
{
    return slot < _slots.size() ? _slots[slot].Result : nullptr;
}

void SQLQueryHolderBase::Run(MySQLConnection& connection)
{
    for (Slot& slot : _slots)
        if (slot.Statement)
            slot.Result = connection.Query(*slot.Statement);
}

QueryHolderTask::QueryHolderTask(std::shared_ptr<SQLQueryHolderBase> holder) : _holder(std::move(holder))
{
}

void QueryHolderTask::Execute(MySQLConnection& connection)
{
    try
    {
        if (_holder)
            _holder->Run(connection);
        _done.set_value();
    }
    catch (...)
    {
        LOG_ERROR("sql.sql", "A queued query holder threw an exception on {}", connection.GetInfo().ToLogString());
        _done.set_exception(std::current_exception());
    }
    NotifyCompleted();
}

void QueryHolderTask::Cancel()
{
    _done.set_value();
    NotifyCompleted();
}

SQLQueryHolderCallback::SQLQueryHolderCallback(std::shared_ptr<SQLQueryHolderBase> holder, std::future<void>&& done) : _holder(std::move(holder)), _done(std::move(done))
{
}

SQLQueryHolderCallback&& SQLQueryHolderCallback::AfterComplete(std::function<void(SQLQueryHolderBase const&)>&& callback)
{
    _callback = std::move(callback);
    return std::move(*this);
}

bool SQLQueryHolderCallback::IsReady() const
{
    return _done.valid() && _done.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

bool SQLQueryHolderCallback::InvokeIfReady()
{
    if (_finished)
        return true;
    if (!IsReady())
        return false;
    _finished = true;
    try
    {
        _done.get();
    }
    catch (std::exception const& exception)
    {
        LOG_ERROR("sql.sql", "An async query holder failed: {}", exception.what());
    }
    if (_callback && _holder)
        _callback(*_holder);
    return true;
}
