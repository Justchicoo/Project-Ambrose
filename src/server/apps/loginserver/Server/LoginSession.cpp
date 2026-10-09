/*
 * Project Ambrose by Imjustchico
 * Dispatches client messages, posts admission queue changes on the session executor, checks the AFK and handoff timers, runs database callbacks on the session's network thread, fails lost callbacks and releases the account claim and queue entry when the session closes.
 */

#include "LoginSession.h"
#include "AdmissionQueue.h"
#include "Log.h"
#include "LoginMessageTable.h"
#include "LoginMgr.h"
#include "LoginSettings.h"
#include "MessageRegistry.h"

#include <asio/post.hpp>

LoginSession::LoginSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context)
    : SessionBase(std::move(socket), limits, std::move(context))
{
}

LoginSalt LoginSession::GetLoginSalt() const noexcept
{
    SessionTimestamp const offer = GetOfferTime();
    return LoginSalt{ GetSessionId(), static_cast<uint32>(offer.GetSeconds()), offer.Milliseconds };
}

void LoginSession::OnAccepted()
{
    MarkActivity();
}

void LoginSession::OnMessage(DmlMessageData& message)
{
    MarkActivity();
    LoginMessageTable::Get().Dispatch(*this, sMessageRegistry.GetCatalog(), message);
}

void LoginSession::HandleLoginNotAfk(LoginMessages::LoginNotAfk&)
{
}

void LoginSession::NotifyAdmissionQueuePosition(LoginMessages::CharacterSelected reply)
{
    std::shared_ptr<LoginSession> self = SharedSelf();
    asio::post(GetExecutor(), [self = std::move(self), reply = std::move(reply)]
    {
        if (self->IsOpen() && !self->IsKicked() && self->GetStatus() == SessionStatus::Authenticated)
            self->SendDmlMessage(reply);
    });
}

void LoginSession::AdmitQueuedCharacter(LoginMessages::CharacterSelected reply, std::string realmName, uint32 realmId)
{
    std::shared_ptr<LoginSession> self = SharedSelf();
    asio::post(GetExecutor(), [self = std::move(self), reply = std::move(reply), realmName = std::move(realmName), realmId]
    {
        if (!self->IsOpen() || self->IsKicked() || self->GetStatus() != SessionStatus::Authenticated)
        {
            sAdmissionQueue.CancelReservation(realmId, reply.CharId);
            return;
        }
        self->IssueCharacterSelected(std::move(reply), std::move(realmName), realmId);
    });
}

void LoginSession::FailQueuedCharacter(uint64 characterGuid, std::string detail)
{
    std::shared_ptr<LoginSession> self = SharedSelf();
    asio::post(GetExecutor(), [self = std::move(self), characterGuid, detail = std::move(detail)]
    {
        if (self->IsOpen() && !self->IsKicked() && self->GetStatus() == SessionStatus::Authenticated)
            self->FailCharacterSelect(characterGuid, detail);
    });
}

void LoginSession::HandleRequestServerList(LoginMessages::RequestServerList&)
{
    LoginMessages::ServerList list;
    SendDmlMessage(list);
}

bool LoginSession::SendShutdownNotice(uint32 message)
{
    LoginMessages::LoginServerShutdown notice;
    notice.Message = message;
    return SendDmlMessageDelayedClose(notice);
}

void LoginSession::MarkActivity() noexcept
{
    _lastActivity.store(sLoginMgr.Now().time_since_epoch().count(), std::memory_order_relaxed);
}

void LoginSession::Update()
{
    if (GetState() != SessionState::Accepted || IsKicked())
        return;
    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    if (now < _nextAfkCheck)
        return;
    _nextAfkCheck = now + AfkCheckInterval;
    CheckAfk();
    CheckHandoff();
    _afkChecks.fetch_add(1, std::memory_order_relaxed);
}

void LoginSession::CheckAfk()
{
    std::shared_ptr<LoginSettings const> const settings = sLoginMgr.GetSettings();
    SessionStatus const status = GetStatus();
    if (settings->AfkTimeout.count() > 0 && !_authenticating && (status == SessionStatus::Connected || status == SessionStatus::Authenticated))
    {
        std::chrono::steady_clock::duration const idle = sLoginMgr.Now() - GetLastActivity();
        if (idle >= settings->AfkTimeout)
        {
            LOG_INFO("server.loginserver", "Session {} from {} was idle for {} s, reaching Login.AfkTimeout of {} s; sent MSG_DISCONNECT_LOGIN_AFK Warning={} and closed the session",
                GetSessionId(), GetRemoteAddress().to_string(), std::chrono::duration_cast<std::chrono::seconds>(idle).count(), settings->AfkTimeout.count(), settings->AfkWarning);
            LoginMessages::DisconnectLoginAfk message;
            message.Warning = settings->AfkWarning;
            SendDmlMessageDelayedClose(message);
        }
    }
}

void LoginSession::CheckHandoff()
{
    if (GetStatus() != SessionStatus::CharacterSelected)
        return;
    std::chrono::steady_clock::rep const started = _handoffAt.load(std::memory_order_relaxed);
    if (started == 0)
        return;
    std::chrono::milliseconds const grace = GetContext().GetSettings().HandoffGrace;
    if (grace.count() <= 0)
        return;
    std::chrono::steady_clock::duration const waited = sLoginMgr.Now() - std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(started));
    if (waited < grace)
        return;
    LOG_INFO("server.loginserver", "Session {} was sent to a gameserver {} ms ago and has not closed, reaching Network.HandoffGrace of {} ms; closing it",
        GetSessionId(), std::chrono::duration_cast<std::chrono::milliseconds>(waited).count(), grace.count());
    _handoffAt.store(0, std::memory_order_relaxed);
    CloseSocket();
}

void LoginSession::OnSessionClosed()
{
    sAdmissionQueue.RemoveSession(this);
    if (_claimedAccountId != 0)
        LOG_DEBUG("server.loginserver", "Session {} released account {} (id {})", GetSessionId(), _accountName.empty() ? std::string("not yet admitted") : _accountName, _claimedAccountId);
    ReleaseClaim();
}

void LoginSession::ReleaseClaim()
{
    if (_claimedAccountId == 0)
        return;
    sLoginMgr.ReleaseAccount(_claimedAccountId, this);
    _claimedAccountId = 0;
}

SQLOperation::CompletionHandler LoginSession::MakeCompletionHandler()
{
    return [weak = std::weak_ptr<LoginSession>(SharedSelf()), executor = GetExecutor()]
    {
        asio::post(executor, [weak]
        {
            if (std::shared_ptr<LoginSession> const session = weak.lock())
                session->ProcessCallbacks();
        });
    };
}

void LoginSession::ProcessCallbacks()
{
    _queryCallbacks.ProcessReadyCallbacks();
    _transactionCallbacks.ProcessReadyCallbacks();
    _countedCallbacks.ProcessReadyCallbacks();
    if (_authenticating && _queryCallbacks.GetPendingCount() == 0 && _transactionCallbacks.GetPendingCount() == 0)
    {
        ReleaseClaim();
        FailAuthentication(nullptr, AuthResult::Timeout, "its database callback was lost", false, true);
    }
    if (_listingCharacters && _queryCallbacks.GetPendingCount() == 0)
        FailCharacterList("its database callback was lost");
}

std::shared_ptr<LoginSession> LoginSession::SharedSelf()
{
    return std::static_pointer_cast<LoginSession>(shared_from_this());
}
