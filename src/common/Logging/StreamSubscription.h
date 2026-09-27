/*
 * Project Ambrose by Imjustchico
 * One live-stream subscriber's filter and bounded ring of records, for any record type a stream carries, with the policy its stream chose for a full ring: drop the oldest and count it, which every feed has unless it asks otherwise, keep everything and say it overflowed rather than lose a record, taking nothing more once it has, so what it delivers has no gap its reader could mistake for the whole, or keep only the newest record for each key, where a record replaced that way is not counted as dropped and the rest stay in sequence order. It never allocates once grown, is guarded by a spin lock because the publishing thread takes it on every record, and wakes its reader on the edge from empty to not and when it first overflows.
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

enum class StreamOverflow : uint8
{
    DropOldest,
    KeepAll,
    NewestPerKey
};

template<class Record>
concept KeyedStreamRecord = requires(Record const& record) { record.Key == record.Key; };

struct StreamPopResult
{
    std::size_t Count = 0;
    uint64 Dropped = 0;
    bool Overflowed = false;
};

template<class Record, class Filter>
class StreamSubscription
{
public:
    using RecordPtr = std::shared_ptr<Record const>;

    StreamSubscription(Filter filter, std::size_t capacity, std::function<void()> wake, StreamOverflow overflow = StreamOverflow::DropOldest)
        : _filter(std::move(filter)), _capacity(capacity == 0 ? 1 : capacity), _wake(std::move(wake)), _overflow(overflow)
    {
    }

    StreamSubscription(StreamSubscription const&) = delete;
    StreamSubscription& operator=(StreamSubscription const&) = delete;

    Filter const& GetFilter() const noexcept { return _filter; }
    std::size_t GetCapacity() const noexcept { return _capacity; }
    StreamOverflow GetOverflow() const noexcept { return _overflow; }

    bool Push(RecordPtr const& record)
    {
        if (_closed.load(std::memory_order_relaxed))
            return false;
        std::lock_guard lock(_mutex);
        if (_closed.load(std::memory_order_relaxed) || _overflowed)
            return false;
        if (_overflow == StreamOverflow::NewestPerKey)
            RemoveKeyOf(*record);
        bool const wasEmpty = _count == 0;
        if (_count == _ring.size() && _ring.size() < _capacity)
            Grow();
        if (_count == _capacity)
        {
            if (_overflow == StreamOverflow::KeepAll)
            {
                bool const first = !_overflowed;
                _overflowed = true;
                return first && static_cast<bool>(_wake);
            }
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
        result.Overflowed = _overflowed && _count == 0;
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

    bool IsOverflowed() const
    {
        std::lock_guard lock(_mutex);
        return _overflowed;
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

    void RemoveKeyOf([[maybe_unused]] Record const& incoming)
    {
        if constexpr (KeyedStreamRecord<Record>)
        {
            std::size_t const size = _ring.size();
            for (std::size_t index = 0; index < _count; ++index)
            {
                RecordPtr const& queued = _ring[(_head + index) % size];
                if (!queued || !(queued->Key == incoming.Key))
                    continue;
                for (std::size_t next = index + 1; next < _count; ++next)
                    _ring[(_head + next - 1) % size] = std::move(_ring[(_head + next) % size]);
                _ring[(_head + _count - 1) % size].reset();
                --_count;
                return;
            }
        }
    }

    Filter _filter;
    std::size_t _capacity;
    std::function<void()> _wake;
    StreamOverflow _overflow = StreamOverflow::DropOldest;
    mutable Ambrose::SpinLock _mutex;
    std::vector<RecordPtr> _ring;
    std::size_t _head = 0;
    std::size_t _count = 0;
    uint64 _droppedSincePop = 0;
    uint64 _droppedTotal = 0;
    bool _overflowed = false;
    std::atomic<bool> _closed{ false };
};

#endif
