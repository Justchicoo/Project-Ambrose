/*
 * Project Ambrose by Imjustchico
 * Queues filtered live log records for one subscriber, dropping the oldest when full and waking on the first record.
 */

#include "LogSubscription.h"

#include <utility>

bool LogStreamFilter::Matches(LogMessage const& message) const noexcept
{
    if (!IsLevelEnabled(MinLevel, message.Level))
        return false;
    if (Categories.empty())
        return true;
    for (std::string const& category : Categories)
        if (Ambrose::Logging::IsCategoryWithin(message.Category, category))
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
    std::lock_guard lock(_mutex);
    if (_closed)
        return false;
    bool const wasEmpty = _queue.empty();
    if (_queue.size() >= _capacity)
    {
        _queue.pop_front();
        ++_droppedSincePop;
        ++_droppedTotal;
    }
    _queue.push_back(record);
    return wasEmpty && static_cast<bool>(_wake);
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
    while (result.Count < max && !_queue.empty())
    {
        out.push_back(std::move(_queue.front()));
        _queue.pop_front();
        ++result.Count;
    }
    result.Dropped = std::exchange(_droppedSincePop, 0);
    return result;
}

std::size_t LogSubscription::GetQueuedCount() const
{
    std::lock_guard lock(_mutex);
    return _queue.size();
}

uint64 LogSubscription::GetTotalDropped() const
{
    std::lock_guard lock(_mutex);
    return _droppedTotal;
}

void LogSubscription::Close()
{
    std::lock_guard lock(_mutex);
    _closed = true;
    _queue.clear();
}

bool LogSubscription::IsClosed() const
{
    std::lock_guard lock(_mutex);
    return _closed;
}
