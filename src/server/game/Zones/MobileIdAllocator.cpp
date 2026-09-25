/*
 * Project Ambrose by Imjustchico
 * Hands out mobile ids from the front of each range's free line and puts a released one into a cooling line ordered by when it may come back, moving it to the back of its range's free line only once its delay has passed. Releasing an id that is not held is refused rather than letting the same id sit in the free line twice, which would hand it to two objects at once.
 */

#include "MobileIdAllocator.h"

#include <iterator>

namespace
{
    std::size_t IndexOf(MobileIdAllocator::Range range) noexcept
    {
        return static_cast<std::size_t>(range);
    }
}

MobileIdAllocator::MobileIdAllocator() : _held(std::size_t{ 1 } << 16, false)
{
    for (uint32 id = FirstObjectId; id <= LastObjectId; ++id)
        _free[IndexOf(Range::Object)].push_back(static_cast<uint16>(id));
    for (uint32 id = FirstPlayerId; id <= LastPlayerId; ++id)
        _free[IndexOf(Range::Player)].push_back(static_cast<uint16>(id));
}

MobileIdAllocator::Range MobileIdAllocator::RangeOf(uint16 id) noexcept
{
    return id >= FirstPlayerId ? Range::Player : Range::Object;
}

bool MobileIdAllocator::IsUsable(uint16 id) noexcept
{
    return (id >= FirstObjectId && id <= LastObjectId) || (id >= FirstPlayerId && id <= LastPlayerId);
}

void MobileIdAllocator::Thaw(Clock::time_point now)
{
    while (!_cooling.empty() && _cooling.front().second <= now)
    {
        uint16 const id = _cooling.front().first;
        _cooling.pop_front();
        _free[IndexOf(RangeOf(id))].push_back(id);
    }
}

std::optional<uint16> MobileIdAllocator::Allocate(Range range, Clock::time_point now)
{
    Thaw(now);
    std::deque<uint16>& free = _free[IndexOf(range)];
    if (free.empty())
        return std::nullopt;
    uint16 const id = free.front();
    free.pop_front();
    _held[id] = true;
    ++_heldCount[IndexOf(range)];
    return id;
}

bool MobileIdAllocator::Release(uint16 id, Clock::time_point now, std::chrono::milliseconds delay)
{
    if (!IsUsable(id) || !_held[id])
        return false;
    _held[id] = false;
    --_heldCount[IndexOf(RangeOf(id))];
    Clock::time_point const back = now + delay;
    if (!_cooling.empty() && back < _cooling.back().second)
    {
        auto at = _cooling.end();
        while (at != _cooling.begin() && std::prev(at)->second > back)
            --at;
        _cooling.insert(at, { id, back });
        return true;
    }
    _cooling.emplace_back(id, back);
    return true;
}

bool MobileIdAllocator::IsHeld(uint16 id) const noexcept
{
    return _held[id];
}

std::size_t MobileIdAllocator::Held(Range range) const noexcept
{
    return _heldCount[IndexOf(range)];
}

std::size_t MobileIdAllocator::Cooling() const noexcept
{
    return _cooling.size();
}
