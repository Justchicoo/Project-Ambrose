/*
 * Project Ambrose by Imjustchico
 * Refuses a Go Home use until the live cooldown expires without moving the last-use time on refusal.
 */

#include "GoHomeCooldown.h"

bool GoHomeCooldown::TryUse(Clock::time_point now, std::chrono::seconds cooldown) noexcept
{
    if (!CanUse(now, cooldown))
        return false;
    RecordUse(now);
    return true;
}

bool GoHomeCooldown::CanUse(Clock::time_point now, std::chrono::seconds cooldown) const noexcept
{
    if (!_lastUse)
        return true;
    if (now < *_lastUse)
        return false;
    return now - *_lastUse >= cooldown;
}
