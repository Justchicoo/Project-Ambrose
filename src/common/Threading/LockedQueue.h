/*
 * Project Ambrose by Imjustchico
 * Mutex-protected non-blocking queue with conditional pops, peeking, bulk adds, and a cancel flag.
 */

#ifndef AMBROSE_LOCKEDQUEUE_H
#define AMBROSE_LOCKEDQUEUE_H

#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

template<typename T, typename Storage = std::deque<T>>
class LockedQueue
{
public:
    LockedQueue() = default;

    LockedQueue(LockedQueue const&) = delete;
    LockedQueue& operator=(LockedQueue const&) = delete;

    void Add(T const& item)
    {
        std::lock_guard lock(_mutex);
        _queue.push_back(item);
    }

    void Add(T&& item)
    {
        std::lock_guard lock(_mutex);
        _queue.push_back(std::move(item));
    }

    template<typename Iterator>
    void AddRange(Iterator begin, Iterator end)
    {
        std::lock_guard lock(_mutex);
        _queue.insert(_queue.end(), begin, end);
    }

    bool Next(T& result)
    {
        std::lock_guard lock(_mutex);
        if (_queue.empty())
            return false;
        result = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    template<typename Checker>
    bool Next(T& result, Checker&& check)
    {
        std::lock_guard lock(_mutex);
        if (_queue.empty() || !check(_queue.front()))
            return false;
        result = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    bool Peek(T& result) const
    {
        std::lock_guard lock(_mutex);
        if (_queue.empty())
            return false;
        result = _queue.front();
        return true;
    }

    void Cancel()
    {
        std::lock_guard lock(_mutex);
        _cancelled = true;
    }

    bool IsCancelled() const
    {
        std::lock_guard lock(_mutex);
        return _cancelled;
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

    void Clear()
    {
        std::lock_guard lock(_mutex);
        _queue.clear();
    }

private:
    mutable std::mutex _mutex;
    Storage _queue;
    bool _cancelled = false;
};

#endif
