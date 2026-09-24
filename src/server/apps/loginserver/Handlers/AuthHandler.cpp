/*
 * Project Ambrose by Imjustchico
 * Authenticates MSG_USER_AUTHEN_V3: reserves the attempt against the address's lockout, decrypts Rec1 with the session's offer, checks the session id, revision, machine and address bans, account, ClientKey1, and account bans and locks from one asynchronous query, kicks any earlier session holding the account, stores a hashed session key with the last login and a resealed verifier in one transaction, then admits the client or answers with the error, closing after too many failures, and refuses the older authentication messages.
 */

#include "AccountMgr.h"
#include "Base64.h"
#include "ClientKey.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "LoginMgr.h"
#include "LoginSession.h"
#include "Rec1.h"
#include "SHA256.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <exception>
#include <vector>

namespace
{
    constexpr char const* AuthLog = "server.loginserver";

    LoginMessages::UserAuthenRsp Failure(AuthResult result)
    {
        LoginMessages::UserAuthenRsp response;
        response.Error = result;
        response.Reason = std::string(AuthResults::GetName(result));
        return response;
    }

    std::string const& UnknownAccountVerifier()
    {
        static std::string const verifier = ClientKey::HashPassword("Project Ambrose has no account by this name");
        return verifier;
    }

    std::string HashSessionKey(std::string_view sessionKey)
    {
        return Base64::Encode(SHA256::GetDigestOf(sessionKey));
    }
}

struct LoginSession::AuthAttempt
{
    ~AuthAttempt()
    {
        if (!Finished && Settings)
            sLoginMgr.GetThrottle().Finish(Address, Admission, false, Settings->MaxAuthAttempts, Settings->Lockout, sLoginMgr.Now());
    }

    std::shared_ptr<LoginSettings const> Settings;
    asio::ip::address Address;
    std::string AddressText;
    AuthAdmission Admission = AuthAdmission::Untracked;
    bool Finished = true;
    LoginSalt Salt;
    uint64 MachineId = 0;
    std::string Username;
    std::string ClientKey1;
    uint64 AccountId = 0;
    std::string SessionKey;
};

void LoginSession::HandleUserAuthenV3(LoginMessages::UserAuthenV3& message)
{
    if (_authenticating)
    {
        AddStrike("MSG_USER_AUTHEN_V3 sent while the previous attempt was still being checked");
        return;
    }

    auto attempt = std::make_shared<AuthAttempt>();
    attempt->Settings = sLoginMgr.GetSettings();
    attempt->Address = GetRemoteAddress();
    attempt->AddressText = attempt->Address.to_string();
    attempt->Salt = GetLoginSalt();
    attempt->MachineId = message.MachineId;
    attempt->Admission = sLoginMgr.GetThrottle().Begin(attempt->Address, attempt->Settings->MaxAuthAttempts, attempt->Settings->Lockout, sLoginMgr.Now());
    if (attempt->Admission == AuthAdmission::LockedOut || attempt->Admission == AuthAdmission::TooManyInFlight)
    {
        LOG_DEBUG(AuthLog, "Session {} from {} was refused: {}; sent MSG_USER_AUTHEN_RSP Error=AuthenFailed and closed the session", GetSessionId(), attempt->AddressText,
            attempt->Admission == AuthAdmission::LockedOut ? "the address is locked out after too many failed logins" : "the address already has as many logins in flight as it has attempts left");
        SendDmlMessageDelayedClose(Failure(AuthResult::AuthenFailed));
        return;
    }
    attempt->Finished = false;

    LOG_DEBUG(AuthLog, "Session {} sent MSG_USER_AUTHEN_V3: version {}, revision {}, data revision {}, locale {}, machine {:016X}, patch client {}, Steam patcher {}, console type {}, {}-byte Rec1",
        GetSessionId(), Ambrose::ForLog(message.Version), Ambrose::ForLog(message.Revision), Ambrose::ForLog(message.DataRevision), Ambrose::ForLog(message.Locale), message.MachineId,
        Ambrose::ForLog(message.PatchClientId), message.IsSteamPatcher, message.ConsoleType, message.Rec1.size());

    if (message.Rec1.size() > MaxRec1Bytes)
    {
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, fmt::format("its {}-byte Rec1 is longer than any credentials", message.Rec1.size()), true);
        return;
    }
    std::string const plain = Rec1::Decode(message.Rec1, attempt->Salt);
    std::vector<std::string_view> const parts = Ambrose::Tokenize(plain, ' ', true);
    if (parts.size() != 3)
    {
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, "Rec1 does not decrypt to a session id, username and ClientKey1", true);
        return;
    }
    attempt->Username = std::string(parts[1]);
    attempt->ClientKey1 = std::string(parts[2]);
    std::optional<uint16> const sessionId = Ambrose::StringTo<uint16>(parts[0]);
    if (!sessionId || *sessionId != GetSessionId())
    {
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, fmt::format("Rec1 names session {}", Ambrose::ForLog(parts[0])), true);
        return;
    }
    if (attempt->Settings->EnforceRevision && !attempt->Settings->AllowsRevision(message.Revision))
    {
        FailAuthentication(attempt.get(), AuthResult::ErrorNoLock, fmt::format("revision {} is not in Login.AllowedRevision", Ambrose::ForLog(message.Revision)), false);
        return;
    }
    if (!AccountMgr::IsLookupName(attempt->Username))
    {
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, "no account can have that name", true);
        return;
    }

    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_AUTHENTICATION);
    if (!statement)
    {
        FailAuthentication(attempt.get(), AuthResult::Timeout, "the login database is not open", false, true);
        return;
    }
    uint64 const now = AccountMgr::Now();
    statement->SetData(0, now);
    statement->SetData(1, attempt->AddressText);
    statement->SetData(2, now);
    statement->SetData(3, attempt->MachineId);
    statement->SetData(4, now);
    statement->SetData(5, attempt->Username);
    _authenticating = true;
    _queryCallbacks.AddCallback(LoginDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback([this, attempt](PreparedQueryResult result)
    {
        try
        {
            ContinueAuthentication(attempt, std::move(result));
        }
        catch (std::exception const& failure)
        {
            AbortAuthentication(attempt.get(), failure);
        }
    }));
}

void LoginSession::ContinueAuthentication(std::shared_ptr<AuthAttempt> const& attempt, PreparedQueryResult result)
{
    if (!IsOpen() || IsKicked())
    {
        _authenticating = false;
        return;
    }
    if (!result)
    {
        FailAuthentication(attempt.get(), AuthResult::Timeout, "the login database did not answer", false, true);
        return;
    }

    PreparedResultSet const& row = *result;
    bool const machineBanned = row[7].Get<bool>();
    bool const addressBanned = row[6].Get<bool>();
    if (machineBanned)
    {
        FailAuthentication(attempt.get(), AuthResult::MachineBanned, fmt::format("machine {:016X} is banned", attempt->MachineId), false);
        return;
    }
    if (addressBanned)
    {
        FailAuthentication(attempt.get(), AuthResult::MachineBanned, "the address is banned", false);
        return;
    }
    if (row[0].IsNull())
    {
        ClientKey::VerifyClientKey1(UnknownAccountVerifier(), attempt->Salt, attempt->ClientKey1);
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, "no account has that name", true);
        return;
    }

    AccountInfo account;
    account.Id = row[0].Get<uint64>();
    account.Username = row[1].Get<std::string>();
    account.StoredVerifier = row[2].Get<std::string>();
    account.VerifierKeyId = row[3].Get<uint8>();
    account.Locked = row[4].Get<bool>();
    bool const accountBanned = row[5].Get<bool>();
    attempt->AccountId = account.Id;
    attempt->Username = account.Username;

    std::optional<std::string> const verifier = sAccountMgr.GetVerifier(account);
    if (!verifier)
    {
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, "the account's verifier cannot be opened", false);
        return;
    }
    if (!ClientKey::VerifyClientKey1(*verifier, attempt->Salt, attempt->ClientKey1))
    {
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, "the password is wrong", true);
        return;
    }
    if (accountBanned || account.Locked)
    {
        FailAuthentication(attempt.get(), AuthResult::AccountBanned, accountBanned ? "the account is banned" : "the account is locked", false);
        return;
    }

    AccountClaim claim = sLoginMgr.ClaimAccount(account.Id, SharedSelf(), attempt->Settings->DuplicateLogins);
    if (!claim.Claimed)
    {
        FailAuthentication(attempt.get(), AuthResult::AuthenFailed, "the account is already logged in and Login.DuplicateLoginPolicy refuses a second login", false);
        SendServerMessage(u"This account is already logged in.", true);
        return;
    }
    _claimedAccountId = account.Id;
    if (claim.Previous)
    {
        LOG_INFO(AuthLog, "Session {} logged in as {} (id {}), so session {} that held the account is kicked", GetSessionId(), account.Username, account.Id, claim.Previous->GetSessionId());
        claim.Previous->KickPlayer(DisconnectLoggedInElsewhere, "This account logged in from another location.");
    }

    auto session = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_SESSION);
    auto lastLogin = LoginDatabase.GetPreparedStatement(LOGIN_UPD_LAST_LOGIN);
    if (!session || !lastLogin)
    {
        ReleaseClaim();
        FailAuthentication(attempt.get(), AuthResult::Timeout, "the login database is not open", false, true);
        return;
    }
    attempt->SessionKey = ClientKey::GenerateSessionKey(attempt->Salt);
    std::string const keyHash = HashSessionKey(attempt->SessionKey);
    uint64 const now = AccountMgr::Now();
    uint64 const expires = now + static_cast<uint64>(attempt->Settings->SessionKeyLifetime.count());
    auto transaction = LoginDatabase.BeginTransaction();
    session->SetData(0, account.Id);
    session->SetData(1, attempt->MachineId);
    session->SetData(2, keyHash);
    session->SetData(3, now);
    session->SetData(4, expires);
    session->SetData(5, attempt->MachineId);
    session->SetData(6, keyHash);
    session->SetData(7, now);
    session->SetData(8, expires);
    transaction->Append(std::move(session));
    lastLogin->SetData(0, now);
    lastLogin->SetData(1, attempt->AddressText);
    lastLogin->SetData(2, attempt->MachineId);
    lastLogin->SetData(3, account.Id);
    transaction->Append(std::move(lastLogin));

    std::shared_ptr<AccountSettings const> const accountSettings = sAccountMgr.GetSettings();
    if (account.VerifierKeyId != accountSettings->Keys.GetActiveKeyId())
    {
        if (auto reseal = LoginDatabase.GetPreparedStatement(LOGIN_UPD_VERIFIER_RESEAL))
        {
            VerifierKeyRing::SealedVerifier const sealed = accountSettings->Keys.Seal(*verifier, account.Username);
            reseal->SetData(0, sealed.Stored);
            reseal->SetData(1, sealed.KeyId);
            reseal->SetData(2, account.Id);
            reseal->SetData(3, account.StoredVerifier);
            reseal->SetData(4, account.VerifierKeyId);
            transaction->Append(std::move(reseal));
        }
    }

    _transactionCallbacks.AddCallback(LoginDatabase.AsyncCommitTransaction(std::move(transaction), MakeCompletionHandler()).AfterComplete([this, attempt](bool committed)
    {
        try
        {
            CompleteAuthentication(attempt, committed);
        }
        catch (std::exception const& failure)
        {
            AbortAuthentication(attempt.get(), failure);
        }
    }));
}

void LoginSession::CompleteAuthentication(std::shared_ptr<AuthAttempt> const& attempt, bool committed)
{
    if (!IsOpen() || IsKicked())
    {
        _authenticating = false;
        ReleaseClaim();
        LOG_DEBUG(AuthLog, "Session {} closed before account {} (id {}) could be admitted", GetSessionId(), attempt->Username, attempt->AccountId);
        return;
    }
    if (!committed)
    {
        ReleaseClaim();
        FailAuthentication(attempt.get(), AuthResult::Timeout, "the session key could not be stored", false, true);
        return;
    }

    sLoginMgr.GetThrottle().Finish(attempt->Address, attempt->Admission, false, attempt->Settings->MaxAuthAttempts, attempt->Settings->Lockout, sLoginMgr.Now());
    attempt->Finished = true;
    _authenticating = false;
    _failedResponses = 0;
    _accountName = attempt->Username;
    _accountId.store(attempt->AccountId, std::memory_order_relaxed);
    _machineId = attempt->MachineId;
    SetStatus(SessionStatus::Authenticated);

    LoginMessages::UserAuthenRsp response;
    response.Error = AuthResult::Success;
    response.UserId = attempt->AccountId;
    response.Rec1 = Rec1::Encode(attempt->SessionKey, attempt->Salt);
    response.PayingUser = 1;
    SendDmlMessage(response);

    LoginMessages::UserAdmitInd admit;
    admit.Status = 1;
    admit.PositionInQueue = 0;
    SendDmlMessage(admit);

    LOG_INFO(AuthLog, "Session {} from {} authenticated as {} (id {}) on machine {:016X}: sent MSG_USER_AUTHEN_RSP Error=0 and MSG_USER_ADMIT_IND Status=1",
        GetSessionId(), attempt->AddressText, attempt->Username, attempt->AccountId, attempt->MachineId);
}

void LoginSession::FailAuthentication(AuthAttempt* attempt, AuthResult result, std::string_view detail, bool countsAsGuess, bool close)
{
    _authenticating = false;
    bool closing = close;
    uint32 limit = 0;
    if (attempt)
    {
        limit = attempt->Settings->MaxAuthAttempts;
        AuthLockState lock = AuthLockState::NotLocked;
        if (!attempt->Finished)
        {
            lock = sLoginMgr.GetThrottle().Finish(attempt->Address, attempt->Admission, countsAsGuess, limit, attempt->Settings->Lockout, sLoginMgr.Now());
            attempt->Finished = true;
        }
        if (lock == AuthLockState::LockedNow)
            LOG_WARN(AuthLog, "Locking out {} for {} s after {} failed logins", attempt->AddressText, attempt->Settings->Lockout.count(), limit);
        if (lock != AuthLockState::NotLocked)
            closing = true;
    }
    ++_failedResponses;
    if (limit != 0 && _failedResponses >= limit)
        closing = true;

    std::string const name = attempt && !attempt->Username.empty() ? Ambrose::ForLog(attempt->Username) : std::string("an unknown user");
    std::string const address = GetRemoteAddress().to_string();
    if (sLoginMgr.AllowAuthLog())
        LOG_INFO(AuthLog, "Session {} from {} failed to authenticate as {}: {}; sent MSG_USER_AUTHEN_RSP Error={}{}", GetSessionId(), address, name, detail, AuthResults::GetName(result), closing ? " and closed the session" : "");
    else
        LOG_DEBUG(AuthLog, "Session {} from {} failed to authenticate as {}: {}; sent MSG_USER_AUTHEN_RSP Error={}{}", GetSessionId(), address, name, detail, AuthResults::GetName(result), closing ? " and closed the session" : "");
    if (closing)
        SendDmlMessageDelayedClose(Failure(result));
    else
        SendDmlMessage(Failure(result));
}

void LoginSession::AbortAuthentication(AuthAttempt* attempt, std::exception const& failure)
{
    ReleaseClaim();
    FailAuthentication(attempt, AuthResult::Timeout, fmt::format("checking the login failed: {}", failure.what()), false, true);
}

void LoginSession::RefuseUnsupportedAuthentication(std::string_view tag)
{
    uint32 const limit = sLoginMgr.GetSettings()->MaxAuthAttempts;
    ++_failedResponses;
    bool const closing = limit != 0 && _failedResponses >= limit;
    if (sLoginMgr.AllowAuthLog())
        LOG_INFO(AuthLog, "Session {} sent {}, which Ambrose does not accept in place of MSG_USER_AUTHEN_V3; sent MSG_USER_AUTHEN_RSP Error=AuthenFailed{}", GetSessionId(), tag, closing ? " and closed the session" : "");
    if (closing)
        SendDmlMessageDelayedClose(Failure(AuthResult::AuthenFailed));
    else
        SendDmlMessage(Failure(AuthResult::AuthenFailed));
}

void LoginSession::HandleUserAuthen(LoginMessages::UserAuthen&)
{
    RefuseUnsupportedAuthentication(LoginMessages::UserAuthen::Tag);
}

void LoginSession::HandleUserAuthenV2(LoginMessages::UserAuthenV2&)
{
    RefuseUnsupportedAuthentication(LoginMessages::UserAuthenV2::Tag);
}

void LoginSession::HandleWebAuthen(LoginMessages::WebAuthen&)
{
    RefuseUnsupportedAuthentication(LoginMessages::WebAuthen::Tag);
}

void LoginSession::HandleWebValidate(LoginMessages::WebValidate&)
{
    RefuseUnsupportedAuthentication(LoginMessages::WebValidate::Tag);
}
