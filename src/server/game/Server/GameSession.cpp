/*
 * Project Ambrose by Imjustchico
 * Draining is what this adds to a session, and dispatching what it can answer: the world calls DrainQueue on its own thread and the queued work runs there, bounded so one talkative client cannot hold the tick, and every handler is written knowing it runs on that thread and nowhere else. MSG_ATTACH is taken as soon as a client connects, because a client that has not attached has nothing else to say, and the key it carries is spent before the client is let in: the spend is one conditional update, so two clients holding the same key cannot both win it, and a spend that changed no row is read back only to say why, because the reason a client was turned away is worth knowing while the reason it was let in is not. A refused attach is told once and the socket closed behind it, and a session that was let in gives its wizard back when it goes.
 */

#include "GameSession.h"
#include "Frame.h"
#include "GameMessageTable.h"
#include "Log.h"
#include "MessageRegistry.h"
#include "StringUtil.h"

#include <chrono>
#include <utility>

namespace
{
    std::atomic<uint32> RealmId{ 0 };

    int64 NowEpochSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

GameSession::GameSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context)
    : SessionBase(std::move(socket), limits, std::move(context))
{
}

void GameSession::SetRealmId(uint32 realmId) noexcept
{
    RealmId.store(realmId, std::memory_order_relaxed);
}

uint32 GameSession::GetRealmId() noexcept
{
    return RealmId.load(std::memory_order_relaxed);
}

std::shared_ptr<GameSession> GameSession::SharedSelf()
{
    return std::static_pointer_cast<GameSession>(shared_from_this());
}

std::size_t GameSession::DrainQueue(std::size_t limit)
{
    return ProcessQueuedMessages(limit);
}

void GameSession::ProcessCallbacks()
{
    _countedCallbacks.ProcessReadyCallbacks();
    _queryCallbacks.ProcessReadyCallbacks();
}

SQLOperation::CompletionHandler GameSession::MakeCompletionHandler()
{
    return [weak = std::weak_ptr<GameSession>(SharedSelf()), executor = GetExecutor()]
    {
        asio::post(executor, [weak]
        {
            if (std::shared_ptr<GameSession> const session = weak.lock())
                session->ProcessCallbacks();
        });
    };
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

void GameSession::OnSessionClosed()
{
    if (_attached.exchange(false, std::memory_order_relaxed))
    {
        LoginKeyClaim claim;
        claim.AccountId = GetAccountId();
        claim.CharacterId = GetCharacterId();
        claim.RealmId = GetRealmId();
        LoginKeyValidator::MarkOffline(claim);
    }
    _countedCallbacks.Clear();
    _queryCallbacks.Clear();
    SessionBase::OnSessionClosed();
}

void GameSession::HandleAttach(GameMessages::Attach& message)
{
    LoginKeyClaim claim;
    claim.Key = message.LoginKey;
    claim.AccountId = message.UserId;
    claim.CharacterId = message.CharId;
    claim.RealmId = GetRealmId();

    LOG_INFO("server.gamesession", "Session {} from {} is attaching as account {} with wizard {} for zone {} at {}, on a key of {} character(s)",
        GetSessionId(), GetRemoteAddress().to_string(), message.UserId, message.CharId, Ambrose::ForLog(message.ZoneName, 128),
        Ambrose::ForLog(message.Location, 64), message.LoginKey.size());

    if (_attaching.exchange(true, std::memory_order_relaxed))
    {
        RefuseAttach(claim, LoginKeyVerdict::AlreadyUsed);
        return;
    }

    int64 const now = NowEpochSeconds();
    std::optional<CountedCallback> consume = LoginKeyValidator::BeginConsume(claim, now, MakeCompletionHandler());
    if (!consume)
    {
        RefuseAttach(claim, LoginKeyVerdict::Unavailable);
        return;
    }

    _countedCallbacks.AddCallback(std::move(*consume).AfterComplete([this, claim, now](std::optional<uint64> affected)
    {
        if (!IsOpen() || IsKicked())
            return;
        if (!affected)
        {
            RefuseAttach(claim, LoginKeyVerdict::Unavailable);
            return;
        }
        if (*affected == 1)
        {
            AcceptAttach(claim);
            return;
        }
        Diagnose(claim, now);
    }));
}

void GameSession::Diagnose(LoginKeyClaim claim, int64 now)
{
    std::optional<QueryCallback> diagnose = LoginKeyValidator::BeginDiagnose(claim.Key, MakeCompletionHandler());
    if (!diagnose)
    {
        RefuseAttach(claim, LoginKeyVerdict::Unavailable);
        return;
    }
    _queryCallbacks.AddCallback(std::move(*diagnose).WithPreparedCallback([this, claim, now](PreparedQueryResult result)
    {
        if (!IsOpen() || IsKicked())
            return;
        RefuseAttach(claim, LoginKeyValidator::Classify(LoginKeyValidator::ReadRecord(result), claim, now));
    }));
}

void GameSession::AcceptAttach(LoginKeyClaim const& claim)
{
    SetAccountId(claim.AccountId);
    SetCharacterId(claim.CharacterId);
    _attached.store(true, std::memory_order_relaxed);
    SetStatus(SessionStatus::Authenticated);
    LoginKeyValidator::MarkOnline(claim);
    LOG_INFO("server.gamesession", "Session {} attached: account {} with wizard {} on realm {}, its key accepted and spent",
        GetSessionId(), claim.AccountId, claim.CharacterId, claim.RealmId);
}

void GameSession::RefuseAttach(LoginKeyClaim const& claim, LoginKeyVerdict verdict)
{
    LOG_WARN("server.gamesession", "Session {} from {} was refused as account {} with wizard {}: {}",
        GetSessionId(), GetRemoteAddress().to_string(), claim.AccountId, claim.CharacterId, LoginKeyValidator::Describe(verdict));
    GameMessages::AttachFailed failed;
    failed.Error = 1;
    failed.Rejected = 1;
    failed.NoDisconnect = 0;
    SendDmlMessageDelayedClose(failed);
}
