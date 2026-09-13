/*
 * Project Ambrose by Imjustchico
 * Monotonic millisecond clock with wrap-safe differences, plus interval and countdown timers.
 */

#ifndef AMBROSE_TIMER_H
#define AMBROSE_TIMER_H

#include "Duration.h"
#include "Types.h"

#include <chrono>

inline std::chrono::steady_clock::time_point GetApplicationStartTime()
{
    static std::chrono::steady_clock::time_point const start = std::chrono::steady_clock::now();
    return start;
}

inline uint32 GetMSTime()
{
    auto const elapsed = std::chrono::duration_cast<Milliseconds>(std::chrono::steady_clock::now() - GetApplicationStartTime());
    return static_cast<uint32>(elapsed.count());
}

inline constexpr uint32 GetMSTimeDiff(uint32 oldMSTime, uint32 newMSTime)
{
    return newMSTime - oldMSTime;
}

inline uint32 GetMSTimeDiffToNow(uint32 oldMSTime)
{
    return GetMSTimeDiff(oldMSTime, GetMSTime());
}

class IntervalTimer
{
public:
    constexpr IntervalTimer() = default;

    constexpr explicit IntervalTimer(int64 interval) : _interval(interval) { }

    constexpr void Update(int64 diff)
    {
        _current += diff;
        if (_current < 0)
            _current = 0;
    }

    constexpr bool Passed() const
    {
        return _current >= _interval;
    }

    constexpr void Reset()
    {
        if (_interval > 0 && _current >= _interval)
            _current %= _interval;
    }

    constexpr void SetCurrent(int64 current)
    {
        _current = current;
    }

    constexpr void SetInterval(int64 interval)
    {
        _interval = interval;
    }

    constexpr int64 GetInterval() const
    {
        return _interval;
    }

    constexpr int64 GetCurrent() const
    {
        return _current;
    }

private:
    int64 _interval = 0;
    int64 _current = 0;
};

class TimeTracker
{
public:
    constexpr TimeTracker() = default;

    constexpr explicit TimeTracker(Milliseconds expiry) : _expiry(expiry) { }

    constexpr void Update(Milliseconds diff)
    {
        _expiry -= diff;
    }

    constexpr bool Passed() const
    {
        return _expiry <= Milliseconds::zero();
    }

    constexpr void Reset(Milliseconds expiry)
    {
        _expiry = expiry;
    }

    constexpr Milliseconds GetExpiry() const
    {
        return _expiry;
    }

private:
    Milliseconds _expiry{0};
};

#endif
