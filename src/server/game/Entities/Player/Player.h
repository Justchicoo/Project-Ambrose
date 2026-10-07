/*
 * Project Ambrose by Imjustchico
 * A live wizard whose stats change on the world thread, whose potion refill is timed there, and whose dirty character stats are saved by its session.
 */

#ifndef AMBROSE_PLAYER_H
#define AMBROSE_PLAYER_H

#include "PlayerStats.h"

#include <chrono>
#include <optional>
#include <utility>

class Player
{
public:
    using Clock = std::chrono::steady_clock;

    explicit Player(PlayerStats stats) : _stats(std::move(stats)) {}

    PlayerStats const& GetStats() const noexcept { return _stats; }
    bool HasDirtyStats() const noexcept { return _dirtyStats; }
    void ClearDirtyStats() noexcept { _dirtyStats = false; }

    bool SetHealth(int32 value) noexcept;
    bool SetMana(int32 value) noexcept;
    bool SetGold(int64 value) noexcept;
    int64 ModifyGold(int64 amount) noexcept;
    bool SetPotions(float charge, float maximum) noexcept;
    bool SetPotionCapacity(uint32 capacity) noexcept;
    bool UsePotion(double restoreFraction, Clock::time_point now, std::chrono::seconds refillInterval) noexcept;
    bool RefillPotion(Clock::time_point now, std::chrono::seconds refillInterval) noexcept;
    bool SetPowerPip(float value) noexcept;
    bool SetShadowPipRating(float value) noexcept;

private:
    PlayerStats _stats;
    std::optional<Clock::time_point> _nextPotionRefill;
    bool _dirtyStats = false;
};

#endif
