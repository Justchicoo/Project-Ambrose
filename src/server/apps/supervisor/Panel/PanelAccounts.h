/*
 * Project Ambrose by Imjustchico
 * The game-master account and ban console for the panel: searching accounts by username, email, address or machine id, creating them, resetting a password so the new verifier is sealed and every session dies, locking and unlocking, setting the security level, and banning and unbanning accounts, addresses and machines, each action checked against the caller's command level and grants, refused loudly and audited either way, with email, address and machine id hidden from callers without the PII grant.
 */

#ifndef AMBROSE_PANELACCOUNTS_H
#define AMBROSE_PANELACCOUNTS_H

#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class PanelAccountResult : uint8
{
    Ok,
    NotFound,
    NameInvalid,
    NameTaken,
    PasswordInvalid,
    PermissionDenied,
    LevelTooHigh,
    ReasonRequired,
    StoreFailed
};

struct PanelAccountRecord
{
    uint64 Id = 0;
    std::string Username;
    std::string Email;
    std::string LastAddress;
    uint64 LastMachineId = 0;
    uint8 SecurityLevel = 0;
    bool Locked = false;
    bool Online = false;
    uint64 JoinDate = 0;
    uint64 LastLogin = 0;
    bool Banned = false;
    uint64 BanUntil = 0;
};

struct PanelBanRecord
{
    std::string Kind;
    std::string Target;
    uint64 BannedAt = 0;
    uint64 UnbanAt = 0;
    std::string BannedBy;
    std::string Reason;
};

struct PanelCharacterRecord
{
    uint64 Id = 0;
    std::string Name;
    uint8 Level = 0;
    uint8 School = 0;
    std::string Location;
    bool Deleted = false;
};

class AccountBackend
{
public:
    virtual ~AccountBackend() = default;

    virtual std::vector<PanelAccountRecord> Search(std::string_view query) = 0;
    virtual std::optional<PanelAccountRecord> Get(uint64 id) = 0;
    virtual PanelAccountResult Create(std::string_view username, std::string_view password, std::string_view email, uint64& id) = 0;
    virtual PanelAccountResult SetPassword(uint64 id, std::string_view password, std::string& generated) = 0;
    virtual PanelAccountResult SetLocked(uint64 id, bool locked) = 0;
    virtual PanelAccountResult SetSecurityLevel(uint64 id, uint8 level) = 0;
    virtual PanelAccountResult BanAccount(uint64 id, uint64 seconds, std::string_view by, std::string_view reason) = 0;
    virtual PanelAccountResult UnbanAccount(uint64 id, std::string_view reason) = 0;
    virtual PanelAccountResult BanAddress(std::string_view address, uint64 seconds, std::string_view by, std::string_view reason) = 0;
    virtual PanelAccountResult UnbanAddress(std::string_view address, std::string_view reason) = 0;
    virtual PanelAccountResult BanMachine(uint64 machineId, uint64 seconds, std::string_view by, std::string_view reason) = 0;
    virtual PanelAccountResult UnbanMachine(uint64 machineId, std::string_view reason) = 0;
    virtual std::vector<PanelBanRecord> ListBans() = 0;
    virtual std::vector<PanelCharacterRecord> CharactersOf(uint64 accountId) = 0;
};

class PanelAccounts
{
public:
    explicit PanelAccounts(AccountBackend& backend) : _backend(backend) {}

    struct Caller
    {
        int64 UserId = 0;
        std::string Username;
        uint8 CommandLevel = 0;
        bool CanRead = false;
        bool CanReadPii = false;
        bool CanCreate = false;
        bool CanEdit = false;
        bool CanSetPassword = false;
        bool CanBan = false;
        bool CanUnban = false;
        bool CanReadCharacters = false;
    };

    nlohmann::json Search(Caller const& caller, std::string_view query) const;
    nlohmann::json Get(Caller const& caller, uint64 id) const;
    nlohmann::json Create(Caller const& caller, std::string_view username, std::string_view password, std::string_view email, PanelAccountResult& result) const;
    nlohmann::json ResetPassword(Caller const& caller, uint64 id, PanelAccountResult& result, std::string& generated) const;
    nlohmann::json SetLocked(Caller const& caller, uint64 id, bool locked, std::string_view reason, PanelAccountResult& result) const;
    nlohmann::json SetSecurityLevel(Caller const& caller, uint64 id, uint8 level, std::string_view reason, PanelAccountResult& result) const;
    nlohmann::json Ban(Caller const& caller, std::string_view kind, std::string_view target, uint64 seconds, std::string_view reason, PanelAccountResult& result) const;
    nlohmann::json Unban(Caller const& caller, std::string_view kind, std::string_view target, std::string_view reason, PanelAccountResult& result) const;
    nlohmann::json ListBans(Caller const& caller) const;
    nlohmann::json Characters(Caller const& caller, uint64 accountId) const;
    nlohmann::json TopSearch(Caller const& caller, std::string_view query) const;

    static nlohmann::json AccountJson(PanelAccountRecord const& record, bool showPii);
    static nlohmann::json BanJson(PanelBanRecord const& record);
    static nlohmann::json CharacterJson(PanelCharacterRecord const& record);
    static std::string_view Describe(PanelAccountResult result) noexcept;

private:
    bool MayActOn(Caller const& caller, PanelAccountRecord const& target) const;

    AccountBackend& _backend;
};

#endif
