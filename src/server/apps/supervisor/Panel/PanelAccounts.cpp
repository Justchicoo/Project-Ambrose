/*
 * Project Ambrose by Imjustchico
 * The game-master account and ban console for the panel: searching accounts, creating them, resetting passwords, locking, security levels, and bans, each checked against the caller's command level and audited either way, with PII hidden from callers without the grant.
 */

#include "PanelAccounts.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace
{
    bool EqualsFold(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    }

    bool ContainsFold(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty())
            return true;
        std::string h(haystack), n(needle);
        std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
        return h.find(n) != std::string::npos;
    }
}

bool PanelAccounts::MayActOn(Caller const& caller, PanelAccountRecord const& target) const
{
    return target.SecurityLevel < caller.CommandLevel;
}

nlohmann::json PanelAccounts::AccountJson(PanelAccountRecord const& record, bool showPii)
{
    nlohmann::json answer;
    answer["id"] = record.Id;
    answer["username"] = record.Username;
    answer["security_level"] = record.SecurityLevel;
    answer["locked"] = record.Locked;
    answer["online"] = record.Online;
    answer["join_date"] = record.JoinDate;
    answer["last_login"] = record.LastLogin;
    answer["banned"] = record.Banned;
    answer["ban_until"] = record.BanUntil;
    if (showPii)
    {
        answer["email"] = record.Email;
        answer["last_address"] = record.LastAddress;
        answer["last_machine_id"] = record.LastMachineId;
    }
    return answer;
}

nlohmann::json PanelAccounts::BanJson(PanelBanRecord const& record)
{
    nlohmann::json answer;
    answer["kind"] = record.Kind;
    answer["target"] = record.Target;
    answer["banned_at"] = record.BannedAt;
    answer["unban_at"] = record.UnbanAt;
    answer["banned_by"] = record.BannedBy;
    answer["reason"] = record.Reason;
    return answer;
}

nlohmann::json PanelAccounts::CharacterJson(PanelCharacterRecord const& record)
{
    nlohmann::json answer;
    answer["id"] = record.Id;
    answer["name"] = record.Name;
    answer["level"] = record.Level;
    answer["school"] = record.School;
    answer["location"] = record.Location;
    answer["deleted"] = record.Deleted;
    return answer;
}

std::string_view PanelAccounts::Describe(PanelAccountResult result) noexcept
{
    switch (result)
    {
        case PanelAccountResult::Ok: return "ok";
        case PanelAccountResult::NotFound: return "no such account";
        case PanelAccountResult::NameInvalid: return "invalid name";
        case PanelAccountResult::NameTaken: return "name taken";
        case PanelAccountResult::PasswordInvalid: return "invalid password";
        case PanelAccountResult::PermissionDenied: return "permission denied";
        case PanelAccountResult::LevelTooHigh: return "target outranks the caller";
        case PanelAccountResult::ReasonRequired: return "a reason is required";
        case PanelAccountResult::StoreFailed: return "store failed";
    }
    return "unknown";
}

nlohmann::json PanelAccounts::Search(Caller const& caller, std::string_view query) const
{
    nlohmann::json answer = nlohmann::json::array();
    if (!caller.CanRead)
        return answer;
    for (PanelAccountRecord const& record : _backend.Search(query))
        answer.push_back(AccountJson(record, caller.CanReadPii));
    return answer;
}

nlohmann::json PanelAccounts::Get(Caller const& caller, uint64 id) const
{
    if (!caller.CanRead)
        return nullptr;
    std::optional<PanelAccountRecord> const record = _backend.Get(id);
    if (!record)
        return nullptr;
    return AccountJson(*record, caller.CanReadPii);
}

nlohmann::json PanelAccounts::Create(Caller const& caller, std::string_view username, std::string_view password, std::string_view email, PanelAccountResult& result) const
{
    if (!caller.CanCreate)
    {
        result = PanelAccountResult::PermissionDenied;
        return nullptr;
    }
    uint64 id = 0;
    result = _backend.Create(username, password, email, id);
    if (result != PanelAccountResult::Ok)
        return nullptr;
    std::optional<PanelAccountRecord> const record = _backend.Get(id);
    if (!record)
    {
        result = PanelAccountResult::StoreFailed;
        return nullptr;
    }
    return AccountJson(*record, caller.CanReadPii);
}

nlohmann::json PanelAccounts::ResetPassword(Caller const& caller, uint64 id, PanelAccountResult& result, std::string& generated) const
{
    if (!caller.CanSetPassword)
    {
        result = PanelAccountResult::PermissionDenied;
        return nullptr;
    }
    std::optional<PanelAccountRecord> const record = _backend.Get(id);
    if (!record)
    {
        result = PanelAccountResult::NotFound;
        return nullptr;
    }
    if (!MayActOn(caller, *record))
    {
        result = PanelAccountResult::LevelTooHigh;
        return nullptr;
    }
    result = _backend.SetPassword(id, {}, generated);
    if (result != PanelAccountResult::Ok)
        return nullptr;
    nlohmann::json answer;
    answer["id"] = id;
    answer["password"] = generated;
    return answer;
}

nlohmann::json PanelAccounts::SetLocked(Caller const& caller, uint64 id, bool locked, std::string_view reason, PanelAccountResult& result) const
{
    if (!caller.CanEdit)
    {
        result = PanelAccountResult::PermissionDenied;
        return nullptr;
    }
    if (reason.empty())
    {
        result = PanelAccountResult::ReasonRequired;
        return nullptr;
    }
    std::optional<PanelAccountRecord> const record = _backend.Get(id);
    if (!record)
    {
        result = PanelAccountResult::NotFound;
        return nullptr;
    }
    if (!MayActOn(caller, *record))
    {
        result = PanelAccountResult::LevelTooHigh;
        return nullptr;
    }
    result = _backend.SetLocked(id, locked);
    if (result != PanelAccountResult::Ok)
        return nullptr;
    std::optional<PanelAccountRecord> const updated = _backend.Get(id);
    return updated ? AccountJson(*updated, caller.CanReadPii) : nullptr;
}

nlohmann::json PanelAccounts::SetSecurityLevel(Caller const& caller, uint64 id, uint8 level, std::string_view reason, PanelAccountResult& result) const
{
    if (!caller.CanEdit)
    {
        result = PanelAccountResult::PermissionDenied;
        return nullptr;
    }
    if (reason.empty())
    {
        result = PanelAccountResult::ReasonRequired;
        return nullptr;
    }
    std::optional<PanelAccountRecord> const record = _backend.Get(id);
    if (!record)
    {
        result = PanelAccountResult::NotFound;
        return nullptr;
    }
    if (!MayActOn(caller, *record) || level >= caller.CommandLevel)
    {
        result = PanelAccountResult::LevelTooHigh;
        return nullptr;
    }
    result = _backend.SetSecurityLevel(id, level);
    if (result != PanelAccountResult::Ok)
        return nullptr;
    std::optional<PanelAccountRecord> const updated = _backend.Get(id);
    return updated ? AccountJson(*updated, caller.CanReadPii) : nullptr;
}

nlohmann::json PanelAccounts::Ban(Caller const& caller, std::string_view kind, std::string_view target, uint64 seconds, std::string_view reason, PanelAccountResult& result) const
{
    if (!caller.CanBan)
    {
        result = PanelAccountResult::PermissionDenied;
        return nullptr;
    }
    if (reason.empty())
    {
        result = PanelAccountResult::ReasonRequired;
        return nullptr;
    }
    std::string const by = caller.Username;
    if (EqualsFold(kind, "account"))
    {
        uint64 id = 0;
        try { id = std::stoull(std::string(target)); } catch (...) { result = PanelAccountResult::NotFound; return nullptr; }
        std::optional<PanelAccountRecord> const record = _backend.Get(id);
        if (!record)
        {
            result = PanelAccountResult::NotFound;
            return nullptr;
        }
        if (!MayActOn(caller, *record))
        {
            result = PanelAccountResult::LevelTooHigh;
            return nullptr;
        }
        result = _backend.BanAccount(id, seconds, by, reason);
    }
    else if (EqualsFold(kind, "address"))
    {
        result = _backend.BanAddress(target, seconds, by, reason);
    }
    else if (EqualsFold(kind, "machine"))
    {
        uint64 machineId = 0;
        try { machineId = std::stoull(std::string(target)); } catch (...) { result = PanelAccountResult::NotFound; return nullptr; }
        result = _backend.BanMachine(machineId, seconds, by, reason);
    }
    else
    {
        result = PanelAccountResult::NotFound;
        return nullptr;
    }
    if (result != PanelAccountResult::Ok)
        return nullptr;
    nlohmann::json answer;
    answer["kind"] = std::string(kind);
    answer["target"] = std::string(target);
    answer["seconds"] = seconds;
    return answer;
}

nlohmann::json PanelAccounts::Unban(Caller const& caller, std::string_view kind, std::string_view target, std::string_view reason, PanelAccountResult& result) const
{
    if (!caller.CanUnban)
    {
        result = PanelAccountResult::PermissionDenied;
        return nullptr;
    }
    if (reason.empty())
    {
        result = PanelAccountResult::ReasonRequired;
        return nullptr;
    }
    if (EqualsFold(kind, "account"))
    {
        uint64 id = 0;
        try { id = std::stoull(std::string(target)); } catch (...) { result = PanelAccountResult::NotFound; return nullptr; }
        result = _backend.UnbanAccount(id, reason);
    }
    else if (EqualsFold(kind, "address"))
    {
        result = _backend.UnbanAddress(target, reason);
    }
    else if (EqualsFold(kind, "machine"))
    {
        uint64 machineId = 0;
        try { machineId = std::stoull(std::string(target)); } catch (...) { result = PanelAccountResult::NotFound; return nullptr; }
        result = _backend.UnbanMachine(machineId, reason);
    }
    else
    {
        result = PanelAccountResult::NotFound;
        return nullptr;
    }
    if (result != PanelAccountResult::Ok)
        return nullptr;
    nlohmann::json answer;
    answer["kind"] = std::string(kind);
    answer["target"] = std::string(target);
    return answer;
}

nlohmann::json PanelAccounts::ListBans(Caller const& caller) const
{
    nlohmann::json answer = nlohmann::json::array();
    if (!caller.CanRead)
        return answer;
    for (PanelBanRecord const& record : _backend.ListBans())
        answer.push_back(BanJson(record));
    return answer;
}

nlohmann::json PanelAccounts::Characters(Caller const& caller, uint64 accountId) const
{
    nlohmann::json answer = nlohmann::json::array();
    if (!caller.CanReadCharacters)
        return answer;
    for (PanelCharacterRecord const& record : _backend.CharactersOf(accountId))
        answer.push_back(CharacterJson(record));
    return answer;
}

nlohmann::json PanelAccounts::TopSearch(Caller const& caller, std::string_view query) const
{
    nlohmann::json answer = nlohmann::json::array();
    if (query.empty())
        return answer;
    if (caller.CanRead)
    {
        for (PanelAccountRecord const& record : _backend.Search(query))
        {
            if (!ContainsFold(record.Username, query))
                continue;
            nlohmann::json row;
            row["kind"] = "account";
            row["id"] = record.Id;
            row["title"] = record.Username;
            row["path"] = "/accounts/" + std::to_string(record.Id);
            answer.push_back(row);
            if (answer.size() >= 8)
                break;
        }
    }
    return answer;
}
