/*
 * Project Ambrose by Imjustchico
 * Backlog ring and subscriber registry for live structured log records; a subscription closes when the last copy of the handle Subscribe returned is dropped, and the registry holds it plainly so publishing takes no reference count per subscriber.
 */

#ifndef AMBROSE_LOGSTREAMHUB_H
#define AMBROSE_LOGSTREAMHUB_H

#include "LogSubscription.h"

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class LogStreamHub
{
public:
    static constexpr std::size_t DefaultBacklog = 1000;
    static constexpr std::size_t MaxBacklog = 100000;
    static constexpr std::size_t DefaultSubscriberCapacity = 10000;

    LogStreamHub() = default;

    LogStreamHub(LogStreamHub const&) = delete;
    LogStreamHub& operator=(LogStreamHub const&) = delete;

    std::shared_ptr<LogSubscription> Subscribe(LogStreamFilter filter, std::size_t capacity = DefaultSubscriberCapacity, std::function<void()> wake = {});
    void SetBacklogCapacity(std::size_t capacity);
    std::size_t GetBacklogCapacity() const;
    void Publish(LogMessage const& message);
    std::vector<std::shared_ptr<LogMessage const>> GetBacklog() const;
    std::size_t GetSubscriberCount() const;

private:
    mutable std::mutex _mutex;
    std::deque<std::shared_ptr<LogMessage const>> _backlog;
    std::size_t _backlogCapacity = DefaultBacklog;
    std::vector<std::shared_ptr<LogSubscription>> _subscribers;
};

#endif
