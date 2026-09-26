/*
 * Project Ambrose by Imjustchico
 * Where a wizard stands while it plays, as its own client last said: a position and facing taken from each move the client sends while its zone counter matches the session's, the movement state it last reported, and one pending write the moment it has moved, taken when the wizard leaves the world rather than as it moves.
 */

#ifndef AMBROSE_PLAYERMOVEMENT_H
#define AMBROSE_PLAYERMOVEMENT_H

#include "Types.h"

#include <optional>

struct PlayerPosition
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Yaw = 0.0f;

    bool operator==(PlayerPosition const&) const = default;
};

enum class MoveResult : uint8
{
    Moved,
    StaleZone
};

class PlayerMovement
{
public:
    void Reset(PlayerPosition const& start, uint8 zoneCounter);

    MoveResult Apply(uint16 locationX, uint16 locationY, uint16 locationZ, uint8 direction, uint8 zoneCounter);
    void SetMoveState(int8 state) noexcept { _moveState = state; }
    void SetZoneCounter(uint8 zoneCounter) noexcept { _zoneCounter = zoneCounter; }

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
    uint64 _moves = 0;
    bool _moved = false;
};

#endif
