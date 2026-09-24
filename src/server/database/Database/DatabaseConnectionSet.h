/*
 * Project Ambrose by Imjustchico
 * One generation of a pool's connections: sync connections with exclusive leases, async workers on a shared queue, a keepalive pinger, and the statement table, shut down as a unit, whose use is read at once: connections running a statement, sync leases, callers waiting for one, queued work and reconnects.
 */

#ifndef AMBROSE_DATABASECONNECTIONSET_H
#define AMBROSE_DATABASECONNECTIONSET_H

#include "DatabaseWorker.h"
#include "MySQLConnection.h"

#include <atomic>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct PoolStatementInfo
{
    std::string Name;
    std::size_t ParameterCount = 0;
    bool OnSync = false;
    bool OnAsync = false;
};

using PoolStatementTable = std::unordered_map<uint32, PoolStatementInfo>;

struct DatabasePoolUse
{
    std::size_t AsyncConnections = 0;
    std::size_t SyncConnections = 0;
    std::size_t AsyncActive = 0;
    std::size_t SyncLeased = 0;
    std::size_t SyncWaiting = 0;
    std::size_t Queued = 0;
    uint64 Reconnects = 0;
};

class DatabaseConnectionSet
{
public:
    using ConnectionFactory = std::function<std::unique_ptr<MySQLConnection>(MySQLConnectionInfo const&, MySQLConnectionSettings const&)>;

    static constexpr uint64 MinimumMariaDBVersion = 100600;
    static constexpr uint64 MinimumMySQLVersion = 80000;
    static constexpr uint64 MinimumClientVersion = 30300;

    DatabaseConnectionSet(std::string poolName, std::atomic<int64> const& keepAliveMs);
    ~DatabaseConnectionSet();

    DatabaseConnectionSet(DatabaseConnectionSet const&) = delete;
    DatabaseConnectionSet& operator=(DatabaseConnectionSet const&) = delete;

    uint32 Open(ConnectionFactory const& factory, MySQLConnectionInfo const& info, MySQLConnectionSettings const& settings, uint32 asyncThreads, uint32 syncThreads);
    void Start();
    void Shutdown(std::chrono::milliseconds drainTimeout);
    void ShortenDrain(std::chrono::milliseconds remaining) noexcept;

    bool Enqueue(std::unique_ptr<SQLOperation>& operation);
    MySQLConnection* AcquireSync();
    MySQLConnection* TryAcquireSync();
    void ReleaseSync(MySQLConnection* connection);
    void PingIdleSync();

    std::shared_ptr<PoolStatementTable const> GetStatements() const noexcept { return _statements; }
    std::size_t GetAsyncConnectionCount() const noexcept { return _asyncConnections.size(); }
    std::size_t GetSyncConnectionCount() const noexcept { return _syncConnections.size(); }
    std::size_t GetQueueSize() const { return _queue.Size(); }
    uint64 GetReconnectCount() const;
    uint64 GetConcurrentUseCount() const;
    DatabasePoolUse GetUse() const;

private:
    uint32 OpenConnection(ConnectionFactory const& factory, MySQLConnectionInfo const& info, MySQLConnectionSettings settings, ConnectionFlags flags, std::vector<std::unique_ptr<MySQLConnection>>& into);
    MySQLConnection* TakeFreeLocked();
    void RunPinger();

    std::string _poolName;
    std::atomic<int64> const& _keepAliveMs;
    std::vector<std::unique_ptr<MySQLConnection>> _asyncConnections;
    std::vector<std::unique_ptr<MySQLConnection>> _syncConnections;
    std::shared_ptr<PoolStatementTable const> _statements;
    DatabaseWorker::Queue _queue;
    std::vector<std::unique_ptr<DatabaseWorker>> _workers;

    mutable std::mutex _syncMutex;
    std::condition_variable _syncCondition;
    std::vector<bool> _syncBusy;
    std::size_t _nextSync = 0;
    std::size_t _syncWaiters = 0;
    bool _syncClosing = false;

    std::mutex _pingerMutex;
    std::condition_variable _pingerCondition;
    bool _pingerStop = false;
    std::thread _pinger;

    std::atomic<bool> _shutDown{ false };
    std::atomic<bool> _accepting{ false };
    std::atomic<int64> _drainLimitNs{ INT64_MAX };
};

#endif
