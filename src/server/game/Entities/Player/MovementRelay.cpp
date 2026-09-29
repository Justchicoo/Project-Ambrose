/*
 * Project Ambrose by Imjustchico
 * Counts a flush with no new move as a quiet one, and once the quiet flushes reach the number asked for the wizard is shown as idle, standing whatever its client last said, until a new move or a new state from its client ends it; the state shown is compared with the one last sent, so standing goes out once and the client's own state goes out again when the idle ends. What a wizard who has just arrived is shown of another is that other's last move and, when it is not shown standing, its state.
 */

#include "MovementRelay.h"

void MovementRelay::Reset(PlayerMovement const& movement) noexcept
{
    _sentChanges = movement.GetChanges();
    _sentState = movement.GetMoveState();
    _saidState = movement.GetMoveState();
    _quietFlushes = 0;
    _idle = false;
}

int8 MovementRelay::ShownState(PlayerMovement const& movement) const noexcept
{
    return _idle ? Standing : movement.GetMoveState();
}

MovementUpdate MovementRelay::Take(PlayerMovement const& movement, uint32 idleFlushes)
{
    MovementUpdate update;
    if (movement.GetMoveState() != _saidState)
    {
        _saidState = movement.GetMoveState();
        _quietFlushes = 0;
        _idle = false;
    }
    if (movement.GetChanges() != _sentChanges && movement.GetPackedMove())
    {
        update.Move = *movement.GetPackedMove();
        _sentChanges = movement.GetChanges();
        _quietFlushes = 0;
        _idle = false;
    }
    else if (_quietFlushes < idleFlushes)
        ++_quietFlushes;
    if (!update.Move && _quietFlushes >= idleFlushes)
        _idle = true;
    int8 const shown = ShownState(movement);
    if (shown != _sentState)
    {
        update.State = shown;
        _sentState = shown;
    }
    return update;
}

MovementUpdate MovementRelay::Current(PlayerMovement const& movement) const
{
    MovementUpdate update;
    update.Move = movement.GetPackedMove();
    if (int8 const shown = ShownState(movement); shown != Standing)
        update.State = shown;
    return update;
}
