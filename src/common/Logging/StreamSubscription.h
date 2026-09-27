/*
 * Project Ambrose by Imjustchico
 * One live-stream subscriber's filter and bounded drop-oldest ring of records, for any record type a stream carries: it never allocates once grown, is guarded by a spin lock because the publishing thread takes it on every record, counts what it dropped and wakes its reader on the edge from empty to not.
 */

#ifndef AMBROSE_STREAMSUBSCRIPTION_H
#define AMBROSE_STREAMSUBSCRIPTION_H

#include "SpinLock.h"
#include "Types.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

struct StreamPopResult
{
    std::size_t Count = 0;
    uint64 Dropped = 0;
};

template<class Record, class Filter>
class StreamSubscription
{
public:
    using RecordPtr = std::shared_ptr<Record const>;

    StreamSubscription(Filter filter, std::size_t capacity, std::function<void()> wake)
        : _filter(std::move(filter)), _capacity(capacity == 0 ? 1 : capacity), _wake(std::move(wake))
    {
    }

    StreamSubscription(StreamSubscription const&) = delete;
    StreamSubscription& operator=(StreamSubscription const&) = delete;

    Filter const& GetFilter() const noexcept { return _filter; }
    std::size_t GetCapacity() const noexcept { return _capacity; }

    bool Push(RecordPtr const& record)
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

    void Wake() const
    {
        if (_wake)
            _wake();
    }

    StreamPopResult Pop(std::vector<RecordPtr>& out, std::size_t max)
    {
        std::lock_guard lock(_mutex);
        StreamPopResult result;
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

    std::size_t GetQueuedCount() const
    {
        std::lock_guard lock(_mutex);
        return _count;
    }

    uint64 GetTotalDropped() const
    {
        std::lock_guard lock(_mutex);
        return _droppedTotal;
    }

    void Close()
    {
        std::lock_guard lock(_mutex);
        _closed.store(true, std::memory_order_relaxed);
        _ring.clear();
        _head = 0;
        _count = 0;
    }

    bool IsClosed() const noexcept { return _closed.load(std::memory_order_relaxed); }

private:
    void Grow()
    {
        std::size_t const size = std::min(_capacity, std::max<std::size_t>(64, _ring.size() * 2));
        std::vector<RecordPtr> grown(size);
        for (std::size_t index = 0; index < _count; ++index)
            grown[index] = std::move(_ring[(_head + index) % _ring.size()]);
        _ring.swap(grown);
        _head = 0;
    }

    Filter _filter;
    std::size_t _capacity;
    std::function<void()> _wake;
    mutable Ambrose::SpinLock _mutex;
    std::vector<RecordPtr> _ring;
    std::size_t _head = 0;
    std::size_t _count = 0;
    uint64 _droppedSincePop = 0;
    uint64 _droppedTotal = 0;
    std::atomic<bool> _closed{ false };
};

#endif
