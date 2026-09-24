/*
 * Project Ambrose by Imjustchico
 * Opens a generation's connections with client and server version checks and prepared statements, leases sync connections fairly, pings idle ones, counts what is in use, and shuts down by draining the queue until a deadline and then cancelling.
 */

#include "DatabaseConnectionSet.h"
#include "Log.h"
#include "ThreadName.h"

#include <errmsg.h>
#include <mysql.h>

#include <fmt/format.h>

#include <algorithm>

DatabaseConnectionSet::DatabaseConnectionSet(std::string poolName, std::atomic<int64> const& keepAliveMs) : _poolName(std::move(poolName)), _keepAliveMs(keepAliveMs)
{
}

DatabaseConnectionSet::~DatabaseConnectionSet()
{
    Shutdown(std::chrono::milliseconds(0));
}

uint32 DatabaseConnectionSet::OpenConnection(ConnectionFactory const& factory, MySQLConnectionInfo const& info, MySQLConnectionSettings settings, ConnectionFlags flags, std::vector<std::unique_ptr<MySQLConnection>>& into)
{
    settings.Flags = flags;
    std::unique_ptr<MySQLConnection> connection = factory(info, settings);
    if (uint32 const error = connection->Open())
        return error;

    uint64 const version = connection->GetServerVersion();
    uint64 const minimum = connection->IsMariaDB() ? MinimumMariaDBVersion : MinimumMySQLVersion;
    if (version < minimum)
    {
        LOG_ERROR("sql.driver", "Database pool {} needs {} {}.{} or newer, but the server runs {}", _poolName, connection->IsMariaDB() ? "MariaDB" : "MySQL", minimum / 10000, (minimum / 100) % 100, connection->GetServerInfo());
        return CR_VERSION_ERROR;
    }
    if (!connection->PrepareStatements())
    {
        LOG_ERROR("sql.driver", "Database pool {} could not prepare its statements", _poolName);
        return connection->GetLastErrorCode() ? connection->GetLastErrorCode() : CR_UNKNOWN_ERROR;
    }
    into.push_back(std::move(connection));
    return 0;
}

uint32 DatabaseConnectionSet::Open(ConnectionFactory const& factory, MySQLConnectionInfo const& info, MySQLConnectionSettings const& settings, uint32 asyncThreads, uint32 syncThreads)
{
    if (mysql_get_client_version() < MinimumClientVersion)
    {
        LOG_ERROR("sql.driver", "Database pool {} needs MariaDB Connector/C 3.3 or newer, but {} is loaded", _poolName, mysql_get_client_info());
        return CR_VERSION_ERROR;
    }
    for (uint32 i = 0; i < syncThreads; ++i)
        if (uint32 const error = OpenConnection(factory, info, settings, ConnectionFlags::Sync, _syncConnections))
            return error;
    for (uint32 i = 0; i < asyncThreads; ++i)
        if (uint32 const error = OpenConnection(factory, info, settings, ConnectionFlags::Async, _asyncConnections))
            return error;

    auto table = std::make_shared<PoolStatementTable>();
    for (std::unique_ptr<MySQLConnection> const& connection : _syncConnections)
        for (PreparedStatementInfo const& statement : connection->GetPreparedStatementInfos())
        {
            PoolStatementInfo& entry = (*table)[statement.Index];
            entry.Name = statement.Name;
            entry.ParameterCount = statement.ParameterCount;
            entry.OnSync = true;
        }
    for (std::unique_ptr<MySQLConnection> const& connection : _asyncConnections)
        for (PreparedStatementInfo const& statement : connection->GetPreparedStatementInfos())
        {
            PoolStatementInfo& entry = (*table)[statement.Index];
            entry.Name = statement.Name;
            entry.ParameterCount = statement.ParameterCount;
            entry.OnAsync = true;
        }
    _statements = std::move(table);
    _syncBusy.assign(_syncConnections.size(), false);
    return 0;
}

void DatabaseConnectionSet::Start()
{
    for (std::size_t i = 0; i < _asyncConnections.size(); ++i)
        _workers.push_back(std::make_unique<DatabaseWorker>(_queue, *_asyncConnections[i], fmt::format("DB {} {}", _poolName, i + 1), _keepAliveMs));
    if (!_syncConnections.empty())
        _pinger = std::thread([this] { RunPinger(); });
    _accepting = !_workers.empty();
}

void DatabaseConnectionSet::Shutdown(std::chrono::milliseconds drainTimeout)
{
    if (_shutDown.exchange(true))
        return;
    _accepting = false;

    _queue.Close();
    auto const deadline = std::chrono::steady_clock::now() + drainTimeout;
    auto withinDeadline = [this, deadline]
    {
        auto const now = std::chrono::steady_clock::now();
        return now < deadline && now.time_since_epoch().count() < _drainLimitNs.load(std::memory_order_relaxed);
    };
    while (withinDeadline() && std::any_of(_workers.begin(), _workers.end(), [](auto const& worker) { return !worker->IsFinished(); }))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (std::any_of(_workers.begin(), _workers.end(), [](auto const& worker) { return !worker->IsFinished(); }) || _workers.empty())
        _queue.Cancel();
    for (std::unique_ptr<DatabaseWorker>& worker : _workers)
        worker->Join();
    _workers.clear();
    std::size_t cancelled = 0;
    for (std::unique_ptr<SQLOperation>& operation : _queue.TakeAll())
    {
        operation->Cancel();
        ++cancelled;
    }
    if (cancelled)
        LOG_WARN("sql.driver", "Database pool {} cancelled {} queued operation(s) that did not finish before closing", _poolName, cancelled);

    {
        std::lock_guard<std::mutex> lock(_pingerMutex);
        _pingerStop = true;
    }
    _pingerCondition.notify_all();
    if (_pinger.joinable())
        _pinger.join();

    {
        std::unique_lock<std::mutex> lock(_syncMutex);
        _syncClosing = true;
        _syncCondition.notify_all();
        _syncCondition.wait(lock, [this] { return _syncWaiters == 0 && std::none_of(_syncBusy.begin(), _syncBusy.end(), [](bool busy) { return busy; }); });
    }
    for (auto* list : { &_syncConnections, &_asyncConnections })
        for (std::unique_ptr<MySQLConnection>& connection : *list)
            connection->Close();
}

void DatabaseConnectionSet::ShortenDrain(std::chrono::milliseconds remaining) noexcept
{
    int64 const limit = std::chrono::duration_cast<std::chrono::steady_clock::duration>((std::chrono::steady_clock::now() + remaining).time_since_epoch()).count();
    int64 current = _drainLimitNs.load(std::memory_order_relaxed);
    while (limit < current && !_drainLimitNs.compare_exchange_weak(current, limit, std::memory_order_relaxed))
    {
    }
}

bool DatabaseConnectionSet::Enqueue(std::unique_ptr<SQLOperation>& operation)
{
    if (!_accepting.load())
        return false;
    return _queue.Push(std::move(operation));
}

MySQLConnection* DatabaseConnectionSet::TakeFreeLocked()
{
    for (std::size_t step = 0; step < _syncBusy.size(); ++step)
    {
        std::size_t const index = (_nextSync + step) % _syncBusy.size();
        if (!_syncBusy[index])
        {
            _syncBusy[index] = true;
            _nextSync = (index + 1) % _syncBusy.size();
            return _syncConnections[index].get();
        }
    }
    return nullptr;
}

MySQLConnection* DatabaseConnectionSet::AcquireSync()
{
    std::unique_lock<std::mutex> lock(_syncMutex);
    ++_syncWaiters;
    _syncCondition.wait(lock, [this] { return _syncClosing || std::find(_syncBusy.begin(), _syncBusy.end(), false) != _syncBusy.end(); });
    MySQLConnection* const connection = _syncClosing ? nullptr : TakeFreeLocked();
    --_syncWaiters;
    if (_syncClosing)
        _syncCondition.notify_all();
    return connection;
}

MySQLConnection* DatabaseConnectionSet::TryAcquireSync()
{
    std::lock_guard<std::mutex> lock(_syncMutex);
    return _syncClosing ? nullptr : TakeFreeLocked();
}

void DatabaseConnectionSet::ReleaseSync(MySQLConnection* connection)
{
    {
        std::lock_guard<std::mutex> lock(_syncMutex);
        for (std::size_t i = 0; i < _syncConnections.size(); ++i)
            if (_syncConnections[i].get() == connection)
                _syncBusy[i] = false;
    }
    _syncCondition.notify_all();
}

void DatabaseConnectionSet::PingIdleSync()
{
    for (std::size_t i = 0; i < _syncConnections.size(); ++i)
    {
        MySQLConnection* const connection = TryAcquireSync();
        if (!connection)
            return;
        connection->Ping();
        ReleaseSync(connection);
    }
}

void DatabaseConnectionSet::RunPinger()
{
    Ambrose::Threading::SetCurrentThreadName(fmt::format("DB {} ping", _poolName));
    std::unique_lock<std::mutex> lock(_pingerMutex);
    auto lastPing = std::chrono::steady_clock::now();
    while (!_pingerStop)
    {
        int64 const interval = _keepAliveMs.load(std::memory_order_relaxed);
        std::chrono::milliseconds const wait = interval > 0 ? std::min(std::chrono::milliseconds(interval), std::chrono::milliseconds(1000)) : std::chrono::milliseconds(1000);
        _pingerCondition.wait_for(lock, wait, [this] { return _pingerStop; });
        if (_pingerStop)
            return;
        if (interval <= 0 || std::chrono::steady_clock::now() - lastPing < std::chrono::milliseconds(interval))
            continue;
        lock.unlock();
        PingIdleSync();
        lock.lock();
        lastPing = std::chrono::steady_clock::now();
    }
}

uint64 DatabaseConnectionSet::GetReconnectCount() const
{
    uint64 total = 0;
    for (auto const* list : { &_syncConnections, &_asyncConnections })
        for (std::unique_ptr<MySQLConnection> const& connection : *list)
            total += connection->GetReconnectCount();
    return total;
}

DatabasePoolUse DatabaseConnectionSet::GetUse() const
{
    DatabasePoolUse use;
    use.AsyncConnections = _asyncConnections.size();
    use.SyncConnections = _syncConnections.size();
    for (std::unique_ptr<MySQLConnection> const& connection : _asyncConnections)
        if (connection->IsInUse())
            ++use.AsyncActive;
    {
        std::lock_guard<std::mutex> lock(_syncMutex);
        use.SyncLeased = static_cast<std::size_t>(std::count(_syncBusy.begin(), _syncBusy.end(), true));
        use.SyncWaiting = _syncWaiters;
    }
    use.Queued = _queue.Size();
    use.Reconnects = GetReconnectCount();
    return use;
}

uint64 DatabaseConnectionSet::GetConcurrentUseCount() const
{
    uint64 total = 0;
    for (auto const* list : { &_syncConnections, &_asyncConnections })
        for (std::unique_ptr<MySQLConnection> const& connection : *list)
            total += connection->GetConcurrentUseCount();
    return total;
}
