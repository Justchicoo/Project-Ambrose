/*
 * Project Ambrose by Imjustchico
 * Publishes live log records into a backlog and every matching subscriber, with gap-free subscription.
 */

#include "LogStreamHub.h"

#include <algorithm>

std::shared_ptr<LogSubscription> LogStreamHub::Subscribe(LogStreamFilter filter, std::size_t capacity, std::function<void()> wake)
{
    auto subscription = std::make_shared<LogSubscription>(std::move(filter), capacity, std::move(wake));
    bool shouldWake = false;
    {
        std::lock_guard lock(_mutex);
        for (std::shared_ptr<LogMessage const> const& record : _backlog)
            if (subscription->GetFilter().Matches(*record))
                shouldWake = subscription->Push(record) || shouldWake;
        _subscribers.push_back(subscription);
    }
    if (shouldWake)
        subscription->Wake();
    return subscription;
}

void LogStreamHub::SetBacklogCapacity(std::size_t capacity)
{
    std::lock_guard lock(_mutex);
    _backlogCapacity = std::min(capacity, MaxBacklog);
    while (_backlog.size() > _backlogCapacity)
        _backlog.pop_front();
}

std::size_t LogStreamHub::GetBacklogCapacity() const
{
    std::lock_guard lock(_mutex);
    return _backlogCapacity;
}

void LogStreamHub::Publish(LogMessage const& message)
{
    auto const record = std::make_shared<LogMessage const>(message);
    std::vector<std::shared_ptr<LogSubscription>> toWake;
    {
        std::lock_guard lock(_mutex);
        if (_backlogCapacity > 0)
        {
            if (_backlog.size() >= _backlogCapacity)
                _backlog.pop_front();
            _backlog.push_back(record);
        }
        bool prune = false;
        for (std::weak_ptr<LogSubscription> const& weak : _subscribers)
        {
            std::shared_ptr<LogSubscription> subscription = weak.lock();
            if (!subscription || subscription->IsClosed())
            {
                prune = true;
                continue;
            }
            if (subscription->GetFilter().Matches(*record) && subscription->Push(record))
                toWake.push_back(std::move(subscription));
        }
        if (prune)
        {
            std::erase_if(_subscribers, [](std::weak_ptr<LogSubscription> const& weak)
            {
                std::shared_ptr<LogSubscription> const subscription = weak.lock();
                return !subscription || subscription->IsClosed();
            });
        }
    }
    for (std::shared_ptr<LogSubscription> const& subscription : toWake)
        subscription->Wake();
}

std::vector<std::shared_ptr<LogMessage const>> LogStreamHub::GetBacklog() const
{
    std::lock_guard lock(_mutex);
    return std::vector<std::shared_ptr<LogMessage const>>(_backlog.begin(), _backlog.end());
}

std::size_t LogStreamHub::GetSubscriberCount() const
{
    std::lock_guard lock(_mutex);
    return static_cast<std::size_t>(std::count_if(_subscribers.begin(), _subscribers.end(), [](std::weak_ptr<LogSubscription> const& weak)
    {
        std::shared_ptr<LogSubscription> const subscription = weak.lock();
        return subscription && !subscription->IsClosed();
    }));
}
