/*
 * Project Ambrose by Imjustchico
 * Runs queued log records on a dedicated thread in order, honoring barriers, drop counts, and idle flushes.
 */

#include "LogWorker.h"

#include <algorithm>
#include <utility>

void LogBarrier::Complete()
{
    {
        std::lock_guard lock(Mutex);
        Completed = true;
    }
    Done.notify_all();
}

bool LogBarrier::WaitFor(std::chrono::milliseconds timeout)
{
    std::unique_lock lock(Mutex);
    return Done.wait_for(lock, timeout, [this] { return Completed; });
}

LogWorker::LogWorker(std::size_t capacity, LogOverflowPolicy policy, DispatchFunction dispatch, DropFunction dropped, IdleFunction idle)
    : _capacity(std::max<std::size_t>(capacity, 1)), _policy(policy), _dispatch(std::move(dispatch)), _droppedCallback(std::move(dropped)), _idle(std::move(idle))
{
    std::lock_guard lock(_mutex);
    _thread = std::thread([this] { Run(); });
    _threadId = _thread.get_id();
}

LogWorker::~LogWorker()
{
    Close();
    if (_thread.joinable() && std::this_thread::get_id() != _threadId)
        _thread.join();
    else if (_thread.joinable())
        _thread.detach();
    Release();
}

LogWorker::PushResult LogWorker::Push(LogWorkItem& item)
{
    bool notify = false;
    {
        std::unique_lock lock(_mutex);
        if (_closed)
            return PushResult::Closed;
        if (!item.Barrier)
        {
            if (_messageCount >= _capacity)
            {
                if (_policy == LogOverflowPolicy::Drop)
                {
                    ++_dropped;
                    ++_droppedTotal;
                    return PushResult::Dropped;
                }
                _space.wait(lock, [this] { return _messageCount < _capacity || _closed; });
                if (_closed)
                    return PushResult::Closed;
            }
            ++_messageCount;
            _highWater = std::max(_highWater, _messageCount);
        }
        notify = _items.empty();
        _items.push_back(std::move(item));
    }
    if (notify)
        _wake.notify_one();
    return PushResult::Queued;
}

void LogWorker::Close()
{
    {
        std::lock_guard lock(_mutex);
        _closed = true;
    }
    _wake.notify_all();
    _space.notify_all();
}

void LogWorker::Join()
{
    if (_thread.joinable() && std::this_thread::get_id() != _threadId)
        _thread.join();
}

void LogWorker::Release()
{
    {
        std::lock_guard lock(_mutex);
        _isReleased = true;
    }
    _released.notify_all();
}

void LogWorker::WaitUntilReleased()
{
    std::unique_lock lock(_mutex);
    _released.wait(lock, [this] { return _isReleased; });
}

bool LogWorker::IsWorkerThread() const noexcept
{
    return std::this_thread::get_id() == _threadId;
}

std::size_t LogWorker::GetCapacity() const noexcept
{
    return _capacity;
}

LogOverflowPolicy LogWorker::GetPolicy() const noexcept
{
    return _policy;
}

uint64 LogWorker::GetDroppedTotal() const noexcept
{
    std::lock_guard lock(_mutex);
    return _droppedTotal;
}

std::size_t LogWorker::GetHighWaterMark() const noexcept
{
    std::lock_guard lock(_mutex);
    return _highWater;
}

void LogWorker::Run()
{
    {
        std::lock_guard lock(_mutex);
    }
    std::chrono::steady_clock::time_point nextIdle = std::chrono::steady_clock::now() + IdleTick;
    while (true)
    {
        std::vector<LogWorkItem> batch;
        uint64 dropped = 0;
        bool finished = false;
        {
            std::unique_lock lock(_mutex);
            _wake.wait_until(lock, nextIdle, [this] { return !_items.empty() || _closed; });
            batch.swap(_items);
            dropped = std::exchange(_dropped, 0);
            _messageCount = 0;
            finished = batch.empty() && _closed;
        }
        bool const timedOut = batch.empty();
        if (!timedOut)
            _space.notify_all();
        for (LogWorkItem& item : batch)
        {
            if (item.Barrier && dropped > 0 && _droppedCallback)
                _droppedCallback(std::exchange(dropped, 0), item.Routing);
            _dispatch(item);
        }
        if (dropped > 0 && _droppedCallback)
            _droppedCallback(dropped, timedOut ? nullptr : batch.back().Routing);
        batch.clear();
        std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
        if (finished || timedOut || now >= nextIdle)
        {
            if (_idle)
                _idle();
            nextIdle = now + IdleTick;
        }
        if (finished)
            break;
    }
}
