/*
 * Project Ambrose by Imjustchico
 * Where a wizard stands while it plays, with its packed client movement pending for the next live-configured flush, its movement state relayed alongside movement, and idle animation stopped after the configured number of empty flush intervals, while one independent position write remains pending until the wizard leaves the world.
 */

#ifndef AMBROSE_PLAYERMOVEMENT_H
#define AMBROSE_PLAYERMOVEMENT_H

#include "Types.h"

#include <chrono>
#include <optional>

struct PlayerPosition
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Yaw = 0.0f;

    bool operator==(PlayerPosition const&) const = default;
};

struct PackedPlayerMove
{
    uint16 LocationX = 0;
    uint16 LocationY = 0;
    uint16 LocationZ = 0;
    uint8 Direction = 0;

    bool operator==(PackedPlayerMove const&) const = default;
};

struct PlayerMovementBroadcast
{
    std::optional<PackedPlayerMove> Move;
    std::optional<int8> NewState;
};

enum class MoveResult : uint8
{
    Moved,
    StaleZone
};

class PlayerMovement
{
public:
    using Clock = std::chrono::steady_clock;

    void Reset(PlayerPosition const& start, uint8 zoneCounter, Clock::time_point now = Clock::now());

    MoveResult Apply(uint16 locationX, uint16 locationY, uint16 locationZ, uint8 direction, uint8 zoneCounter);
    void SetMoveState(int8 state) noexcept;
    void SetZoneCounter(uint8 zoneCounter) noexcept { _zoneCounter = zoneCounter; }
    std::optional<PlayerMovementBroadcast> Flush(Clock::time_point now, std::chrono::milliseconds interval, uint32 idleIntervals);

    PlayerPosition const& GetPosition() const noexcept { return _position; }
    uint8 GetZoneCounter() const noexcept { return _zoneCounter; }
    int8 GetMoveState() const noexcept { return _moveState; }
    uint64 GetMoves() const noexcept { return _moves; }
    bool HasMoved() const noexcept { return _moved; }

    std::optional<PlayerPosition> TakeWrite() noexcept;

private:
    PlayerPosition _position;
    uint8 _zoneCounter = 0;
    int8 _moveState = 0;
    PackedPlayerMove _packedMove;
    uint64 _moves = 0;
    uint64 _broadcastMoves = 0;
    uint32 _idleFlushes = 0;
    Clock::time_point _lastFlush{};
    bool _moveStateDirty = false;
    bool _moved = false;
};

#endif
