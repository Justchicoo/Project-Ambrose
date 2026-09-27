/*
 * Project Ambrose by Imjustchico
 * Takes the moves, movement states and jumps a client sends for its own wizard once it stands in an instance: valid moves and states are batched for other players, stale-zone moves are logged and ignored, and a jump is relayed according to ExcludeOriginator; a message that arrives before the wizard has an instance, or after it has left one, has no wizard to move.
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
    if (!_mapId)
        return;
    QueueMapBroadcast(message.ExcludeOriginator == 0, [message](GameSession& recipient) { recipient.SendDmlMessage(message); });
    LOG_DEBUG("server.gamesession", "Session {}'s wizard {} jumped{}", GetSessionId(), _worldGuid, message.ExcludeOriginator != 0 ? ", shown to others only" : "");
}
