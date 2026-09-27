/*
 * Project Ambrose by Imjustchico
 * Reads a move with the client's own unpacking and ignores one sent under another zone counter, which the client stamps on every move and resets to 0 when it attaches, so a move from before a transfer never lands in the zone after it. However many moves arrive, one write is pending until it is taken.
 */

#include "PlayerMovement.h"
#include "MovementPacking.h"

void PlayerMovement::Reset(PlayerPosition const& start, uint8 zoneCounter, Clock::time_point now)
{
    _position = start;
    _zoneCounter = zoneCounter;
    _moveState = 0;
    _packedMove = {};
    _moves = 0;
    _broadcastMoves = 0;
    _idleFlushes = 0;
    _lastFlush = now;
    _moveStateDirty = false;
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
    _packedMove = { locationX, locationY, locationZ, direction };
    ++_moves;
    _moved = true;
    return MoveResult::Moved;
}

void PlayerMovement::SetMoveState(int8 state) noexcept
{
    if (_moveState == state)
        return;
    _moveState = state;
    _idleFlushes = 0;
    _moveStateDirty = true;
}

std::optional<PlayerMovementBroadcast> PlayerMovement::Flush(Clock::time_point now, std::chrono::milliseconds interval, uint32 idleIntervals)
{
    if (now - _lastFlush < interval)
        return std::nullopt;
    _lastFlush = now;

    PlayerMovementBroadcast broadcast;
    if (_moves != _broadcastMoves)
    {
        broadcast.Move = _packedMove;
        broadcast.NewState = _moveState;
        _broadcastMoves = _moves;
        _moveStateDirty = false;
        _idleFlushes = 0;
        return broadcast;
    }

    if (_moveStateDirty)
    {
        broadcast.NewState = _moveState;
        _moveStateDirty = false;
        _idleFlushes = 0;
        return broadcast;
    }

    if (_moveState != 0 && ++_idleFlushes >= idleIntervals)
    {
        _moveState = 0;
        broadcast.NewState = _moveState;
        _idleFlushes = 0;
    }
    return broadcast;
}

std::optional<PlayerPosition> PlayerMovement::TakeWrite() noexcept
{
    if (!_moved)
        return std::nullopt;
    _moved = false;
    return _position;
}
