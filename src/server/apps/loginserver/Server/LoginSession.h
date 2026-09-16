/*
 * Project Ambrose by Imjustchico
 * The login server's session: routes every client message through the login message table, authenticates MSG_USER_AUTHEN_V3 against the login database without blocking its network thread, holds the account it claimed and admitted, drops the client once it idles past the AFK timeout before choosing a character, and tells it when the login server shuts down.
 */

#ifndef AMBROSE_LOGINSESSION_H
#define AMBROSE_LOGINSESSION_H

#include "AsyncCallbackProcessor.h"
#include "AuthResult.h"
#include "LoginMessages.h"
#include "LoginSalt.h"
#include "SQLOperation.h"
#include "SessionBase.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

class LoginSession : public SessionBase
{
public:
    static constexpr uint32 DisconnectLoggedInElsewhere = 1;
    static constexpr std::size_t MaxRec1Bytes = 512;
    static constexpr std::chrono::seconds AfkCheckInterval{ 1 };

    LoginSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context);

    uint64 GetAccountId() const noexcept { return _accountId.load(std::memory_order_relaxed); }
    LoginSalt GetLoginSalt() const noexcept;
    std::chrono::steady_clock::time_point GetLastActivity() const noexcept { return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(_lastActivity.load(std::memory_order_relaxed))); }
    uint64 GetAfkCheckCount() const noexcept { return _afkChecks.load(std::memory_order_relaxed); }
    bool SendShutdownNotice(uint32 message);
    void Update() override;

    void HandleUserAuthenV3(LoginMessages::UserAuthenV3& message);
    void HandleUserAuthen(LoginMessages::UserAuthen& message);
    void HandleUserAuthenV2(LoginMessages::UserAuthenV2& message);
    void HandleWebAuthen(LoginMessages::WebAuthen& message);
    void HandleWebValidate(LoginMessages::WebValidate& message);
    void HandleLoginNotAfk(LoginMessages::LoginNotAfk& message);

protected:
    void OnAccepted() override;
    void OnMessage(DmlMessageData& message) override;
    void OnSessionClosed() override;

private:
    struct AuthAttempt;

    void ContinueAuthentication(std::shared_ptr<AuthAttempt> const& attempt, PreparedQueryResult result);
    void CompleteAuthentication(std::shared_ptr<AuthAttempt> const& attempt, bool committed);
    void FailAuthentication(AuthAttempt* attempt, AuthResult result, std::string_view detail, bool countsAsGuess, bool close = false);
    void AbortAuthentication(AuthAttempt* attempt, std::exception const& failure);
    void RefuseUnsupportedAuthentication(std::string_view tag);
    void ReleaseClaim();
    SQLOperation::CompletionHandler MakeCompletionHandler();
    void ProcessCallbacks();
    void MarkActivity() noexcept;
    void CheckAfk();
    std::shared_ptr<LoginSession> SharedSelf();

    AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
    AsyncCallbackProcessor<TransactionCallback> _transactionCallbacks;
    std::atomic<std::chrono::steady_clock::rep> _lastActivity{ 0 };
    std::chrono::steady_clock::time_point _nextAfkCheck;
    std::atomic<uint64> _afkChecks{ 0 };
    bool _authenticating = false;
    uint32 _failedResponses = 0;
    uint64 _claimedAccountId = 0;
    std::atomic<uint64> _accountId{ 0 };
    std::string _accountName;
};

#endif
