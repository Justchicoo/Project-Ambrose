/*
 * Project Ambrose by Imjustchico
 * The login server's session: routes every client message through the login message table, authenticates MSG_USER_AUTHEN_V3 and validates MSG_USER_VALIDATE's PassKey3 against the login database without blocking its network thread, holds the account it claimed and admitted, deletes one of its wizards for MSG_DELETECHARACTER, lists the account's characters from the login and characters databases the same way, coalescing a request made meanwhile into one more list, creates a wizard the same way again and answers only that it did or did not, drops the client once it idles past the AFK timeout before choosing a character, and tells it when the login server shuts down. It also keeps the account's security level for the queue bypass and carries picks a full realm queued from the admission queue onto its own executor.
 */

#ifndef AMBROSE_LOGINSESSION_H
#define AMBROSE_LOGINSESSION_H

#include "AsyncCallbackProcessor.h"
#include "AuthResult.h"
#include "CountedCallback.h"
#include "LoginMessages.h"
#include "LoginSalt.h"
#include "SQLOperation.h"
#include "SessionBase.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

class PropertyObject;

class LoginSession : public SessionBase
{
public:
    static constexpr uint32 DisconnectLoggedInElsewhere = 1;
    static constexpr std::size_t MaxRec1Bytes = 512;
    static constexpr std::chrono::seconds AfkCheckInterval{ 1 };

    LoginSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context);

    uint64 GetAccountId() const noexcept { return _accountId.load(std::memory_order_relaxed); }
    uint8 GetSecurityLevel() const noexcept { return _securityLevel.load(std::memory_order_relaxed); }
    LoginSalt GetLoginSalt() const noexcept;
    std::chrono::steady_clock::time_point GetLastActivity() const noexcept { return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(_lastActivity.load(std::memory_order_relaxed))); }
    uint64 GetAfkCheckCount() const noexcept { return _afkChecks.load(std::memory_order_relaxed); }
    bool SendShutdownNotice(uint32 message);
    void Update() override;

    void HandleUserAuthenV3(LoginMessages::UserAuthenV3& message);
    void HandleUserValidate(LoginMessages::UserValidate& message);
    void HandleUserAuthen(LoginMessages::UserAuthen& message);
    void HandleUserAuthenV2(LoginMessages::UserAuthenV2& message);
    void HandleWebAuthen(LoginMessages::WebAuthen& message);
    void HandleWebValidate(LoginMessages::WebValidate& message);
    void HandleLoginNotAfk(LoginMessages::LoginNotAfk& message);
    void HandleRequestCharacterList(LoginMessages::RequestCharacterList& message);
    void HandleRequestServerList(LoginMessages::RequestServerList& message);
    void HandleSelectCharacter(LoginMessages::SelectCharacter& message);
    void HandleCreateCharacter(LoginMessages::CreateCharacter& message);
    void HandleDeleteCharacter(LoginMessages::DeleteCharacter& message);
    void HandleLoginLogCharacterCreation(LoginMessages::LoginLogCharacterCreation& message);
    void NotifyAdmissionQueuePosition(LoginMessages::CharacterSelected reply);
    void AdmitQueuedCharacter(LoginMessages::CharacterSelected reply, std::string realmName, uint32 realmId);
    void FailQueuedCharacter(uint64 characterGuid, std::string detail);

protected:
    void OnAccepted() override;
    void OnMessage(DmlMessageData& message) override;
    void OnSessionClosed() override;

private:
    struct AuthAttempt;
    struct ValidateAttempt;

    void ContinueAuthentication(std::shared_ptr<AuthAttempt> const& attempt, PreparedQueryResult result);
    void CompleteAuthentication(std::shared_ptr<AuthAttempt> const& attempt, bool committed);
    void FailAuthentication(AuthAttempt* attempt, AuthResult result, std::string_view detail, bool countsAsGuess, bool close = false, uint64 unbanDate = 0, std::string_view reasonOverride = {});
    void AbortAuthentication(AuthAttempt* attempt, std::exception const& failure);
    void RefuseUnsupportedAuthentication(std::string_view tag);
    void ContinueValidation(std::shared_ptr<ValidateAttempt> const& attempt, PreparedQueryResult result);
    void CompleteValidation(std::shared_ptr<ValidateAttempt> const& attempt, bool committed);
    void FailValidation(ValidateAttempt* attempt, AuthResult result, std::string_view detail, bool countsAsGuess, uint64 unbanDate = 0, std::string_view reasonOverride = {});
    void StartCharacterList();
    void SelectCharacter(uint64 charId, std::string const& realmName, PreparedQueryResult result);
    void IssueCharacterSelected(LoginMessages::CharacterSelected reply, std::string realmName, uint32 realmId);
    void FailCharacterSelect(uint64 charId, std::string_view detail);
    void ListCharacters(uint32 purchasedSlots, uint32 expected);
    void FinishCharacterList(uint32 purchasedSlots, PreparedQueryResult result);
    void FailCharacterList(std::string_view detail);
    bool AbandonCharacterList();
    void JudgeCreation(PropertyObject const& info, uint32 purchasedSlots, uint8 securityLevel, uint32 existing);
    void LogRequest(PropertyObject const& info) const;
    void RefuseCreation(std::string_view detail);
    bool AbandonCreation();
    void EndCharacterList();
    void ReleaseClaim();
    SQLOperation::CompletionHandler MakeCompletionHandler();
    void ProcessCallbacks();
    void MarkActivity() noexcept;
    void CheckAfk();
    void CheckHandoff();
    std::shared_ptr<LoginSession> SharedSelf();

    AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
    AsyncCallbackProcessor<TransactionCallback> _transactionCallbacks;
    AsyncCallbackProcessor<CountedCallback> _countedCallbacks;
    std::atomic<std::chrono::steady_clock::rep> _lastActivity{ 0 };
    std::chrono::steady_clock::time_point _nextAfkCheck;
    std::atomic<uint64> _afkChecks{ 0 };
    bool _authenticating = false;
    bool _listingCharacters = false;
    bool _relistCharacters = false;
    bool _creatingCharacter = false;
    uint32 _creationStage = 0;
    uint32 _creationParameter = 0;
    uint32 _failedResponses = 0;
    uint64 _claimedAccountId = 0;
    std::atomic<uint64> _accountId{ 0 };
    std::atomic<uint8> _securityLevel{ 0 };
    uint64 _machineId = 0;
    std::atomic<std::chrono::steady_clock::rep> _handoffAt{ 0 };
    std::string _accountName;
};

#endif
