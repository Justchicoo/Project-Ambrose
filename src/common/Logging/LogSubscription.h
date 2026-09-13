/*
 * Project Ambrose by Imjustchico
 * One live-log subscriber's filter and bounded drop-oldest queue with dropped counts and an edge-triggered wake.
 */

#ifndef AMBROSE_LOGSUBSCRIPTION_H
#define AMBROSE_LOGSUBSCRIPTION_H

#include "LogMessage.h"

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct LogStreamFilter
{
    LogLevel MinLevel = LogLevel::Trace;
    std::vector<std::string> Categories;

    bool Matches(LogMessage const& message) const noexcept;
};

struct LogPopResult
{
    std::size_t Count = 0;
    uint64 Dropped = 0;
};

class LogSubscription
{
public:
    LogSubscription(LogStreamFilter filter, std::size_t capacity, std::function<void()> wake);

    LogSubscription(LogSubscription const&) = delete;
    LogSubscription& operator=(LogSubscription const&) = delete;

    LogStreamFilter const& GetFilter() const noexcept;
    std::size_t GetCapacity() const noexcept;
    bool Push(std::shared_ptr<LogMessage const> const& record);
    void Wake() const;
    LogPopResult Pop(std::vector<std::shared_ptr<LogMessage const>>& out, std::size_t max);
    std::size_t GetQueuedCount() const;
    uint64 GetTotalDropped() const;
    void Close();
    bool IsClosed() const;

private:
    LogStreamFilter _filter;
    std::size_t _capacity;
    std::function<void()> _wake;
    mutable std::mutex _mutex;
    std::deque<std::shared_ptr<LogMessage const>> _queue;
    uint64 _droppedSincePop = 0;
    uint64 _droppedTotal = 0;
    bool _closed = false;
};

#endif
