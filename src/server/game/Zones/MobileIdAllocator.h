/*
 * Project Ambrose by Imjustchico
 * The sixteen-bit mobile ids one zone instance hands out, the number the client knows an object by while it is in view: a low range for the objects the zone places and an upper range for players, zero and the last value never used. A released id waits out its delay before it can be handed out again, and after that it goes to the back of the line rather than the front, so the id the client most recently saw leave is the one it will see come back last; handing a fast leave-and-rejoin the id that just left is how a new object's arrival races the old one's removal. Running out of a range is an answer the caller can act on, never a crash.
 */

#ifndef AMBROSE_MOBILEIDALLOCATOR_H
#define AMBROSE_MOBILEIDALLOCATOR_H

#include "Types.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

class MobileIdAllocator
{
public:
    using Clock = std::chrono::steady_clock;

    enum class Range : uint8
    {
        Object = 0,
        Player = 1
    };

    static constexpr uint16 FirstObjectId = 1;
    static constexpr uint16 LastObjectId = 0xBFFF;
    static constexpr uint16 FirstPlayerId = 0xC000;
    static constexpr uint16 LastPlayerId = 0xFFFE;

    MobileIdAllocator();

    std::optional<uint16> Allocate(Range range, Clock::time_point now);
    bool Release(uint16 id, Clock::time_point now, std::chrono::milliseconds delay);

    bool IsHeld(uint16 id) const noexcept;
    std::size_t Held(Range range) const noexcept;
    std::size_t Cooling() const noexcept;

    static Range RangeOf(uint16 id) noexcept;
    static bool IsUsable(uint16 id) noexcept;

private:
    void Thaw(Clock::time_point now);

    std::vector<bool> _held;
    std::array<std::deque<uint16>, 2> _free;
    std::deque<std::pair<uint16, Clock::time_point>> _cooling;
    std::array<std::size_t, 2> _heldCount{};
};

#endif
