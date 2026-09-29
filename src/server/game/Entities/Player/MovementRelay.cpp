/*
 * Project Ambrose by Imjustchico
 * Relays changed moves and states, idles a quiet mover after its configured flush count, shows the last move to newcomers, and forces a link-dead wizard to stand immediately.
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

MovementUpdate MovementRelay::Stop() noexcept
{
    _idle = true;
    _quietFlushes = 0;
    MovementUpdate update;
    if (_sentState != Standing)
    {
        update.State = Standing;
        _sentState = Standing;
    }
    return update;
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
