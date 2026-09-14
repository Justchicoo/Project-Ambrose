/*
 * Project Ambrose by Imjustchico
 * One async database thread that owns a connection, runs queued operations in order, and pings its connection when it has been idle for the keepalive interval.
 */

#ifndef AMBROSE_DATABASEWORKER_H
#define AMBROSE_DATABASEWORKER_H

#include "ProducerConsumerQueue.h"
#include "SQLOperation.h"
#include "Types.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

class DatabaseWorker
{
public:
    using Queue = ProducerConsumerQueue<std::unique_ptr<SQLOperation>>;

    DatabaseWorker(Queue& queue, MySQLConnection& connection, std::string threadName, std::atomic<int64> const& keepAliveMs);
    ~DatabaseWorker();

    DatabaseWorker(DatabaseWorker const&) = delete;
    DatabaseWorker& operator=(DatabaseWorker const&) = delete;

    void Join();
    bool IsFinished() const noexcept { return _finished.load(); }

private:
    void Run();

    Queue& _queue;
    MySQLConnection& _connection;
    std::string _threadName;
    std::atomic<int64> const& _keepAliveMs;
    std::atomic<bool> _finished{ false };
    std::thread _thread;
};

#endif
