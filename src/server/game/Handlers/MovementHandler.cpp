/*
 * Project Ambrose by Imjustchico
 * Takes the moves, movement states and jumps a client sends for its own wizard once it stands in an instance: a move becomes where the wizard stands unless the client sent it under another zone counter, which is logged and ignored, a movement state is kept for the players who see it, handed to them at the next flush with the move, and a jump is kept for the world to tell the other wizards in the instance at its next tick, because the client handles no MSG_JUMP from the server and plays another wizard's jump when told that wizard's object entered its jumping state, the jumper's own client too when it did not ask to be left out; a message that arrives before the wizard has an instance, or after it has left one, has no wizard to move.
 */

#include "GameSession.h"
#include "Log.h"

void GameSession::HandleClientMove(GameMessages::ClientMove& message)
{
    if (!_mapId)
        return;
    if (_movement.Apply(message.LocationX, message.LocationY, message.LocationZ, message.Direction, message.ZoneCounter) == MoveResult::StaleZone)
        LOG_DEBUG("server.gamesession", "Session {} sent a move under zone counter {} while its wizard's is {}; the move is ignored", GetSessionId(), message.ZoneCounter,
            _movement.GetZoneCounter());
}

void GameSession::HandleClientMoveState(GameMessages::ClientMoveState& message)
{
    if (!_mapId)
        return;
    _movement.SetMoveState(message.NewState);
}

void GameSession::HandleJump(GameMessages::Jump& message)
{
    if (!_mapId || _publicObject.empty())
        return;
    _jump = message.ExcludeOriginator;
}
