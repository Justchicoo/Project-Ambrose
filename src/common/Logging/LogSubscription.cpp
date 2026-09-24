/*
 * Project Ambrose by Imjustchico
 * Queues filtered live log records for one subscriber in a ring that grows to its capacity and then overwrites the oldest, counting what it dropped and waking on the first record.
 */

#include "LogSubscription.h"

#include <algorithm>
#include <utility>

bool LogStreamFilter::MatchesCategory(std::string_view category) const noexcept
{
    for (std::string const& prefix : Categories)
        if (Ambrose::Logging::IsCategoryWithin(category, prefix))
            return true;
    return false;
}

LogSubscription::LogSubscription(LogStreamFilter filter, std::size_t capacity, std::function<void()> wake)
    : _filter(std::move(filter)), _capacity(capacity == 0 ? 1 : capacity), _wake(std::move(wake))
{
}

LogStreamFilter const& LogSubscription::GetFilter() const noexcept
{
    return _filter;
}

std::size_t LogSubscription::GetCapacity() const noexcept
{
    return _capacity;
}

bool LogSubscription::Push(std::shared_ptr<LogMessage const> const& record)
{
    if (_closed.load(std::memory_order_relaxed))
        return false;
    std::lock_guard lock(_mutex);
    if (_closed.load(std::memory_order_relaxed))
        return false;
    bool const wasEmpty = _count == 0;
    if (_count == _ring.size() && _ring.size() < _capacity)
        Grow();
    if (_count == _capacity)
    {
        _ring[_head] = record;
        if (++_head == _ring.size())
            _head = 0;
        ++_droppedSincePop;
        ++_droppedTotal;
    }
    else
    {
        std::size_t slot = _head + _count;
        if (slot >= _ring.size())
            slot -= _ring.size();
        _ring[slot] = record;
        ++_count;
    }
    return wasEmpty && static_cast<bool>(_wake);
}

void LogSubscription::Grow()
{
    std::size_t const size = std::min(_capacity, std::max<std::size_t>(64, _ring.size() * 2));
    std::vector<std::shared_ptr<LogMessage const>> grown(size);
    for (std::size_t index = 0; index < _count; ++index)
        grown[index] = std::move(_ring[(_head + index) % _ring.size()]);
    _ring.swap(grown);
    _head = 0;
}

void LogSubscription::Wake() const
{
    if (_wake)
        _wake();
}

LogPopResult LogSubscription::Pop(std::vector<std::shared_ptr<LogMessage const>>& out, std::size_t max)
{
    std::lock_guard lock(_mutex);
    LogPopResult result;
    while (result.Count < max && _count > 0)
    {
        out.push_back(std::move(_ring[_head]));
        if (++_head == _ring.size())
            _head = 0;
        --_count;
        ++result.Count;
    }
    result.Dropped = std::exchange(_droppedSincePop, 0);
    return result;
}

std::size_t LogSubscription::GetQueuedCount() const
{
    std::lock_guard lock(_mutex);
    return _count;
}

uint64 LogSubscription::GetTotalDropped() const
{
    std::lock_guard lock(_mutex);
    return _droppedTotal;
}

void LogSubscription::Close()
{
    std::lock_guard lock(_mutex);
    _closed.store(true, std::memory_order_relaxed);
    _ring.clear();
    _head = 0;
    _count = 0;
}

bool LogSubscription::IsClosed() const noexcept
{
    return _closed.load(std::memory_order_relaxed);
}
