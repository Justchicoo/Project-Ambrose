/*
 * Project Ambrose by Imjustchico
 * Waits on the shared queue in short slices, runs each operation on the worker's own connection, pings after the idle interval, and exits when the queue is cancelled or closed and drained.
 */

#include "DatabaseWorker.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "ThreadName.h"

#include <algorithm>
#include <chrono>

DatabaseWorker::DatabaseWorker(Queue& queue, MySQLConnection& connection, std::string threadName, std::atomic<int64> const& keepAliveMs)
    : _queue(queue), _connection(connection), _threadName(std::move(threadName)), _keepAliveMs(keepAliveMs), _thread([this] { Run(); })
{
}

DatabaseWorker::~DatabaseWorker()
{
    Join();
}

void DatabaseWorker::Join()
{
    if (_thread.joinable())
        _thread.join();
}

void DatabaseWorker::Run()
{
    Ambrose::Threading::SetCurrentThreadName(_threadName);
    constexpr std::chrono::milliseconds MaxWait{ 1000 };
    auto lastWork = std::chrono::steady_clock::now();
    while (true)
    {
        std::unique_ptr<SQLOperation> operation;
        int64 const keepAlive = _keepAliveMs.load(std::memory_order_relaxed);
        std::chrono::milliseconds const wait = keepAlive > 0 ? std::min(std::chrono::milliseconds(keepAlive), MaxWait) : MaxWait;
        if (_queue.WaitAndPopFor(operation, wait) && operation)
        {
            try
            {
                operation->Execute(_connection);
            }
            catch (std::exception const& exception)
            {
                LOG_ERROR("sql.sql", "Database worker {} caught an exception: {}", _threadName, exception.what());
            }
            lastWork = std::chrono::steady_clock::now();
            continue;
        }
        if (_queue.IsCancelled() || (_queue.IsClosed() && _queue.Empty()))
            break;
        int64 const interval = _keepAliveMs.load(std::memory_order_relaxed);
        if (interval > 0 && std::chrono::steady_clock::now() - lastWork >= std::chrono::milliseconds(interval))
        {
            _connection.Ping();
            lastWork = std::chrono::steady_clock::now();
        }
    }
    _finished = true;
}
