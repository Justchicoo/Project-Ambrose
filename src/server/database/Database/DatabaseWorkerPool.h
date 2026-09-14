/*
 * Project Ambrose by Imjustchico
 * A named connection pool for one database that serves every call from the current connection generation, so opening, closing and swapping never block callers, with typed statement and holder access.
 */

#ifndef AMBROSE_DATABASEWORKERPOOL_H
#define AMBROSE_DATABASEWORKERPOOL_H

#include "DatabaseConnectionSet.h"
#include "PreparedStatement.h"
#include "QueryCallback.h"
#include "QueryHolder.h"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

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
    void Close(std::chrono::milliseconds drainTimeout = DefaultDrainTimeout);
    bool IsOpen() const;

    void Execute(std::string sql);
    QueryCallback AsyncQuery(std::string sql);
    bool DirectExecute(std::string_view sql);
    QueryResult Query(std::string_view sql);

    void KeepAlive();
    void SetKeepAliveInterval(std::chrono::milliseconds interval) noexcept { _keepAliveMs = interval.count(); }
    std::string Escape(std::string_view text);

    std::string const& GetName() const noexcept { return _name; }
    std::size_t GetQueueSize() const;
    std::size_t GetAsyncConnectionCount() const;
    std::size_t GetSyncConnectionCount() const;
    uint64 GetReconnectCount() const;
    uint64 GetConcurrentUseCount() const;

protected:
    std::optional<std::size_t> GetParameterCount(uint32 index) const;
    void ExecuteStatement(std::unique_ptr<PreparedStatementBase> statement);
    QueryCallback AsyncQueryStatement(std::unique_ptr<PreparedStatementBase> statement);
    bool DirectExecuteStatement(PreparedStatementBase const* statement);
    PreparedQueryResult QueryStatement(PreparedStatementBase const* statement);
    std::future<void> DelayQueryHolderBase(std::shared_ptr<SQLQueryHolderBase> holder);

private:
    std::shared_ptr<DatabaseConnectionSet> GetCurrent() const;
    bool Enqueue(std::unique_ptr<SQLOperation> operation);
    bool CheckStatement(PreparedStatementBase const* statement, bool sync, std::string_view call) const;

    std::string _name;
    ConnectionFactory _factory;
    std::mutex _lifecycleMutex;
    std::optional<MySQLConnectionInfo> _info;
    MySQLConnectionSettings _settings;
    uint32 _asyncThreads = 1;
    uint32 _syncThreads = 1;
    std::atomic<int64> _keepAliveMs{ 30000 };
    mutable std::mutex _currentMutex;
    std::shared_ptr<DatabaseConnectionSet> _current;
    std::shared_ptr<PoolStatementTable const> _statements;
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

    std::unique_ptr<Statement> GetPreparedStatement(StatementId index) const
    {
        std::optional<std::size_t> const count = GetParameterCount(static_cast<uint32>(index));
        return count ? std::make_unique<Statement>(static_cast<uint32>(index), *count) : nullptr;
    }

    void Execute(std::unique_ptr<Statement> statement) { ExecuteStatement(std::move(statement)); }
    QueryCallback AsyncQuery(std::unique_ptr<Statement> statement) { return AsyncQueryStatement(std::move(statement)); }
    bool DirectExecute(Statement const& statement) { return DirectExecuteStatement(&statement); }
    PreparedQueryResult Query(Statement const& statement) { return QueryStatement(&statement); }
    std::future<void> DelayQueryHolder(std::shared_ptr<SQLQueryHolder<ConnectionType>> holder) { return DelayQueryHolderBase(std::move(holder)); }
};

#endif
