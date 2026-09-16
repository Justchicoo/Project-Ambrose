/*
 * Project Ambrose by Imjustchico
 * Account management over the login database: creating accounts with validated names and encrypted-at-rest verifiers, passwords, security levels, locks, bans that replace earlier bans, and lookups by name or id.
 */

#ifndef AMBROSE_ACCOUNTMGR_H
#define AMBROSE_ACCOUNTMGR_H

#include "AccountSettings.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

enum class AccountOpResult : uint8
{
    Ok,
    NameTooShort,
    NameTooLong,
    NameInvalid,
    PassTooShort,
    PassTooLong,
    PassInvalid,
    EmailTooLong,
    EmailInvalid,
    NameAlreadyExists,
    NameNotExist,
    BadSecurityLevel,
    ReasonTooLong,
    ReasonInvalid,
    BadDuration,
    ReadBackFailed,
    DatabaseError
};

enum AccountSecurityLevel : uint8
{
    SEC_PLAYER = 0,
    SEC_MODERATOR = 1,
    SEC_GAMEMASTER = 2,
    SEC_ADMINISTRATOR = 3,
    SEC_CONSOLE = 4
};

struct AccountInfo
{
    uint64 Id = 0;
    std::string Username;
    std::string StoredVerifier;
    uint8 VerifierKeyId = 0;
    std::string Email;
    uint8 SecurityLevel = SEC_PLAYER;
    uint8 ChatMode = 0;
    bool Locked = false;
    uint32 PurchasedSlots = 0;
    bool Online = false;
    uint64 JoinDate = 0;
    uint64 LastLogin = 0;
    std::string LastIp;
    uint64 LastMachineId = 0;
};

struct AccountBan
{
    uint64 BanDate = 0;
    uint64 UnbanDate = 0;
    std::string BannedBy;
    std::string Reason;

    bool IsPermanent() const noexcept { return UnbanDate == 0; }
};

struct AccountLookup
{
    AccountOpResult Result = AccountOpResult::DatabaseError;
    std::optional<AccountInfo> Account;
};

class AccountMgr
{
public:
    static constexpr uint32 MaxBannedByLength = 64;
    static constexpr uint32 MaxReasonLength = 255;
    static constexpr std::chrono::seconds MaxBanDuration{ 100LL * 365 * 24 * 3600 };

    static AccountMgr& Instance();

    bool LoadSettings(ConfigMgr const& config);
    void SetSettings(AccountSettings settings);
    std::shared_ptr<AccountSettings const> GetSettings() const;

    AccountOpResult ValidateUsername(std::string_view username) const;
    AccountOpResult ValidatePassword(std::string_view password) const;

    AccountOpResult CreateAccount(std::string_view username, std::string_view password, std::string_view email = {}, uint64* accountId = nullptr);
    AccountOpResult ChangePassword(uint64 accountId, std::string_view password);
    AccountOpResult SetSecurityLevel(uint64 accountId, uint8 level);
    AccountOpResult SetLocked(uint64 accountId, bool locked);
    AccountOpResult Ban(uint64 accountId, std::chrono::seconds duration, std::string_view bannedBy, std::string_view reason);
    AccountOpResult Unban(uint64 accountId);

    AccountLookup GetAccountByName(std::string_view username) const;
    AccountLookup GetAccountById(uint64 accountId) const;
    std::optional<AccountBan> GetActiveBan(uint64 accountId, AccountOpResult* result = nullptr) const;
    std::optional<std::string> GetVerifier(AccountInfo const& account) const;

    static std::string_view Describe(AccountOpResult result) noexcept;
    static uint64 Now() noexcept;

private:
    AccountMgr();

    AccountOpResult StoreVerifier(uint64 accountId, std::string_view username, std::string_view password);

    mutable std::mutex _settingsMutex;
    std::shared_ptr<AccountSettings const> _settings;
};

#define sAccountMgr AccountMgr::Instance()

#endif
