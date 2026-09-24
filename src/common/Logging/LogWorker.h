/*
 * Project Ambrose by Imjustchico
 * Async log thread with a bounded queue, in-queue flush barriers, and a close, join and release handoff.
 */

#ifndef AMBROSE_LOGWORKER_H
#define AMBROSE_LOGWORKER_H

#include "LogCommon.h"
#include "LogMessage.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class LogRouting;

struct LogBarrier
{
    std::mutex Mutex;
    std::condition_variable Done;
    bool Completed = false;

    void Complete();
    bool WaitFor(std::chrono::milliseconds timeout);
};

struct LogWorkItem
{
    std::shared_ptr<LogRouting const> Routing;
    uint16 LoggerIndex = 0;
    LogMessage Message;
    std::shared_ptr<LogBarrier> Barrier;
};

class LogWorker
{
public:
    enum class PushResult : uint8
    {
        Queued,
        Dropped,
        Closed
    };

    using DispatchFunction = std::function<void(LogWorkItem&)>;
    using DropFunction = std::function<void(uint64, std::shared_ptr<LogRouting const> const&)>;
    using IdleFunction = std::function<void()>;

    static constexpr std::chrono::milliseconds IdleTick{ 250 };

    LogWorker(std::size_t capacity, LogOverflowPolicy policy, DispatchFunction dispatch, DropFunction dropped, IdleFunction idle);
    ~LogWorker();

    LogWorker(LogWorker const&) = delete;
    LogWorker& operator=(LogWorker const&) = delete;

    PushResult Push(LogWorkItem& item);
    void Close();
    void Join();
    void Release();
    void WaitUntilReleased();
    bool IsWorkerThread() const noexcept;
    std::size_t GetCapacity() const noexcept;
    LogOverflowPolicy GetPolicy() const noexcept;
    uint64 GetDroppedTotal() const noexcept;
    std::size_t GetHighWaterMark() const noexcept;

private:
    void Run();

    std::size_t _capacity;
    LogOverflowPolicy _policy;
    DispatchFunction _dispatch;
    DropFunction _droppedCallback;
    IdleFunction _idle;
    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::condition_variable _space;
    std::condition_variable _released;
    std::vector<LogWorkItem> _items;
    std::size_t _messageCount = 0;
    bool _closed = false;
    bool _isReleased = false;
    uint64 _dropped = 0;
    uint64 _droppedTotal = 0;
    std::size_t _highWater = 0;
    std::thread::id _threadId;
    std::thread _thread;
};

#endif
