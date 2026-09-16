/*
 * Project Ambrose by Imjustchico
 * Dispatches each client message against the live message catalog, derives the login salt from the session's offer, runs database callbacks on the session's own network thread when the database signals their results, fails an attempt whose callback was lost, and releases the session's account claim when it closes.
 */

#include "LoginSession.h"
#include "Log.h"
#include "LoginMessageTable.h"
#include "LoginMgr.h"
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

void LoginSession::OnMessage(DmlMessageData& message)
{
    LoginMessageTable::Get().Dispatch(*this, sMessageRegistry.GetCatalog(), message);
}

void LoginSession::OnSessionClosed()
{
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
    if (_authenticating && _queryCallbacks.GetPendingCount() == 0 && _transactionCallbacks.GetPendingCount() == 0)
    {
        ReleaseClaim();
        FailAuthentication(nullptr, AuthResult::Timeout, "its database callback was lost", false, true);
    }
}

std::shared_ptr<LoginSession> LoginSession::SharedSelf()
{
    return std::static_pointer_cast<LoginSession>(shared_from_this());
}
