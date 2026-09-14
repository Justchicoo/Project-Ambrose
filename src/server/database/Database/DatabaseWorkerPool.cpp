/*
 * Project Ambrose by Imjustchico
 * Builds and validates a new connection generation before publishing it, takes a snapshot of the current generation for every call, refuses empty or wrong-side statements, and drains a retired generation on close.
 */

#include "DatabaseWorkerPool.h"
#include "AdhocStatement.h"
#include "Log.h"
#include "QueryHolder.h"
#include "QueryResult.h"

#include <algorithm>

namespace
{
    class SyncLease
    {
    public:
        explicit SyncLease(std::shared_ptr<DatabaseConnectionSet> set) : _set(std::move(set)), _connection(_set ? _set->AcquireSync() : nullptr)
        {
        }

        ~SyncLease()
        {
            if (_connection)
                _set->ReleaseSync(_connection);
        }

        SyncLease(SyncLease const&) = delete;
        SyncLease& operator=(SyncLease const&) = delete;

        MySQLConnection* operator->() const noexcept { return _connection; }
        explicit operator bool() const noexcept { return _connection != nullptr; }

    private:
        std::shared_ptr<DatabaseConnectionSet> _set;
        MySQLConnection* _connection;
    };
}

DatabaseWorkerPoolBase::DatabaseWorkerPoolBase(std::string name, ConnectionFactory factory) : _name(std::move(name)), _factory(std::move(factory))
{
}

DatabaseWorkerPoolBase::~DatabaseWorkerPoolBase()
{
    std::shared_ptr<DatabaseConnectionSet> retired;
    {
        std::lock_guard<std::mutex> lock(_currentMutex);
        retired = std::move(_current);
    }
    if (retired)
        retired->Shutdown(std::chrono::milliseconds(0));
}

bool DatabaseWorkerPoolBase::SetConnectionInfo(std::string_view infoString, uint32 asyncThreads, uint32 syncThreads)
{
    std::string error;
    std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(infoString, &error);
    if (!info)
    {
        LOG_ERROR("sql.driver", "Database pool {} has an invalid connection string: {}", _name, error);
        return false;
    }
    uint32 const async = std::min(asyncThreads, MaxThreads);
    uint32 const sync = std::clamp<uint32>(syncThreads, 1, MaxThreads);
    if (async != asyncThreads || sync != syncThreads)
        LOG_WARN("sql.driver", "Database pool {} uses {} async and {} sync connection(s) instead of {} and {} (sync 1-{}, async 0-{})", _name, async, sync, asyncThreads, syncThreads, MaxThreads, MaxThreads);
    std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
    _info = std::move(info);
    _asyncThreads = async;
    _syncThreads = sync;
    return true;
}

void DatabaseWorkerPoolBase::SetConnectionSettings(MySQLConnectionSettings const& settings)
{
    std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
    _settings = settings;
}

uint32 DatabaseWorkerPoolBase::Open()
{
    std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
    if (IsOpen())
        return 0;
    if (!_info)
    {
        LOG_ERROR("sql.driver", "Database pool {} has no connection string", _name);
        return 2000;
    }

    auto set = std::make_shared<DatabaseConnectionSet>(_name, _keepAliveMs);
    if (uint32 const error = set->Open(_factory, *_info, _settings, _asyncThreads, _syncThreads))
    {
        set->Shutdown(std::chrono::milliseconds(0));
        LOG_ERROR("sql.driver", "Could not open database connection pool {} on {}: error {}", _name, _info->ToLogString(), error);
        return error;
    }
    set->Start();
    {
        std::lock_guard<std::mutex> lock(_currentMutex);
        _current = set;
        _statements = set->GetStatements();
    }
    LOG_INFO("sql.driver", "Opened database connection pool {}: {} async, {} sync", _name, set->GetAsyncConnectionCount(), set->GetSyncConnectionCount());
    return 0;
}

void DatabaseWorkerPoolBase::Close(std::chrono::milliseconds drainTimeout)
{
    std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
    std::shared_ptr<DatabaseConnectionSet> retired;
    {
        std::lock_guard<std::mutex> lock(_currentMutex);
        retired = std::move(_current);
    }
    if (!retired)
        return;
    retired->Shutdown(drainTimeout);
    LOG_INFO("sql.driver", "Closed database connection pool {}", _name);
}

bool DatabaseWorkerPoolBase::IsOpen() const
{
    std::lock_guard<std::mutex> lock(_currentMutex);
    return _current != nullptr;
}

std::shared_ptr<DatabaseConnectionSet> DatabaseWorkerPoolBase::GetCurrent() const
{
    std::lock_guard<std::mutex> lock(_currentMutex);
    return _current;
}

bool DatabaseWorkerPoolBase::Enqueue(std::unique_ptr<SQLOperation> operation)
{
    std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent();
    if (set && set->Enqueue(operation))
        return true;
    if (!set)
        LOG_ERROR("sql.sql", "Database pool {} is not open, so queued work was refused", _name);
    else if (set->GetAsyncConnectionCount() == 0)
        LOG_ERROR("sql.sql", "Database pool {} has no async connections, so queued work was refused", _name);
    if (operation)
        operation->Cancel();
    return false;
}

void DatabaseWorkerPoolBase::Execute(std::string sql)
{
    Enqueue(std::make_unique<AdhocStatementTask>(std::move(sql), false));
}

QueryCallback DatabaseWorkerPoolBase::AsyncQuery(std::string sql)
{
    auto task = std::make_unique<AdhocStatementTask>(std::move(sql), true);
    std::future<QueryResult> result = task->GetFuture();
    Enqueue(std::move(task));
    return QueryCallback(std::move(result));
}

std::optional<std::size_t> DatabaseWorkerPoolBase::GetParameterCount(uint32 index) const
{
    std::shared_ptr<PoolStatementTable const> table;
    {
        std::lock_guard<std::mutex> lock(_currentMutex);
        table = _statements;
    }
    if (!table)
    {
        LOG_ERROR("sql.sql", "Database pool {} has never been opened, so statement {} has no parameters yet", _name, index);
        return std::nullopt;
    }
    auto const found = table->find(index);
    if (found == table->end())
    {
        LOG_ERROR("sql.sql", "Database pool {} has no prepared statement {}", _name, index);
        return std::nullopt;
    }
    return found->second.ParameterCount;
}

bool DatabaseWorkerPoolBase::CheckStatement(PreparedStatementBase const* statement, bool sync, std::string_view call) const
{
    if (!statement)
    {
        LOG_ERROR("sql.sql", "Database pool {} refused {} with an empty statement", _name, call);
        return false;
    }
    std::shared_ptr<PoolStatementTable const> table;
    {
        std::lock_guard<std::mutex> lock(_currentMutex);
        table = _statements;
    }
    auto const found = table ? table->find(statement->GetIndex()) : PoolStatementTable::const_iterator();
    if (!table || found == table->end())
    {
        LOG_ERROR("sql.sql", "Database pool {} refused {}: statement {} is not prepared", _name, call, statement->GetIndex());
        return false;
    }
    if (sync ? !found->second.OnSync : !found->second.OnAsync)
    {
        LOG_ERROR("sql.sql", "Database pool {} refused {}: statement {} is prepared only for {} connections", _name, call, found->second.Name, sync ? "async" : "sync");
        return false;
    }
    return true;
}

void DatabaseWorkerPoolBase::ExecuteStatement(std::unique_ptr<PreparedStatementBase> statement)
{
    if (!CheckStatement(statement.get(), false, "Execute"))
        return;
    Enqueue(std::make_unique<PreparedStatementTask>(std::move(statement), false));
}

QueryCallback DatabaseWorkerPoolBase::AsyncQueryStatement(std::unique_ptr<PreparedStatementBase> statement)
{
    bool const valid = CheckStatement(statement.get(), false, "AsyncQuery");
    auto task = std::make_unique<PreparedStatementTask>(std::move(statement), true);
    std::future<PreparedQueryResult> result = task->GetFuture();
    if (valid)
        Enqueue(std::move(task));
    else
        task->Cancel();
    return QueryCallback(std::move(result));
}

std::future<void> DatabaseWorkerPoolBase::DelayQueryHolderBase(std::shared_ptr<SQLQueryHolderBase> holder)
{
    auto task = std::make_unique<QueryHolderTask>(holder);
    std::future<void> done = task->GetFuture();
    if (!holder)
    {
        LOG_ERROR("sql.sql", "Database pool {} refused an empty query holder", _name);
        task->Cancel();
    }
    else
        Enqueue(std::move(task));
    return done;
}

bool DatabaseWorkerPoolBase::DirectExecute(std::string_view sql)
{
    SyncLease connection(GetCurrent());
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so DirectExecute was refused", _name);
        return false;
    }
    return connection->Execute(sql);
}

QueryResult DatabaseWorkerPoolBase::Query(std::string_view sql)
{
    SyncLease connection(GetCurrent());
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so Query was refused", _name);
        return nullptr;
    }
    return connection->Query(sql);
}

bool DatabaseWorkerPoolBase::DirectExecuteStatement(PreparedStatementBase const* statement)
{
    if (!CheckStatement(statement, true, "DirectExecute"))
        return false;
    SyncLease connection(GetCurrent());
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so statement {} was refused", _name, statement->GetIndex());
        return false;
    }
    return connection->Execute(*statement);
}

PreparedQueryResult DatabaseWorkerPoolBase::QueryStatement(PreparedStatementBase const* statement)
{
    if (!CheckStatement(statement, true, "Query"))
        return nullptr;
    SyncLease connection(GetCurrent());
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so statement {} was refused", _name, statement->GetIndex());
        return nullptr;
    }
    return connection->Query(*statement);
}

void DatabaseWorkerPoolBase::KeepAlive()
{
    if (std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent())
        set->PingIdleSync();
}

std::string DatabaseWorkerPoolBase::Escape(std::string_view text)
{
    SyncLease connection(GetCurrent());
    if (!connection)
    {
        MySQLConnection offline(MySQLConnectionInfo{});
        return offline.Escape(text);
    }
    return connection->Escape(text);
}

std::size_t DatabaseWorkerPoolBase::GetQueueSize() const
{
    std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent();
    return set ? set->GetQueueSize() : 0;
}

std::size_t DatabaseWorkerPoolBase::GetAsyncConnectionCount() const
{
    std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent();
    return set ? set->GetAsyncConnectionCount() : 0;
}

std::size_t DatabaseWorkerPoolBase::GetSyncConnectionCount() const
{
    std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent();
    return set ? set->GetSyncConnectionCount() : 0;
}

uint64 DatabaseWorkerPoolBase::GetReconnectCount() const
{
    std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent();
    return set ? set->GetReconnectCount() : 0;
}

uint64 DatabaseWorkerPoolBase::GetConcurrentUseCount() const
{
    std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent();
    return set ? set->GetConcurrentUseCount() : 0;
}
