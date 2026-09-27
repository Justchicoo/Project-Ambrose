/*
 * Project Ambrose by Imjustchico
 * Backlog ring and subscriber registry for any live stream of sequenced records: a new subscriber gets the matching backlog before anything published after it, so there is no gap and no repeat, its ring follows the overflow policy it subscribed with, dropping the oldest unless its stream chose otherwise, a subscription closes when the last copy of the handle Subscribe returned is dropped, and the registry holds it plainly so publishing takes no reference count per subscriber.
 */

#ifndef AMBROSE_STREAMHUB_H
#define AMBROSE_STREAMHUB_H

#include "StreamSubscription.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

template<class Record, class Filter>
class StreamHub
{
public:
    using Subscription = StreamSubscription<Record, Filter>;
    using RecordPtr = std::shared_ptr<Record const>;

    static constexpr std::size_t DefaultBacklog = 1000;
    static constexpr std::size_t MaxBacklog = 100000;
    static constexpr std::size_t DefaultSubscriberCapacity = 10000;

    StreamHub() = default;

    StreamHub(StreamHub const&) = delete;
    StreamHub& operator=(StreamHub const&) = delete;

    std::shared_ptr<Subscription> Subscribe(Filter filter, std::size_t capacity = DefaultSubscriberCapacity, std::function<void()> wake = {}, StreamOverflow overflow = StreamOverflow::DropOldest)
    {
        auto subscription = std::make_shared<Subscription>(std::move(filter), capacity, std::move(wake), overflow);
        bool shouldWake = false;
        {
            std::lock_guard lock(_mutex);
            for (RecordPtr const& record : _backlog)
                if (subscription->GetFilter().Matches(*record))
                    shouldWake = subscription->Push(record) || shouldWake;
            _subscribers.push_back(subscription);
        }
        if (shouldWake)
            subscription->Wake();
        return std::shared_ptr<Subscription>(subscription.get(), [subscription](Subscription* handle) { handle->Close(); });
    }

    void SetBacklogCapacity(std::size_t capacity)
    {
        std::lock_guard lock(_mutex);
        _backlogCapacity = std::min(capacity, MaxBacklog);
        while (_backlog.size() > _backlogCapacity)
            _backlog.pop_front();
    }

    std::size_t GetBacklogCapacity() const
    {
        std::lock_guard lock(_mutex);
        return _backlogCapacity;
    }

    void Publish(Record const& record)
    {
        Publish(std::make_shared<Record const>(record));
    }

    void Publish(RecordPtr const& record)
    {
        std::vector<std::shared_ptr<Subscription>> toWake;
        {
            std::lock_guard lock(_mutex);
            if (_backlogCapacity > 0)
            {
                if (_backlog.size() >= _backlogCapacity)
                    _backlog.pop_front();
                _backlog.push_back(record);
            }
            bool prune = false;
            for (std::shared_ptr<Subscription> const& subscription : _subscribers)
            {
                if (subscription->IsClosed())
                {
                    prune = true;
                    continue;
                }
                if (subscription->GetFilter().Matches(*record) && subscription->Push(record))
                    toWake.push_back(subscription);
            }
            if (prune)
                std::erase_if(_subscribers, [](std::shared_ptr<Subscription> const& subscription) { return subscription->IsClosed(); });
        }
        for (std::shared_ptr<Subscription> const& subscription : toWake)
            subscription->Wake();
    }

    std::vector<RecordPtr> GetBacklog() const
    {
        std::lock_guard lock(_mutex);
        return std::vector<RecordPtr>(_backlog.begin(), _backlog.end());
    }

    std::size_t GetSubscriberCount() const
    {
        std::lock_guard lock(_mutex);
        return static_cast<std::size_t>(std::count_if(_subscribers.begin(), _subscribers.end(), [](std::shared_ptr<Subscription> const& subscription) { return !subscription->IsClosed(); }));
    }

private:
    mutable std::mutex _mutex;
    std::deque<RecordPtr> _backlog;
    std::size_t _backlogCapacity = DefaultBacklog;
    std::vector<std::shared_ptr<Subscription>> _subscribers;
};

#endif
