/*
 * Project Ambrose by Imjustchico
 * Allows Go Home presses no sooner than a caller-supplied live cooldown after the last accepted press, using a monotonic clock and recording only accepted uses.
 */

#ifndef AMBROSE_GOHOMECOOLDOWN_H
#define AMBROSE_GOHOMECOOLDOWN_H

#include <chrono>
#include <optional>

class GoHomeCooldown
{
public:
    using Clock = std::chrono::steady_clock;

    bool CanUse(Clock::time_point now, std::chrono::seconds cooldown) const noexcept;
    void RecordUse(Clock::time_point now) noexcept { _lastUse = now; }
    bool TryUse(Clock::time_point now, std::chrono::seconds cooldown) noexcept;
    void Clear() noexcept { _lastUse.reset(); }

private:
    std::optional<Clock::time_point> _lastUse;
};

#endif
