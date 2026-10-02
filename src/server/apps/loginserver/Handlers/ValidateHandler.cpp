/*
 * Project Ambrose by Imjustchico
 * Validates MSG_USER_VALIDATE, which a client sends to come back to character select without its password: reserves the attempt against the address's lockout, reads the account, its bans, lock and session key in one asynchronous query, refuses a key issued to another machine, one renewed longer ago than Login.SessionKeyLifetime as it stands now, and a PassKey3 not made from that key and this connection's offer, kicks any earlier session holding the account, renews the key and the last login in one transaction, then sends MSG_USER_VALIDATE_RSP with Error=0 and MSG_USER_ADMIT_IND; every refusal sends only MSG_USER_VALIDATE_RSP with the error and closes. A ban or lock refusal carries the ban's end as TimeStamp, in Unix seconds or forever.
 */

#include "AccountMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "LoginMgr.h"
#include "LoginSession.h"
#include "PassKey3.h"
#include "StringUtil.h"
#include "SystemMessages.h"

#include <fmt/format.h>

#include <exception>

namespace
{
    constexpr char const* ValidateLog = "server.loginserver";
    constexpr std::size_t PassKey3Length = 88;
}

struct LoginSession::ValidateAttempt
{
    ~ValidateAttempt()
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
    uint64 AccountId = 0;
    uint64 MachineId = 0;
    std::string PassKey3;
    std::string Username;
    std::string StoredKey;
};

void LoginSession::HandleUserValidate(LoginMessages::UserValidate& message)
{
    if (_authenticating)
    {
        AddStrike("MSG_USER_VALIDATE sent while the previous attempt was still being checked");
        return;
    }

    auto attempt = std::make_shared<ValidateAttempt>();
    attempt->Settings = sLoginMgr.GetSettings();
    attempt->Address = GetRemoteAddress();
    attempt->AddressText = attempt->Address.to_string();
    attempt->Salt = GetLoginSalt();
    attempt->AccountId = message.UserId;
    attempt->MachineId = message.MachineId;
    attempt->PassKey3 = std::move(message.PassKey3);
    attempt->Admission = sLoginMgr.GetThrottle().Begin(attempt->Address, attempt->Settings->MaxAuthAttempts, attempt->Settings->Lockout, sLoginMgr.Now());
    if (attempt->Admission == AuthAdmission::LockedOut || attempt->Admission == AuthAdmission::TooManyInFlight)
    {
        FailValidation(nullptr, AuthResult::ValidateFailed, attempt->Admission == AuthAdmission::LockedOut ? "the address is locked out after too many failed logins"
            : "the address already has as many logins in flight as it has attempts left", false);
        return;
    }
    attempt->Finished = false;

    LOG_DEBUG(ValidateLog, "Session {} sent MSG_USER_VALIDATE: user {}, machine {:016X}, locale {}, patch client {}, {}-byte PassKey3", GetSessionId(), message.UserId, message.MachineId,
        Ambrose::ForLog(message.Locale), Ambrose::ForLog(message.PatchClientId), attempt->PassKey3.size());

    if (attempt->PassKey3.size() != PassKey3Length)
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, fmt::format("its {}-byte PassKey3 is not {} bytes", attempt->PassKey3.size(), PassKey3Length), true);
        return;
    }

    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_VALIDATION);
    if (!statement)
    {
        FailValidation(attempt.get(), AuthResult::Timeout, "the login database is not open", false);
        return;
    }
    uint64 const now = AccountMgr::Now();
    statement->SetData(0, now);
    statement->SetData(1, attempt->AddressText);
    statement->SetData(2, now);
    statement->SetData(3, attempt->MachineId);
    statement->SetData(4, now);
    statement->SetData(5, attempt->AccountId);
    _authenticating = true;
    _queryCallbacks.AddCallback(LoginDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback([this, attempt](PreparedQueryResult result)
    {
        try
        {
            ContinueValidation(attempt, std::move(result));
        }
        catch (std::exception const& failure)
        {
            ReleaseClaim();
            FailValidation(attempt.get(), AuthResult::Timeout, fmt::format("checking the validation failed: {}", failure.what()), false);
        }
    }));
}

void LoginSession::ContinueValidation(std::shared_ptr<ValidateAttempt> const& attempt, PreparedQueryResult result)
{
    if (!IsOpen() || IsKicked())
    {
        _authenticating = false;
        return;
    }
    if (!result)
    {
        FailValidation(attempt.get(), AuthResult::Timeout, "the login database did not answer", false);
        return;
    }

    PreparedResultSet const& row = *result;
    if (!row[9].IsNull())
    {
        FailValidation(attempt.get(), AuthResult::MachineBanned, fmt::format("machine {:016X} is banned", attempt->MachineId), false, row[9].Get<uint64>());
        return;
    }
    if (!row[8].IsNull())
    {
        FailValidation(attempt.get(), AuthResult::MachineBanned, "the address is banned", false, row[8].Get<uint64>());
        return;
    }
    if (row[0].IsNull())
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, "no account has that id", true);
        return;
    }
    attempt->Username = row[1].Get<std::string>();
    if (row[4].IsNull())
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, "the account holds no session key", true);
        return;
    }
    if (row[3].Get<uint64>() != attempt->MachineId)
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, fmt::format("its session key was issued to machine {:016X}", row[3].Get<uint64>()), true);
        return;
    }
    uint64 const renewed = row[6].Get<uint64>();
    uint64 const now = AccountMgr::Now();
    uint64 const lifetime = static_cast<uint64>(attempt->Settings->SessionKeyLifetime.count());
    if (now >= renewed + lifetime)
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, fmt::format("its session key was last renewed {} s ago, and Login.SessionKeyLifetime is {} s", now - renewed, lifetime),
            false);
        return;
    }
    attempt->StoredKey = row[4].Get<std::string>();
    uint8 const keyId = row[5].Get<uint8>();
    std::optional<std::string> const sessionKey = sAccountMgr.GetSettings()->Keys.OpenSessionKey(attempt->StoredKey, keyId, attempt->AccountId);
    if (!sessionKey)
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, fmt::format("its session key does not open with key {}", keyId), false);
        return;
    }
    if (!PassKey3::Verify(*sessionKey, attempt->Salt, attempt->PassKey3))
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, "its PassKey3 was not made from the account's session key and this connection's offer", true);
        return;
    }
    bool const accountBanned = !row[7].IsNull();
    if (accountBanned || row[2].Get<bool>())
    {
        FailValidation(attempt.get(), AuthResult::AccountBanned, accountBanned ? "the account is banned" : "the account is locked", false, accountBanned ? row[7].Get<uint64>() : 0);
        return;
    }

    AccountClaim claim = sLoginMgr.ClaimAccount(attempt->AccountId, SharedSelf(), attempt->Settings->DuplicateLogins);
    if (!claim.Claimed)
    {
        FailValidation(attempt.get(), AuthResult::ValidateFailed, "the account is already logged in and Login.DuplicateLoginPolicy refuses a second login", false);
        return;
    }
    _claimedAccountId = attempt->AccountId;
    if (claim.Previous)
    {
        LOG_INFO(ValidateLog, "Session {} validated as {} (id {}), so session {} that held the account is kicked", GetSessionId(), attempt->Username, attempt->AccountId,
            claim.Previous->GetSessionId());
        claim.Previous->KickPlayer(DisconnectLoggedInElsewhere, "This account logged in from another location.");
    }

    auto renew = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_SESSION_RENEWED);
    auto lastLogin = LoginDatabase.GetPreparedStatement(LOGIN_UPD_LAST_LOGIN);
    if (!renew || !lastLogin)
    {
        ReleaseClaim();
        FailValidation(attempt.get(), AuthResult::Timeout, "the login database is not open", false);
        return;
    }
    renew->SetData(0, now);
    renew->SetData(1, now + lifetime);
    renew->SetData(2, attempt->AccountId);
    renew->SetData(3, attempt->StoredKey);
    lastLogin->SetData(0, now);
    lastLogin->SetData(1, attempt->AddressText);
    lastLogin->SetData(2, attempt->MachineId);
    lastLogin->SetData(3, attempt->AccountId);
    auto transaction = LoginDatabase.BeginTransaction();
    transaction->Append(std::move(renew));
    transaction->Append(std::move(lastLogin));
    _transactionCallbacks.AddCallback(LoginDatabase.AsyncCommitTransaction(std::move(transaction), MakeCompletionHandler()).AfterComplete([this, attempt](bool committed)
    {
        try
        {
            CompleteValidation(attempt, committed);
        }
        catch (std::exception const& failure)
        {
            ReleaseClaim();
            FailValidation(attempt.get(), AuthResult::Timeout, fmt::format("checking the validation failed: {}", failure.what()), false);
        }
    }));
}

void LoginSession::CompleteValidation(std::shared_ptr<ValidateAttempt> const& attempt, bool committed)
{
    if (!IsOpen() || IsKicked())
    {
        _authenticating = false;
        ReleaseClaim();
        return;
    }
    if (!committed)
    {
        ReleaseClaim();
        FailValidation(attempt.get(), AuthResult::Timeout, "the session key could not be renewed", false);
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

    LoginMessages::UserValidateRsp response;
    response.Error = AuthResult::Success;
    response.UserId = attempt->AccountId;
    response.PayingUser = 1;
    SendDmlMessage(response);

    LoginMessages::UserAdmitInd admit;
    admit.Status = 1;
    admit.PositionInQueue = 0;
    SendDmlMessage(admit);

    LOG_INFO(ValidateLog, "Session {} from {} validated as {} (id {}) on machine {:016X}: sent MSG_USER_VALIDATE_RSP Error=0 and MSG_USER_ADMIT_IND Status=1",
        GetSessionId(), attempt->AddressText, attempt->Username, attempt->AccountId, attempt->MachineId);
}

void LoginSession::FailValidation(ValidateAttempt* attempt, AuthResult result, std::string_view detail, bool countsAsGuess, uint64 unbanDate)
{
    _authenticating = false;
    if (attempt && !attempt->Finished)
    {
        AuthLockState const lock = sLoginMgr.GetThrottle().Finish(attempt->Address, attempt->Admission, countsAsGuess, attempt->Settings->MaxAuthAttempts, attempt->Settings->Lockout,
            sLoginMgr.Now());
        attempt->Finished = true;
        if (lock == AuthLockState::LockedNow)
            LOG_WARN(ValidateLog, "Locking out {} for {} s after {} failed logins", attempt->AddressText, attempt->Settings->Lockout.count(), attempt->Settings->MaxAuthAttempts);
    }
    std::string const name = !attempt ? std::string("an unknown account")
        : !attempt->Username.empty() ? Ambrose::ForLog(attempt->Username) : fmt::format("account id {}", attempt->AccountId);
    std::string const address = GetRemoteAddress().to_string();
    if (sLoginMgr.AllowAuthLog())
        LOG_INFO(ValidateLog, "Session {} from {} failed to validate as {}: {}; sent MSG_USER_VALIDATE_RSP Error={} and closed the session", GetSessionId(), address, name, detail,
            AuthResults::GetName(result));
    else
        LOG_DEBUG(ValidateLog, "Session {} from {} failed to validate as {}: {}; sent MSG_USER_VALIDATE_RSP Error={} and closed the session", GetSessionId(), address, name, detail,
            AuthResults::GetName(result));
    LoginMessages::UserValidateRsp response;
    response.Error = result;
    response.Reason = std::string(AuthResults::GetName(result));
    if (SystemMessages::CarriesBanEnd(static_cast<uint32>(result)))
        response.TimeStamp = SystemMessages::FormatBanEnd(unbanDate);
    SendDmlMessageDelayedClose(response);
}
