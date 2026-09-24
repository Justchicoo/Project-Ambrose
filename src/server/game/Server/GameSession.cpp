/*
 * Project Ambrose by Imjustchico
 * Draining is what this adds to a session, and dispatching what it can answer: the world calls DrainQueue on its own thread and the queued work runs there, bounded so one talkative client cannot hold the tick, and every handler is written knowing it runs on that thread and nowhere else. MSG_ATTACH is taken as soon as a client connects, because a client that has not attached has nothing else to say, and it is read and reported here while the key it carries waits for 4.07 to judge it.
 */

#include "GameSession.h"
#include "Frame.h"
#include "GameMessageTable.h"
#include "Log.h"
#include "MessageRegistry.h"
#include "StringUtil.h"

#include <utility>

GameSession::GameSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context)
    : SessionBase(std::move(socket), limits, std::move(context))
{
}

std::size_t GameSession::DrainQueue(std::size_t limit)
{
    return ProcessQueuedMessages(limit);
}

void GameSession::OnMessage(DmlMessageData& message)
{
    DispatchResult const result = GameMessageTable::Get().Dispatch(*this, sMessageRegistry.GetCatalog(), message);
    if (result != DispatchResult::NotHandled && result != DispatchResult::UnknownMessage)
        return;
    _unhandled.fetch_add(1, std::memory_order_relaxed);
    LOG_DEBUG("server.gamesession", "Session {} sent service {} order {}, which the game server has no handler for yet",
        GetSessionId(), message.ServiceId, message.Order);
}

void GameSession::HandleAttach(GameMessages::Attach& message)
{
    SetAccountId(message.UserId);
    SetCharacterId(message.CharId);
    LOG_INFO("server.gamesession", "Session {} from {} attached as account {} with wizard {} for zone {} at {}, on a key of {} character(s)",
        GetSessionId(), GetRemoteAddress().to_string(), message.UserId, message.CharId, Ambrose::ForLog(message.ZoneName, 128),
        Ambrose::ForLog(message.Location, 64), message.LoginKey.size());
}
