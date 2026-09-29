/*
 * Project Ambrose by Imjustchico
 * When the world flushes movement to the players who see it: the tick's time is added up, and a flush is due once it reaches the interval asked for at that tick, so an interval changed between two ticks applies from the next flush.
 */

#ifndef AMBROSE_MOVEFLUSHCLOCK_H
#define AMBROSE_MOVEFLUSHCLOCK_H

#include <chrono>

class MoveFlushClock
{
public:
    bool Advance(std::chrono::milliseconds diff, std::chrono::milliseconds interval) noexcept
    {
        _elapsed += diff;
        if (_elapsed < interval)
            return false;
        _elapsed = std::chrono::milliseconds::zero();
        return true;
    }

    void Reset() noexcept { _elapsed = std::chrono::milliseconds::zero(); }

private:
    std::chrono::milliseconds _elapsed{ 0 };
};

#endif
