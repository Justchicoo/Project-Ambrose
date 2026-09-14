/*
 * Project Ambrose by Imjustchico
 * Blocking multi-producer multi-consumer queue with timed waits, cancel for immediate stop, and close for draining stop.
 */

#ifndef AMBROSE_PRODUCERCONSUMERQUEUE_H
#define AMBROSE_PRODUCERCONSUMERQUEUE_H

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

template<typename T>
class ProducerConsumerQueue
{
public:
    ProducerConsumerQueue() = default;

    ProducerConsumerQueue(ProducerConsumerQueue const&) = delete;
    ProducerConsumerQueue& operator=(ProducerConsumerQueue const&) = delete;

    bool Push(T const& value)
    {
        return Emplace(value);
    }

    bool Push(T&& value)
    {
        return Emplace(std::move(value));
    }

    template<typename... Args>
    bool Emplace(Args&&... args)
    {
        {
            std::lock_guard lock(_mutex);
            if (_cancelled || _closed)
                return false;
            _queue.emplace_back(std::forward<Args>(args)...);
        }
        _condition.notify_one();
        return true;
    }

    bool Pop(T& value)
    {
        std::lock_guard lock(_mutex);
        if (_cancelled || _queue.empty())
            return false;
        value = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    bool WaitAndPop(T& value)
    {
        std::unique_lock lock(_mutex);
        _condition.wait(lock, [this] { return _cancelled || _closed || !_queue.empty(); });
        return TakeLocked(value);
    }

    template<typename Rep, typename Period>
    bool WaitAndPopFor(T& value, std::chrono::duration<Rep, Period> timeout)
    {
        std::unique_lock lock(_mutex);
        _condition.wait_for(lock, timeout, [this] { return _cancelled || _closed || !_queue.empty(); });
        return TakeLocked(value);
    }

    void Cancel()
    {
        {
            std::lock_guard lock(_mutex);
            _cancelled = true;
        }
        _condition.notify_all();
    }

    void Close()
    {
        {
            std::lock_guard lock(_mutex);
            _closed = true;
        }
        _condition.notify_all();
    }

    bool IsCancelled() const
    {
        std::lock_guard lock(_mutex);
        return _cancelled;
    }

    bool IsClosed() const
    {
        std::lock_guard lock(_mutex);
        return _closed;
    }

    bool Empty() const
    {
        std::lock_guard lock(_mutex);
        return _queue.empty();
    }

    std::size_t Size() const
    {
        std::lock_guard lock(_mutex);
        return _queue.size();
    }

    std::deque<T> TakeAll()
    {
        std::lock_guard lock(_mutex);
        return std::exchange(_queue, std::deque<T>());
    }

private:
    bool TakeLocked(T& value)
    {
        if (_cancelled || _queue.empty())
            return false;
        value = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    mutable std::mutex _mutex;
    std::condition_variable _condition;
    std::deque<T> _queue;
    bool _cancelled = false;
    bool _closed = false;
};

#endif
