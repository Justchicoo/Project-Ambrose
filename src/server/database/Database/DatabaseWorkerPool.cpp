/*
 * Project Ambrose by Imjustchico
 * Builds and validates a new connection generation before publishing it, swaps generations live and drains retired ones in the background, takes a snapshot for every call, reads several queries on one leased connection inside a read-only repeatable-read transaction so they see one database state, and refuses empty or wrong-side statements and transactions.
 */

#include "DatabaseWorkerPool.h"
#include "AdhocStatement.h"
#include "Log.h"
#include "QueryHolder.h"
#include "QueryResult.h"

#include <algorithm>
#include <functional>

namespace
{
    class SyncLease
    {
    public:
        explicit SyncLease(std::function<std::shared_ptr<DatabaseConnectionSet>()> const& current)
        {
            for (int attempt = 0; attempt < 4; ++attempt)
            {
                std::shared_ptr<DatabaseConnectionSet> set = current();
                if (!set)
                    return;
                if (MySQLConnection* const connection = set->AcquireSync())
                {
                    _set = std::move(set);
                    _connection = connection;
                    return;
                }
                if (current() == set)
                    return;
            }
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
        MySQLConnection* _connection = nullptr;
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
    WaitForRetired(std::chrono::milliseconds(0));
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
    if (retired)
        retired->Shutdown(drainTimeout);
    WaitForRetired(drainTimeout);
    if (retired)
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
    std::shared_ptr<DatabaseConnectionSet> set = GetCurrent();
    for (int attempt = 0; attempt < 4 && set; ++attempt)
    {
        if (set->Enqueue(operation))
            return true;
        std::shared_ptr<DatabaseConnectionSet> next = GetCurrent();
        if (next == set)
            break;
        set = std::move(next);
    }
    if (!set)
        LOG_ERROR("sql.sql", "Database pool {} is not open, so queued work was cancelled", _name);
    else if (set->GetAsyncConnectionCount() == 0)
        LOG_ERROR("sql.sql", "Database pool {} has no async connections, so queued work was cancelled", _name);
    else
        LOG_ERROR("sql.sql", "Database pool {} is closing, so queued work was cancelled", _name);
    if (operation)
        operation->Cancel();
    return false;
}

void DatabaseWorkerPoolBase::Execute(std::string sql)
{
    Enqueue(std::make_unique<AdhocStatementTask>(std::move(sql), false));
}

QueryCallback DatabaseWorkerPoolBase::AsyncQuery(std::string sql, SQLOperation::CompletionHandler onCompleted)
{
    auto task = std::make_unique<AdhocStatementTask>(std::move(sql), true);
    task->SetCompletionHandler(std::move(onCompleted));
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

QueryCallback DatabaseWorkerPoolBase::AsyncQueryStatement(std::unique_ptr<PreparedStatementBase> statement, SQLOperation::CompletionHandler onCompleted)
{
    bool const valid = CheckStatement(statement.get(), false, "AsyncQuery");
    auto task = std::make_unique<PreparedStatementTask>(std::move(statement), true);
    task->SetCompletionHandler(std::move(onCompleted));
    std::future<PreparedQueryResult> result = task->GetFuture();
    if (valid)
        Enqueue(std::move(task));
    else
        task->Cancel();
    return QueryCallback(std::move(result));
}

SQLQueryHolderCallback DatabaseWorkerPoolBase::DelayQueryHolderBase(std::shared_ptr<SQLQueryHolderBase> holder, SQLOperation::CompletionHandler onCompleted)
{
    auto task = std::make_unique<QueryHolderTask>(holder);
    task->SetCompletionHandler(std::move(onCompleted));
    std::future<void> done = task->GetFuture();
    if (!holder)
    {
        LOG_ERROR("sql.sql", "Database pool {} refused an empty query holder", _name);
        task->Cancel();
    }
    else
        Enqueue(std::move(task));
    return SQLQueryHolderCallback(std::move(holder), std::move(done));
}

bool DatabaseWorkerPoolBase::DirectExecute(std::string_view sql)
{
    SyncLease connection([this] { return GetCurrent(); });
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so DirectExecute was refused", _name);
        return false;
    }
    return connection->Execute(sql);
}

QueryResult DatabaseWorkerPoolBase::Query(std::string_view sql)
{
    QueryResult result;
    TryQuery(sql, result);
    return result;
}

bool DatabaseWorkerPoolBase::TryQuery(std::string_view sql, QueryResult& result)
{
    result = nullptr;
    SyncLease connection([this] { return GetCurrent(); });
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so Query was refused", _name);
        return false;
    }
    result = connection->Query(sql);
    return result || connection->GetLastErrorCode() == 0;
}

bool DatabaseWorkerPoolBase::DirectExecuteStatement(PreparedStatementBase const* statement)
{
    if (!CheckStatement(statement, true, "DirectExecute"))
        return false;
    SyncLease connection([this] { return GetCurrent(); });
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so statement {} was refused", _name, statement->GetIndex());
        return false;
    }
    return connection->Execute(*statement);
}

std::optional<uint64> DatabaseWorkerPoolBase::DirectExecuteCountedStatement(PreparedStatementBase const* statement)
{
    if (!CheckStatement(statement, true, "DirectExecute"))
        return std::nullopt;
    SyncLease connection([this] { return GetCurrent(); });
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so statement {} was refused", _name, statement->GetIndex());
        return std::nullopt;
    }
    if (!connection->Execute(*statement))
        return std::nullopt;
    return connection->GetLastAffectedRows();
}

PreparedQueryResult DatabaseWorkerPoolBase::QueryStatement(PreparedStatementBase const* statement, bool* failed)
{
    if (failed)
        *failed = true;
    if (!CheckStatement(statement, true, "Query"))
        return nullptr;
    SyncLease connection([this] { return GetCurrent(); });
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so statement {} was refused", _name, statement->GetIndex());
        return nullptr;
    }
    PreparedQueryResult result = connection->Query(*statement);
    if (failed)
        *failed = !result && connection->GetLastErrorCode() != 0;
    return result;
}

bool DatabaseWorkerPoolBase::QuerySnapshotStatements(std::span<PreparedStatementBase const* const> statements, std::vector<PreparedQueryResult>& results)
{
    results.clear();
    for (PreparedStatementBase const* statement : statements)
        if (!CheckStatement(statement, true, "QuerySnapshot"))
            return false;
    SyncLease connection([this] { return GetCurrent(); });
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so a snapshot read of {} statement(s) was refused", _name, statements.size());
        return false;
    }
    if (!connection->Execute("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ") || !connection->Execute("START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY"))
        return false;
    for (PreparedStatementBase const* statement : statements)
    {
        PreparedQueryResult result = connection->Query(*statement);
        if (!result && connection->GetLastErrorCode() != 0)
        {
            uint32 const code = connection->GetLastErrorCode();
            std::string const text = connection->GetLastErrorText();
            connection->Execute("ROLLBACK");
            LOG_ERROR("sql.sql", "A snapshot read on pool {} failed at statement {}: [{}] {}", _name, statement->GetIndex(), code, text);
            results.clear();
            return false;
        }
        results.push_back(std::move(result));
    }
    connection->Execute("COMMIT");
    return true;
}

void DatabaseWorkerPoolBase::KeepAlive()
{
    if (std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent())
        set->PingIdleSync();
}

std::string DatabaseWorkerPoolBase::Escape(std::string_view text)
{
    SyncLease connection([this] { return GetCurrent(); });
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

DatabasePoolUse DatabaseWorkerPoolBase::GetUse() const
{
    std::shared_ptr<DatabaseConnectionSet> const set = GetCurrent();
    return set ? set->GetUse() : DatabasePoolUse{};
}

void DatabaseWorkerPoolBase::WaitForRetired(std::chrono::milliseconds drainTimeout)
{
    std::vector<RetiredGeneration> retired;
    {
        std::lock_guard<std::mutex> lock(_currentMutex);
        retired = std::move(_retired);
        _retired.clear();
    }
    for (RetiredGeneration& generation : retired)
    {
        generation.Set->ShortenDrain(drainTimeout);
        if (generation.Done.valid())
            generation.Done.wait();
    }
}

uint32 DatabaseWorkerPoolBase::Reconfigure(std::string_view infoString, uint32 asyncThreads, uint32 syncThreads)
{
    std::string error;
    std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(infoString, &error);
    if (!info)
    {
        LOG_ERROR("sql.driver", "Database pool {} keeps its connections: the new connection string is invalid: {}", _name, error);
        return 2000;
    }
    uint32 const async = std::min(asyncThreads, MaxThreads);
    uint32 const sync = std::clamp<uint32>(syncThreads, 1, MaxThreads);
    if (async != asyncThreads || sync != syncThreads)
        LOG_WARN("sql.driver", "Database pool {} uses {} async and {} sync connection(s) instead of {} and {} (sync 1-{}, async 0-{})", _name, async, sync, asyncThreads, syncThreads, MaxThreads, MaxThreads);

    std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
    if (!IsOpen())
    {
        _info = std::move(info);
        _asyncThreads = async;
        _syncThreads = sync;
        return 0;
    }

    auto set = std::make_shared<DatabaseConnectionSet>(_name, _keepAliveMs);
    if (uint32 const openError = set->Open(_factory, *info, _settings, async, sync))
    {
        set->Shutdown(std::chrono::milliseconds(0));
        LOG_ERROR("sql.driver", "Database pool {} keeps its current connections: {} could not be opened (error {})", _name, info->ToLogString(), openError);
        return openError;
    }
    set->Start();
    std::shared_ptr<DatabaseConnectionSet> previous;
    {
        std::lock_guard<std::mutex> lock(_currentMutex);
        previous = std::move(_current);
        _current = set;
        _statements = set->GetStatements();
        std::erase_if(_retired, [](RetiredGeneration const& generation) { return generation.Done.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });
        if (previous)
            _retired.push_back(RetiredGeneration{ previous, std::async(std::launch::async, [previous] { previous->Shutdown(DefaultDrainTimeout); }) });
    }
    _info = std::move(info);
    _asyncThreads = async;
    _syncThreads = sync;
    LOG_INFO("sql.driver", "Reconfigured database connection pool {}: {} async, {} sync on {}", _name, set->GetAsyncConnectionCount(), set->GetSyncConnectionCount(), _info->ToLogString());
    return 0;
}

bool DatabaseWorkerPoolBase::CheckTransaction(TransactionBase const* transaction, bool sync) const
{
    if (!transaction)
    {
        LOG_ERROR("sql.sql", "Database pool {} refused an empty transaction", _name);
        return false;
    }
    if (!transaction->IsValid())
    {
        LOG_ERROR("sql.sql", "Database pool {} refused a transaction that holds an empty statement or was changed after submission", _name);
        return false;
    }
    for (TransactionBase::Entry const& entry : transaction->GetEntries())
        if (std::holds_alternative<std::unique_ptr<PreparedStatementBase>>(entry) && !CheckStatement(std::get<std::unique_ptr<PreparedStatementBase>>(entry).get(), sync, "a transaction"))
            return false;
    return true;
}

void DatabaseWorkerPoolBase::CommitTransactionBase(std::shared_ptr<TransactionBase> transaction)
{
    if (!LockTransaction(transaction.get()) || !CheckTransaction(transaction.get(), false))
        return;
    Enqueue(std::make_unique<TransactionTask>(std::move(transaction)));
}

TransactionCallback DatabaseWorkerPoolBase::AsyncCommitTransactionBase(std::shared_ptr<TransactionBase> transaction, SQLOperation::CompletionHandler onCompleted)
{
    bool const valid = LockTransaction(transaction.get()) && CheckTransaction(transaction.get(), false);
    auto task = std::make_unique<TransactionTask>(std::move(transaction));
    task->SetCompletionHandler(std::move(onCompleted));
    std::future<bool> result = task->GetFuture();
    if (valid)
        Enqueue(std::move(task));
    else
        task->Cancel();
    return TransactionCallback(std::move(result));
}

bool DatabaseWorkerPoolBase::DirectCommitTransactionBase(std::shared_ptr<TransactionBase> const& transaction)
{
    if (!LockTransaction(transaction.get()) || !CheckTransaction(transaction.get(), true))
        return false;
    SyncLease connection([this] { return GetCurrent(); });
    if (!connection)
    {
        LOG_ERROR("sql.sql", "Database pool {} is not open, so a transaction was refused", _name);
        return false;
    }
    try
    {
        return TransactionTask::Commit(*connection.operator->(), *transaction);
    }
    catch (std::exception const& exception)
    {
        LOG_ERROR("sql.sql", "A transaction on database pool {} threw: {}", _name, exception.what());
        return false;
    }
}

bool DatabaseWorkerPoolBase::LockTransaction(TransactionBase* transaction) const
{
    if (transaction && !transaction->Lock())
    {
        LOG_ERROR("sql.sql", "Database pool {} refused a transaction that was already submitted", _name);
        return false;
    }
    return true;
}
