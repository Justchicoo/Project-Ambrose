/*
 * Project Ambrose by Imjustchico
 * Reads a move with the client's own unpacking and ignores one sent under another zone counter, which the client stamps on every move and resets to 0 when it attaches, so a move from before a transfer never lands in the zone after it. However many moves arrive, one write is pending until it is taken, and a move that repeats the one before it is not a change.
 */

#include "PlayerMovement.h"
#include "MovementPacking.h"

#include <cmath>

void PlayerMovement::Reset(PlayerPosition const& start, uint8 zoneCounter)
{
    _position = start;
    _zoneCounter = zoneCounter;
    _moveState = 0;
    _moves = 0;
    _packed.reset();
    _changes = 0;
    _moved = false;
}

MoveResult PlayerMovement::Apply(uint16 locationX, uint16 locationY, uint16 locationZ, uint8 direction, uint8 zoneCounter)
{
    if (zoneCounter != _zoneCounter)
        return MoveResult::StaleZone;
    _position.X = MovementPacking::UnpackLocation(static_cast<int16>(locationX));
    _position.Y = MovementPacking::UnpackLocation(static_cast<int16>(locationY));
    _position.Z = MovementPacking::UnpackLocation(static_cast<int16>(locationZ));
    _position.Yaw = MovementPacking::UnpackYaw(direction);
    PackedMove const packed{ locationX, locationY, locationZ, direction };
    if (!_packed || *_packed != packed)
    {
        _packed = packed;
        ++_changes;
    }
    ++_moves;
    _moved = true;
    return MoveResult::Moved;
}

std::optional<PackedMove> PlayerMovement::Teleport(PlayerPosition const& destination) noexcept
{
    std::optional<int16> const x = MovementPacking::TryPackLocation(destination.X);
    std::optional<int16> const y = MovementPacking::TryPackLocation(destination.Y);
    std::optional<int16> const z = MovementPacking::TryPackLocation(destination.Z);
    if (!x || !y || !z || !std::isfinite(destination.Yaw))
        return std::nullopt;

    PackedMove const packed{ static_cast<uint16>(*x), static_cast<uint16>(*y), static_cast<uint16>(*z), MovementPacking::PackYaw(destination.Yaw) };
    _position = { MovementPacking::UnpackLocation(*x), MovementPacking::UnpackLocation(*y), MovementPacking::UnpackLocation(*z),
        MovementPacking::UnpackYaw(packed.Direction) };
    if (!_packed || *_packed != packed)
    {
        _packed = packed;
        ++_changes;
    }
    _moved = true;
    return packed;
}

std::optional<PlayerPosition> PlayerMovement::TakeWrite() noexcept
{
    if (!_moved)
        return std::nullopt;
    _moved = false;
    return _position;
}
