/*
 * Project Ambrose by Imjustchico
 * Validates names and passwords, stores base64 SHA-512 verifiers sealed with the active key, maps unique-name races to 'already exists', replaces bans in one transaction, and reads accounts and active bans through synchronous login database statements that report a closed database as an error.
 */

#include "AccountMgr.h"
#include "AccountText.h"
#include "ClientKey.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Utf.h"

#include <fmt/format.h>

namespace
{
    using LoginStatement = std::unique_ptr<PreparedStatement<LoginDatabaseConnection>>;

    AccountInfo ReadAccount(PreparedResultSet const& row)
    {
        AccountInfo account;
        account.Id = row[0].Get<uint64>();
        account.Username = row[1].Get<std::string>();
        account.StoredVerifier = row[2].Get<std::string>();
        account.VerifierKeyId = row[3].Get<uint8>();
        account.Email = row[4].Get<std::string>();
        account.SecurityLevel = row[5].Get<uint8>();
        account.ChatMode = row[6].Get<uint8>();
        account.Locked = row[7].Get<bool>();
        account.PurchasedSlots = row[8].Get<uint32>();
        account.Online = row[9].Get<bool>();
        account.JoinDate = row[10].Get<uint64>();
        account.LastLogin = row[11].Get<uint64>();
        account.LastIp = row[12].Get<std::string>();
        account.LastMachineId = row[13].Get<uint64>();
        return account;
    }
}

bool AccountMgr::IsLookupName(std::string_view username) noexcept
{
    if (username.empty() || username.size() > AccountSettings::MaxUsernameLength)
        return false;
    for (char const c : username)
        if (!Ambrose::AccountText::IsUsernameCharacter(c))
            return false;
    return true;
}

AccountMgr& AccountMgr::Instance()
{
    static AccountMgr instance;
    return instance;
}

AccountMgr::AccountMgr()
    : _settings(std::make_shared<AccountSettings const>())
{
}

bool AccountMgr::LoadSettings(ConfigMgr const& config)
{
    std::vector<std::string> problems;
    std::optional<AccountSettings> settings = AccountSettings::Load(config, problems);
    for (std::string const& problem : problems)
    {
        if (settings)
            LOG_WARN("accounts", "{}", problem);
        else
            LOG_ERROR("accounts", "{}", problem);
    }
    if (!settings)
        return false;
    if (settings->Keys.GetActiveKeyId() == 0)
        LOG_INFO("accounts", "New password verifiers are stored unencrypted; set Account.VerifierKeys and Account.VerifierActiveKey to encrypt them at rest");
    else
        LOG_INFO("accounts", "New password verifiers are encrypted with verifier key {} ({} key(s) loaded); unencrypted verifiers are {}", settings->Keys.GetActiveKeyId(), settings->Keys.GetKeyCount(),
            settings->AllowPlainVerifiers ? "still accepted" : "refused");
    SetSettings(std::move(*settings));
    return true;
}

void AccountMgr::SetSettings(AccountSettings settings)
{
    auto shared = std::make_shared<AccountSettings const>(std::move(settings));
    std::lock_guard const lock(_settingsMutex);
    _settings = std::move(shared);
}

std::shared_ptr<AccountSettings const> AccountMgr::GetSettings() const
{
    std::lock_guard const lock(_settingsMutex);
    return _settings;
}

AccountOpResult AccountMgr::ValidateUsername(std::string_view username) const
{
    switch (Ambrose::AccountText::CheckUsername(username, GetSettings()->UsernameMinLength))
    {
        case Ambrose::AccountText::TextProblem::TooLong: return AccountOpResult::NameTooLong;
        case Ambrose::AccountText::TextProblem::Invalid: return AccountOpResult::NameInvalid;
        case Ambrose::AccountText::TextProblem::TooShort: return AccountOpResult::NameTooShort;
        case Ambrose::AccountText::TextProblem::Ok: break;
    }
    return AccountOpResult::Ok;
}

AccountOpResult AccountMgr::ValidatePassword(std::string_view password) const
{
    switch (Ambrose::AccountText::CheckPassword(password, GetSettings()->PasswordMinLength))
    {
        case Ambrose::AccountText::TextProblem::TooLong: return AccountOpResult::PassTooLong;
        case Ambrose::AccountText::TextProblem::Invalid: return AccountOpResult::PassInvalid;
        case Ambrose::AccountText::TextProblem::TooShort: return AccountOpResult::PassTooShort;
        case Ambrose::AccountText::TextProblem::Ok: break;
    }
    return AccountOpResult::Ok;
}

AccountOpResult AccountMgr::CreateAccount(std::string_view username, std::string_view password, std::string_view email, uint64* accountId)
{
    if (AccountOpResult const result = ValidateUsername(username); result != AccountOpResult::Ok)
        return result;
    if (AccountOpResult const result = ValidatePassword(password); result != AccountOpResult::Ok)
        return result;
    if (email.size() > AccountSettings::MaxEmailLength)
        return AccountOpResult::EmailTooLong;
    if (!Ambrose::AccountText::IsStorable(email))
        return AccountOpResult::EmailInvalid;

    AccountLookup const existing = GetAccountByName(username);
    if (existing.Result != AccountOpResult::Ok)
        return existing.Result;
    if (existing.Account)
        return AccountOpResult::NameAlreadyExists;

    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT);
    if (!statement)
        return AccountOpResult::DatabaseError;
    VerifierKeyRing::SealedVerifier const sealed = GetSettings()->Keys.Seal(ClientKey::HashPassword(password), username);
    statement->SetData(0, username);
    statement->SetData(1, sealed.Stored);
    statement->SetData(2, sealed.KeyId);
    statement->SetData(3, email);
    statement->SetData(4, Now());
    bool const inserted = LoginDatabase.DirectExecute(*statement);

    AccountLookup const created = GetAccountByName(username);
    if (!inserted)
        return created.Account ? AccountOpResult::NameAlreadyExists : AccountOpResult::DatabaseError;
    if (!created.Account)
    {
        LOG_ERROR("accounts", "Created account {} but could not read it back, so its id is unknown", username);
        return AccountOpResult::ReadBackFailed;
    }
    if (accountId)
        *accountId = created.Account->Id;
    LOG_INFO("accounts", "Created account {} (id {})", created.Account->Username, created.Account->Id);
    return AccountOpResult::Ok;
}

AccountOpResult AccountMgr::StoreVerifier(uint64 accountId, std::string_view username, std::string_view password)
{
    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_UPD_VERIFIER);
    if (!statement)
        return AccountOpResult::DatabaseError;
    VerifierKeyRing::SealedVerifier const sealed = GetSettings()->Keys.Seal(ClientKey::HashPassword(password), username);
    statement->SetData(0, sealed.Stored);
    statement->SetData(1, sealed.KeyId);
    statement->SetData(2, accountId);
    return LoginDatabase.DirectExecute(*statement) ? AccountOpResult::Ok : AccountOpResult::DatabaseError;
}

AccountOpResult AccountMgr::ChangePassword(uint64 accountId, std::string_view password)
{
    if (AccountOpResult const result = ValidatePassword(password); result != AccountOpResult::Ok)
        return result;
    AccountLookup const lookup = GetAccountById(accountId);
    if (lookup.Result != AccountOpResult::Ok)
        return lookup.Result;
    if (!lookup.Account)
        return AccountOpResult::NameNotExist;
    AccountOpResult const result = StoreVerifier(accountId, lookup.Account->Username, password);
    if (result == AccountOpResult::Ok)
        LOG_INFO("accounts", "Changed the password of account {} (id {})", lookup.Account->Username, accountId);
    return result;
}

AccountOpResult AccountMgr::SetSecurityLevel(uint64 accountId, uint8 level)
{
    if (level > SEC_CONSOLE)
        return AccountOpResult::BadSecurityLevel;
    AccountLookup const lookup = GetAccountById(accountId);
    if (lookup.Result != AccountOpResult::Ok)
        return lookup.Result;
    if (!lookup.Account)
        return AccountOpResult::NameNotExist;
    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_UPD_SECURITY_LEVEL);
    if (!statement)
        return AccountOpResult::DatabaseError;
    statement->SetData(0, level);
    statement->SetData(1, accountId);
    if (!LoginDatabase.DirectExecute(*statement))
        return AccountOpResult::DatabaseError;
    LOG_INFO("accounts", "Set the security level of account {} (id {}) to {}", lookup.Account->Username, accountId, level);
    return AccountOpResult::Ok;
}

AccountOpResult AccountMgr::SetLocked(uint64 accountId, bool locked)
{
    AccountLookup const lookup = GetAccountById(accountId);
    if (lookup.Result != AccountOpResult::Ok)
        return lookup.Result;
    if (!lookup.Account)
        return AccountOpResult::NameNotExist;
    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_LOCKED);
    if (!statement)
        return AccountOpResult::DatabaseError;
    statement->SetData(0, static_cast<uint8>(locked ? 1 : 0));
    statement->SetData(1, accountId);
    if (!LoginDatabase.DirectExecute(*statement))
        return AccountOpResult::DatabaseError;
    LOG_INFO("accounts", "{} account {} (id {})", locked ? "Locked" : "Unlocked", lookup.Account->Username, accountId);
    return AccountOpResult::Ok;
}

AccountOpResult AccountMgr::Ban(uint64 accountId, std::chrono::seconds duration, std::string_view bannedBy, std::string_view reason)
{
    if (duration.count() < 0 || duration > MaxBanDuration)
        return AccountOpResult::BadDuration;
    if (bannedBy.size() > MaxBannedByLength || reason.size() > MaxReasonLength)
        return AccountOpResult::ReasonTooLong;
    if (!Ambrose::AccountText::IsStorable(bannedBy) || !Ambrose::AccountText::IsStorable(reason))
        return AccountOpResult::ReasonInvalid;
    AccountLookup const lookup = GetAccountById(accountId);
    if (lookup.Result != AccountOpResult::Ok)
        return lookup.Result;
    if (!lookup.Account)
        return AccountOpResult::NameNotExist;
    LoginStatement lift = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_NOT_BANNED);
    LoginStatement insert = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_BANNED);
    if (!lift || !insert)
        return AccountOpResult::DatabaseError;
    uint64 const now = Now();
    uint64 const unbanDate = duration.count() == 0 ? 0 : now + static_cast<uint64>(duration.count());
    lift->SetData(0, accountId);
    insert->SetData(0, accountId);
    insert->SetData(1, now);
    insert->SetData(2, unbanDate);
    insert->SetData(3, bannedBy);
    insert->SetData(4, reason);
    insert->SetData(5, unbanDate);
    insert->SetData(6, bannedBy);
    insert->SetData(7, reason);
    std::shared_ptr<Transaction<LoginDatabaseConnection>> const transaction = LoginDatabase.BeginTransaction();
    transaction->Append(std::move(lift));
    transaction->Append(std::move(insert));
    if (!LoginDatabase.DirectCommitTransaction(transaction))
        return AccountOpResult::DatabaseError;
    if (unbanDate == 0)
        LOG_INFO("accounts", "Banned account {} (id {}) permanently by {}: {}", lookup.Account->Username, accountId, bannedBy, reason);
    else
        LOG_INFO("accounts", "Banned account {} (id {}) for {} second(s) by {}: {}", lookup.Account->Username, accountId, duration.count(), bannedBy, reason);
    return AccountOpResult::Ok;
}

AccountOpResult AccountMgr::Unban(uint64 accountId)
{
    AccountLookup const lookup = GetAccountById(accountId);
    if (lookup.Result != AccountOpResult::Ok)
        return lookup.Result;
    if (!lookup.Account)
        return AccountOpResult::NameNotExist;
    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_NOT_BANNED);
    if (!statement)
        return AccountOpResult::DatabaseError;
    statement->SetData(0, accountId);
    if (!LoginDatabase.DirectExecute(*statement))
        return AccountOpResult::DatabaseError;
    LOG_INFO("accounts", "Lifted the bans on account {} (id {})", lookup.Account->Username, accountId);
    return AccountOpResult::Ok;
}

AccountLookup AccountMgr::GetAccountByName(std::string_view username) const
{
    if (!IsLookupName(username))
        return { AccountOpResult::Ok, std::nullopt };
    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_BY_NAME);
    if (!statement)
        return { AccountOpResult::DatabaseError, std::nullopt };
    statement->SetData(0, username);
    PreparedQueryResult result;
    if (!LoginDatabase.TryQuery(*statement, result))
        return { AccountOpResult::DatabaseError, std::nullopt };
    if (!result)
        return { AccountOpResult::Ok, std::nullopt };
    return { AccountOpResult::Ok, ReadAccount(*result) };
}

AccountLookup AccountMgr::GetAccountById(uint64 accountId) const
{
    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_BY_ID);
    if (!statement)
        return { AccountOpResult::DatabaseError, std::nullopt };
    statement->SetData(0, accountId);
    PreparedQueryResult result;
    if (!LoginDatabase.TryQuery(*statement, result))
        return { AccountOpResult::DatabaseError, std::nullopt };
    if (!result)
        return { AccountOpResult::Ok, std::nullopt };
    return { AccountOpResult::Ok, ReadAccount(*result) };
}

std::optional<AccountBan> AccountMgr::GetActiveBan(uint64 accountId, AccountOpResult* result) const
{
    if (result)
        *result = AccountOpResult::DatabaseError;
    LoginStatement statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_BANNED);
    if (!statement)
        return std::nullopt;
    statement->SetData(0, accountId);
    statement->SetData(1, Now());
    PreparedQueryResult rows;
    if (!LoginDatabase.TryQuery(*statement, rows))
        return std::nullopt;
    if (result)
        *result = AccountOpResult::Ok;
    if (!rows)
        return std::nullopt;
    AccountBan ban;
    ban.BanDate = (*rows)[0].Get<uint64>();
    ban.UnbanDate = (*rows)[1].Get<uint64>();
    ban.BannedBy = (*rows)[2].Get<std::string>();
    ban.Reason = (*rows)[3].Get<std::string>();
    return ban;
}

std::optional<std::string> AccountMgr::GetVerifier(AccountInfo const& account) const
{
    std::shared_ptr<AccountSettings const> const settings = GetSettings();
    if (account.VerifierKeyId == 0 && settings->Keys.GetActiveKeyId() != 0 && !settings->AllowPlainVerifiers)
    {
        LOG_ERROR("accounts", "Account {} (id {}) has an unencrypted verifier, which Account.AllowPlainVerifiers = 0 refuses", account.Username, account.Id);
        return std::nullopt;
    }
    if (!settings->Keys.HasKey(account.VerifierKeyId))
    {
        LOG_ERROR("accounts", "Account {} (id {}) has a verifier sealed with key {}, which Account.VerifierKeys does not list", account.Username, account.Id, account.VerifierKeyId);
        return std::nullopt;
    }
    std::optional<std::string> verifier = settings->Keys.Open(account.StoredVerifier, account.VerifierKeyId, account.Username);
    if (!verifier || verifier->size() != 88)
    {
        LOG_ERROR("accounts", "Account {} (id {}) has a verifier that does not open with key {}", account.Username, account.Id, account.VerifierKeyId);
        return std::nullopt;
    }
    return verifier;
}

std::string_view AccountMgr::Describe(AccountOpResult result) noexcept
{
    switch (result)
    {
        case AccountOpResult::Ok: return "done";
        case AccountOpResult::NameTooShort: return "the username is too short";
        case AccountOpResult::NameTooLong: return "the username is longer than 32 characters";
        case AccountOpResult::NameInvalid: return "the username may only use letters, digits, '_', '-' and '.'";
        case AccountOpResult::PassTooShort: return "the password is too short";
        case AccountOpResult::PassTooLong: return "the password is longer than 128 bytes";
        case AccountOpResult::PassInvalid: return "the password must be valid UTF-8 without control characters";
        case AccountOpResult::EmailTooLong: return "the email address is longer than 255 bytes";
        case AccountOpResult::EmailInvalid: return "the email address must be valid UTF-8 without control characters";
        case AccountOpResult::NameAlreadyExists: return "an account with that username already exists";
        case AccountOpResult::NameNotExist: return "no account has that name";
        case AccountOpResult::BadSecurityLevel: return "the security level must be 0 to 4";
        case AccountOpResult::ReasonTooLong: return "the ban author must be at most 64 bytes and the reason at most 255";
        case AccountOpResult::ReasonInvalid: return "the ban reason must be valid UTF-8 without control characters";
        case AccountOpResult::ReadBackFailed: return "the account was created but could not be read back; check the login database";
        case AccountOpResult::BadDuration: return "a ban lasts from one second to 100 years, or is permanent";
        case AccountOpResult::DatabaseError: return "the login database failed or is not open; see the sql log";
    }
    return "unknown result";
}

uint64 AccountMgr::Now() noexcept
{
    return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}
