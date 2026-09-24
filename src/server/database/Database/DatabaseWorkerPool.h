/*
 * Project Ambrose by Imjustchico
 * A named connection pool for one database that serves every call from the current connection generation, so opening, closing and live reconfiguring never block callers, with typed statements, including executes that report the rows they changed and several queries read from one consistent snapshot, holders and transactions whose async forms can signal a handler once their result is ready, and the current generation's use for the panel.
 */

#ifndef AMBROSE_DATABASEWORKERPOOL_H
#define AMBROSE_DATABASEWORKERPOOL_H

#include "DatabaseConnectionSet.h"
#include "PreparedStatement.h"
#include "QueryCallback.h"
#include "QueryHolder.h"
#include "Transaction.h"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class DatabaseWorkerPoolBase
{
public:
    using ConnectionFactory = DatabaseConnectionSet::ConnectionFactory;

    static constexpr uint32 MaxThreads = 64;
    static constexpr std::chrono::seconds DefaultDrainTimeout{ 30 };

    DatabaseWorkerPoolBase(std::string name, ConnectionFactory factory);
    virtual ~DatabaseWorkerPoolBase();

    DatabaseWorkerPoolBase(DatabaseWorkerPoolBase const&) = delete;
    DatabaseWorkerPoolBase& operator=(DatabaseWorkerPoolBase const&) = delete;

    bool SetConnectionInfo(std::string_view infoString, uint32 asyncThreads, uint32 syncThreads);
    void SetConnectionSettings(MySQLConnectionSettings const& settings);
    uint32 Open();
    uint32 Reconfigure(std::string_view infoString, uint32 asyncThreads, uint32 syncThreads);
    void Close(std::chrono::milliseconds drainTimeout = DefaultDrainTimeout);
    bool IsOpen() const;

    void Execute(std::string sql);
    QueryCallback AsyncQuery(std::string sql, SQLOperation::CompletionHandler onCompleted = {});
    bool DirectExecute(std::string_view sql);
    QueryResult Query(std::string_view sql);
    bool TryQuery(std::string_view sql, QueryResult& result);

    void KeepAlive();
    void SetKeepAliveInterval(std::chrono::milliseconds interval) noexcept { _keepAliveMs = interval.count(); }
    std::string Escape(std::string_view text);

    std::string const& GetName() const noexcept { return _name; }
    std::size_t GetQueueSize() const;
    std::size_t GetAsyncConnectionCount() const;
    std::size_t GetSyncConnectionCount() const;
    uint64 GetReconnectCount() const;
    uint64 GetConcurrentUseCount() const;
    DatabasePoolUse GetUse() const;

protected:
    std::optional<std::size_t> GetParameterCount(uint32 index) const;
    void ExecuteStatement(std::unique_ptr<PreparedStatementBase> statement);
    QueryCallback AsyncQueryStatement(std::unique_ptr<PreparedStatementBase> statement, SQLOperation::CompletionHandler onCompleted = {});
    bool DirectExecuteStatement(PreparedStatementBase const* statement);
    std::optional<uint64> DirectExecuteCountedStatement(PreparedStatementBase const* statement);
    PreparedQueryResult QueryStatement(PreparedStatementBase const* statement, bool* failed = nullptr);
    bool QuerySnapshotStatements(std::span<PreparedStatementBase const* const> statements, std::vector<PreparedQueryResult>& results);
    SQLQueryHolderCallback DelayQueryHolderBase(std::shared_ptr<SQLQueryHolderBase> holder, SQLOperation::CompletionHandler onCompleted = {});
    void CommitTransactionBase(std::shared_ptr<TransactionBase> transaction);
    TransactionCallback AsyncCommitTransactionBase(std::shared_ptr<TransactionBase> transaction, SQLOperation::CompletionHandler onCompleted = {});
    bool DirectCommitTransactionBase(std::shared_ptr<TransactionBase> const& transaction);

private:
    std::shared_ptr<DatabaseConnectionSet> GetCurrent() const;
    bool Enqueue(std::unique_ptr<SQLOperation> operation);
    bool CheckStatement(PreparedStatementBase const* statement, bool sync, std::string_view call) const;
    bool CheckTransaction(TransactionBase const* transaction, bool sync) const;
    bool LockTransaction(TransactionBase* transaction) const;
    void WaitForRetired(std::chrono::milliseconds drainTimeout);

    std::string _name;
    ConnectionFactory _factory;
    std::mutex _lifecycleMutex;
    std::optional<MySQLConnectionInfo> _info;
    MySQLConnectionSettings _settings;
    uint32 _asyncThreads = 1;
    uint32 _syncThreads = 1;
    std::atomic<int64> _keepAliveMs{ 30 * 60 * 1000 };
    mutable std::mutex _currentMutex;
    std::shared_ptr<DatabaseConnectionSet> _current;
    std::shared_ptr<PoolStatementTable const> _statements;
    struct RetiredGeneration
    {
        std::shared_ptr<DatabaseConnectionSet> Set;
        std::future<void> Done;
    };

    std::vector<RetiredGeneration> _retired;
};

template<typename ConnectionType>
class DatabaseWorkerPool : public DatabaseWorkerPoolBase
{
public:
    using Statement = PreparedStatement<ConnectionType>;
    using StatementId = typename ConnectionType::Statements;

    explicit DatabaseWorkerPool(std::string name)
        : DatabaseWorkerPoolBase(std::move(name), [](MySQLConnectionInfo const& info, MySQLConnectionSettings const& settings) { return std::make_unique<ConnectionType>(info, settings); })
    {
    }

    using DatabaseWorkerPoolBase::AsyncQuery;
    using DatabaseWorkerPoolBase::DirectExecute;
    using DatabaseWorkerPoolBase::Execute;
    using DatabaseWorkerPoolBase::Query;
    using DatabaseWorkerPoolBase::TryQuery;

    std::unique_ptr<Statement> GetPreparedStatement(StatementId index) const
    {
        std::optional<std::size_t> const count = GetParameterCount(static_cast<uint32>(index));
        return count ? std::make_unique<Statement>(static_cast<uint32>(index), *count) : nullptr;
    }

    void Execute(std::unique_ptr<Statement> statement) { ExecuteStatement(std::move(statement)); }
    QueryCallback AsyncQuery(std::unique_ptr<Statement> statement, SQLOperation::CompletionHandler onCompleted = {}) { return AsyncQueryStatement(std::move(statement), std::move(onCompleted)); }
    bool DirectExecute(Statement const& statement) { return DirectExecuteStatement(&statement); }
    std::optional<uint64> DirectExecuteCounted(Statement const& statement) { return DirectExecuteCountedStatement(&statement); }
    PreparedQueryResult Query(Statement const& statement) { return QueryStatement(&statement); }
    bool TryQuery(Statement const& statement, PreparedQueryResult& result)
    {
        bool failed = false;
        result = QueryStatement(&statement, &failed);
        return !failed;
    }
    bool QuerySnapshot(std::initializer_list<Statement const*> statements, std::vector<PreparedQueryResult>& results)
    {
        std::vector<PreparedStatementBase const*> const list(statements.begin(), statements.end());
        return QuerySnapshotStatements(list, results);
    }
    SQLQueryHolderCallback DelayQueryHolder(std::shared_ptr<SQLQueryHolder<ConnectionType>> holder, SQLOperation::CompletionHandler onCompleted = {}) { return DelayQueryHolderBase(std::move(holder), std::move(onCompleted)); }
    std::shared_ptr<Transaction<ConnectionType>> BeginTransaction() const { return std::make_shared<Transaction<ConnectionType>>(); }
    void CommitTransaction(std::shared_ptr<Transaction<ConnectionType>> transaction) { CommitTransactionBase(std::move(transaction)); }
    TransactionCallback AsyncCommitTransaction(std::shared_ptr<Transaction<ConnectionType>> transaction, SQLOperation::CompletionHandler onCompleted = {}) { return AsyncCommitTransactionBase(std::move(transaction), std::move(onCompleted)); }
    bool DirectCommitTransaction(std::shared_ptr<Transaction<ConnectionType>> const& transaction) { return DirectCommitTransactionBase(transaction); }
};

#endif
