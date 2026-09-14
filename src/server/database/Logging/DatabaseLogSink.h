/*
 * Project Ambrose by Imjustchico
 * The process-wide buffer behind DB log appenders: a bounded queue of log rows that a background thread commits to the login database in batched transactions, backing off when the pool is busy and counting what it drops.
 */

#ifndef AMBROSE_DATABASELOGSINK_H
#define AMBROSE_DATABASELOGSINK_H

#include "Types.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class DatabaseLogSink
{
public:
    struct Row
    {
        uint64 LoggedAtMs = 0;
        uint32 RealmId = 0;
        std::string Category;
        uint8 Level = 0;
        std::string Message;
    };

    static constexpr std::size_t DefaultCapacity = 10000;
    static constexpr std::size_t BatchSize = 500;
    static constexpr std::size_t BusyQueueThreshold = 64;
    static constexpr std::size_t MaxCategoryLength = 255;
    static constexpr std::size_t MaxMessageLength = 65535;
    static constexpr std::chrono::milliseconds FlushInterval{ 250 };

    static DatabaseLogSink& Instance();

    void Start();
    void Stop(std::chrono::milliseconds drainTimeout);
    void Push(Row row);
    void Flush();
    void SetCapacity(std::size_t capacity) noexcept { _capacity = capacity ? capacity : DefaultCapacity; }

    bool IsRunning() const noexcept { return _running.load(); }
    std::size_t GetPendingCount() const;
    uint64 GetDroppedCount() const noexcept { return _dropped.load(); }
    uint64 GetWrittenCount() const noexcept { return _written.load(); }

    static std::string PrepareText(std::string_view text, std::size_t maxLength);

private:
    DatabaseLogSink() = default;

    void Run();
    bool CommitBatch(std::deque<Row>& batch);

    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<Row> _rows;
    std::atomic<std::size_t> _capacity{ DefaultCapacity };
    std::atomic<bool> _running{ false };
    bool _stopRequested = false;
    bool _flushRequested = false;
    std::atomic<uint64> _dropped{ 0 };
    std::atomic<uint64> _written{ 0 };
    uint64 _reportedDrops = 0;
    bool _warnedNoAsync = false;
    std::thread _thread;
};

#define sDatabaseLogSink DatabaseLogSink::Instance()

#endif
