/*
 * Project Ambrose by Imjustchico
 * The panel's personal API keys: a key reads amb_ plus a 12-character public id plus a dot plus a 32-byte secret in base32, only a SHA-256 of the secret is stored and compared in constant time, and the full text is handed out once at creation and never again; each key carries its owner's id, a name, a permission subset, the scopes it may touch, the CIDRs it may come from for IPv4 and IPv6, when it was created and when it expires, when it was revoked and until when a rotated key's old secret still answers, and when and from where it was last used, written at most once a minute; creating one counts the owner's live keys in the same transaction as the insert against the per-user limit, rotating one makes a replacement with the same permissions, scopes and CIDRs and an optional short grace for the old secret, and revoking one ends it on the next request; authenticating one refuses an unknown id or a wrong secret without saying which, an expired or revoked key past its grace, and an address outside its CIDRs with an audit row naming the key id; rights at request time are the key's subset and scopes met against its owner's current permissions, so demoting the owner shrinks every key at once.
 */

#include "PanelApiKeys.h"

#include "Base32.h"
#include "ConstantTime.h"
#include "CryptoRandom.h"
#include "Hex.h"
#include "PanelAudit.h"
#include "PanelPermissions.h"
#include "SHA256.h"
#include "StringUtil.h"
#include "TrustedProxies.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace
{
    constexpr std::size_t SecretLength = (PanelApiKeys::SecretBytes * 8 + 4) / 5;
    constexpr std::string_view IdAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    constexpr std::string_view KeyColumns = "panel_api_key.id, panel_api_key.public_id, panel_api_key.owner_user_id,"
        " panel_api_key.name, panel_api_key.permissions, panel_api_key.scopes, panel_api_key.cidrs,"
        " panel_api_key.created_epoch_ms, panel_api_key.expires_epoch_ms, panel_api_key.revoked_epoch_ms,"
        " panel_api_key.grace_expires_epoch_ms, panel_api_key.replaced_by,"
        " panel_api_key.last_used_epoch_ms, panel_api_key.last_used_address";

    bool IsIdChar(char value) noexcept
    {
        return IdAlphabet.find(value) != std::string_view::npos;
    }

    bool IsScopeChar(char value) noexcept
    {
        return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '-';
    }

    bool IsAppScope(std::string_view scope) noexcept
    {
        if (scope.empty() || scope.size() > 32 || scope.front() < 'a' || scope.front() > 'z')
            return false;
        return std::all_of(scope.begin(), scope.end(), IsScopeChar);
    }

    std::string Join(std::vector<std::string> const& parts)
    {
        std::string joined;
        for (std::string const& part : parts)
        {
            if (!joined.empty())
                joined += ',';
            joined += part;
        }
        return joined;
    }

    std::vector<std::string> ParseArray(std::string const& text)
    {
        std::vector<std::string> values;
        nlohmann::json const parsed = nlohmann::json::parse(text, nullptr, false);
        if (!parsed.is_array())
            return values;
        for (nlohmann::json const& entry : parsed)
            if (entry.is_string())
                values.push_back(entry.get<std::string>());
        return values;
    }

    std::string DumpArray(std::vector<std::string> const& values)
    {
        nlohmann::json array = nlohmann::json::array();
        for (std::string const& value : values)
            array.push_back(value);
        return array.dump();
    }

    AuditEvent KeyEvent(PanelApiKeyCall const& call, std::string name, AuditResult result = AuditResult::Succeeded)
    {
        AuditEvent event;
        event.Name = std::move(name);
        event.Actor = call.ViaKey.empty() ? AuditActor::User : AuditActor::Token;
        event.ActorId = call.ViaKey.empty() ? std::to_string(call.UserId) : call.ViaKey;
        event.ActorName = call.Username;
        event.Address = call.Address;
        event.UserAgent = call.UserAgent;
        event.Result = result;
        return event;
    }
}

bool PanelApiKey::Live(int64 nowEpochMs) const noexcept
{
    if (ExpiresEpochMs && nowEpochMs >= *ExpiresEpochMs)
        return false;
    if (!RevokedEpochMs)
        return true;
    return GraceExpiresEpochMs && nowEpochMs < *GraceExpiresEpochMs;
}

PanelApiKeys::PanelApiKeys(PanelStore& store, Setting setting, Audit audit)
    : _store(store), _setting(std::move(setting)), _audit(std::move(audit))
{
}

bool PanelApiKeys::IsKeyText(std::string_view text) noexcept
{
    if (text.size() != Prefix.size() + PublicIdLength + 1 + SecretLength)
        return false;
    if (!text.starts_with(Prefix))
        return false;
    text.remove_prefix(Prefix.size());
    for (char const value : text.substr(0, PublicIdLength))
        if (!IsIdChar(value))
            return false;
    if (text[PublicIdLength] != '.')
        return false;
    for (char const value : text.substr(PublicIdLength + 1))
        if (!IsIdChar(value))
            return false;
    return true;
}

std::string_view PanelApiKeys::PublicIdOf(std::string_view text) noexcept
{
    if (!IsKeyText(text))
        return {};
    return text.substr(Prefix.size(), PublicIdLength);
}

bool PanelApiKeys::IsKeyPrincipal(std::string_view principal) noexcept
{
    constexpr std::string_view keyed = "apikey:";
    return principal.starts_with(keyed) && principal.size() == keyed.size() + PublicIdLength;
}

std::string_view PanelApiKeys::PublicIdOfPrincipal(std::string_view principal) noexcept
{
    if (!IsKeyPrincipal(principal))
        return {};
    return principal.substr(std::string_view("apikey:").size());
}

std::string PanelApiKeys::PrincipalOf(std::string_view publicId)
{
    return "apikey:" + std::string(publicId);
}

std::string_view PanelApiKeys::Explain(PanelApiKeyResult result) noexcept
{
    switch (result)
    {
        case PanelApiKeyResult::Ok: return "done";
        case PanelApiKeyResult::TooMany: return "the operator already holds the most API keys the panel allows";
        case PanelApiKeyResult::BadName: return "a key takes a name of 1 to 64 printable characters";
        case PanelApiKeyResult::BadPermissions: return "every permission in the subset must be one the panel knows and one its maker holds";
        case PanelApiKeyResult::BadScopes: return "every scope is panel or an app name";
        case PanelApiKeyResult::BadCidrs: return "every allowed entry must be an address or a CIDR range";
        case PanelApiKeyResult::BadExpiry: return "a key expires in the future, and only the owner's policy allows one that never expires";
        case PanelApiKeyResult::UnknownKey: return "no API key starts that way";
        case PanelApiKeyResult::WrongSecret: return "that secret does not match the key";
        case PanelApiKeyResult::Revoked: return "the key was revoked";
        case PanelApiKeyResult::Expired: return "the key has expired";
        case PanelApiKeyResult::OutsideCidrs: return "this address is outside the key's allowed CIDRs";
        case PanelApiKeyResult::StoreFailed: return "the panel store could not be read or written";
    }
    return "the key could not be used";
}

int64 PanelApiKeys::SettingOr(std::string_view key, int64 fallback) const
{
    if (!_setting)
        return fallback;
    std::optional<int64> const value = Ambrose::StringTo<int64>(_setting(key));
    return value.value_or(fallback);
}

int64 PanelApiKeys::MaxPerUser() const
{
    return std::max<int64>(1, SettingOr("Limits.ApiKeysPerUser", DefaultMaxPerUser));
}

int64 PanelApiKeys::ExpiryDays() const
{
    return std::max<int64>(1, SettingOr("Limits.ApiKeyDefaultExpiryDays", DefaultExpiryDays));
}

bool PanelApiKeys::NoExpiryAllowed() const
{
    return SettingOr("Limits.ApiKeyAllowNoExpiry", 0) != 0;
}

std::optional<PanelApiKey> PanelApiKeys::Read(PanelStore::Statement const& row)
{
    PanelApiKey key;
    key.Id = row.Int64(0);
    key.PublicId = row.Text(1);
    key.OwnerUserId = row.Int64(2);
    key.Name = row.Text(3);
    key.Permissions = ParseArray(row.Text(4));
    key.Scopes = ParseArray(row.Text(5));
    key.Cidrs = ParseArray(row.Text(6));
    key.CreatedEpochMs = row.Int64(7);
    if (!row.IsNull(8))
        key.ExpiresEpochMs = row.Int64(8);
    if (!row.IsNull(9))
        key.RevokedEpochMs = row.Int64(9);
    if (!row.IsNull(10))
        key.GraceExpiresEpochMs = row.Int64(10);
    if (!row.IsNull(11))
        key.ReplacedBy = row.Int64(11);
    if (!row.IsNull(12))
        key.LastUsedEpochMs = row.Int64(12);
    key.LastUsedAddress = row.Text(13);
    return key;
}

std::string PanelApiKeys::HashOf(std::span<uint8 const> secret)
{
    return Hex::Encode(SHA256::GetDigestOf(secret));
}

bool PanelApiKeys::Mint(std::string& publicId, std::array<uint8, SecretBytes>& secret, std::string& text)
{
    std::array<uint8, 7> const idBytes = Ambrose::Crypto::GetRandomArray<7>();
    publicId = Base32::Encode(idBytes, Base32::Alphabet::Rfc4648);
    if (publicId.size() != PublicIdLength)
        return false;
    secret = Ambrose::Crypto::GetRandomArray<SecretBytes>();
    std::string const encoded = Base32::Encode(secret, Base32::Alphabet::Rfc4648);
    if (encoded.size() != SecretLength)
        return false;
    text = std::string(Prefix) + publicId + "." + encoded;
    return true;
}

PanelApiKeyResult PanelApiKeys::CheckDraft(PanelApiKeyDraft const& draft, std::set<std::string, std::less<>> const& grantable, std::string& problem) const
{
    problem.clear();
    bool const printable = !draft.Name.empty() && draft.Name.size() <= MaxNameLength
        && std::all_of(draft.Name.begin(), draft.Name.end(), [](char value) { return value >= 0x20 && value != 0x7f; });
    if (!printable)
    {
        problem = "name";
        return PanelApiKeyResult::BadName;
    }
    for (std::string const& permission : draft.Permissions)
    {
        if (!PanelPermissions::Holds(permission) || !grantable.contains(permission))
        {
            problem = permission;
            return PanelApiKeyResult::BadPermissions;
        }
    }
    std::vector<std::string> const scopes = draft.Scopes.empty() ? std::vector<std::string>{ "panel" } : draft.Scopes;
    for (std::string const& scope : scopes)
    {
        if (scope != "panel" && !IsAppScope(scope))
        {
            problem = scope;
            return PanelApiKeyResult::BadScopes;
        }
    }
    std::vector<std::string> cidrProblems;
    TrustedProxies::Parse(Join(draft.Cidrs), &cidrProblems, "cidrs");
    if (!cidrProblems.empty())
    {
        problem = cidrProblems.front();
        return PanelApiKeyResult::BadCidrs;
    }
    int64 const now = PanelStore::NowEpochMs();
    if (draft.ExpiresEpochMs && *draft.ExpiresEpochMs <= now)
    {
        problem = "expires_epoch_ms";
        return PanelApiKeyResult::BadExpiry;
    }
    if (draft.NoExpiry && !NoExpiryAllowed())
    {
        problem = "no_expiry";
        return PanelApiKeyResult::BadExpiry;
    }
    return PanelApiKeyResult::Ok;
}

PanelApiKeyResult PanelApiKeys::Create(PanelApiKeyCall const& call, PanelApiKeyDraft const& draft, PanelApiKeyIssued& issued, std::string& error)
{
    error.clear();
    std::string problem;
    PanelApiKeyResult const checked = CheckDraft(draft, call.Rights, problem);
    if (checked != PanelApiKeyResult::Ok)
    {
        error = std::string(Explain(checked)) + ": " + problem;
        return checked;
    }
    int64 const now = PanelStore::NowEpochMs();
    std::optional<int64> const expires = draft.ExpiresEpochMs
        ? draft.ExpiresEpochMs
        : (draft.NoExpiry ? std::optional<int64>{} : std::optional<int64>(now + ExpiryDays() * 86400000));
    std::vector<std::string> const scopes = draft.Scopes.empty() ? std::vector<std::string>{ "panel" } : draft.Scopes;

    std::string publicId;
    std::array<uint8, SecretBytes> secret{};
    std::string text;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        if (!Mint(publicId, secret, text))
        {
            error = "a key could not be minted";
            return PanelApiKeyResult::StoreFailed;
        }
        std::optional<PanelApiKey> const clash = FindByPublicId(publicId, error);
        if (!clash && !error.empty())
            return PanelApiKeyResult::StoreFailed;
        if (!clash)
            break;
        if (attempt == 7)
        {
            error = "a unique public id could not be minted";
            return PanelApiKeyResult::StoreFailed;
        }
    }
    std::string const hash = HashOf(secret);

    AuditEvent event = KeyEvent(call, "panel:apikey.created");
    event.On("api_key", publicId, draft.Name).On("panel_user", std::to_string(draft.OwnerUserId), call.Username);
    nlohmann::json properties;
    properties["name"] = draft.Name;
    properties["permissions"] = draft.Permissions;
    properties["scopes"] = scopes;
    properties["cidrs"] = draft.Cidrs;
    if (expires)
        properties["expires_epoch_ms"] = *expires;
    else
        properties["no_expiry"] = true;
    event.Properties = properties.dump();

    int64 const maxPerUser = MaxPerUser();
    bool done = false;
    bool limited = false;
    auto const change = [&](AuditEvent&, std::string& failure)
    {
        std::optional<PanelStore::Statement> count = _store.Prepare(
            "SELECT COUNT(*) FROM panel_api_key WHERE owner_user_id = ? AND revoked_epoch_ms IS NULL", failure);
        if (!count)
            return false;
        count->Bind(1, draft.OwnerUserId);
        if (!count->Step(failure))
            return false;
        if (count->Int64(0) >= maxPerUser)
        {
            limited = true;
            failure = std::string(Explain(PanelApiKeyResult::TooMany));
            return false;
        }
        count.reset();
        std::optional<PanelStore::Statement> insert = _store.Prepare(
            "INSERT INTO panel_api_key (public_id, secret_hash, owner_user_id, name, permissions, scopes, cidrs,"
            " created_epoch_ms, expires_epoch_ms) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", failure);
        if (!insert)
            return false;
        insert->Bind(1, publicId);
        insert->Bind(2, hash);
        insert->Bind(3, draft.OwnerUserId);
        insert->Bind(4, draft.Name);
        insert->Bind(5, DumpArray(draft.Permissions));
        insert->Bind(6, DumpArray(scopes));
        insert->Bind(7, DumpArray(draft.Cidrs));
        insert->Bind(8, now);
        if (expires)
            insert->Bind(9, *expires);
        else
            insert->BindNull(9);
        if (!insert->Run(failure))
            return false;
        done = true;
        return true;
    };
    bool const recorded = _audit ? _audit(event, change, error) : change(event, error);
    if (limited)
        return PanelApiKeyResult::TooMany;
    if (!recorded || !done)
        return PanelApiKeyResult::StoreFailed;

    std::optional<PanelApiKey> const made = FindByPublicId(publicId, error);
    if (!made)
        return PanelApiKeyResult::StoreFailed;
    issued.Key = *made;
    issued.Key.OwnerUsername = call.Username;
    issued.Text = text;
    return PanelApiKeyResult::Ok;
}

std::optional<PanelApiKey> PanelApiKeys::FindByPublicId(std::string_view publicId, std::string& error)
{
    error.clear();
    std::optional<PanelStore::Statement> find = _store.Prepare(
        "SELECT " + std::string(KeyColumns) + " FROM panel_api_key WHERE public_id = ?", error);
    if (!find)
        return std::nullopt;
    find->Bind(1, publicId);
    if (!find->Step(error))
        return std::nullopt;
    return Read(*find);
}

std::optional<int64> PanelApiKeys::OwnerOf(std::string_view principal, std::string& error)
{
    std::string_view const publicId = PublicIdOfPrincipal(principal);
    if (publicId.empty())
        return std::nullopt;
    std::optional<PanelApiKey> const key = FindByPublicId(publicId, error);
    if (!key)
        return std::nullopt;
    return key->OwnerUserId;
}

std::vector<PanelApiKey> PanelApiKeys::ListOf(int64 ownerUserId, std::string& error)
{
    error.clear();
    std::vector<PanelApiKey> keys;
    std::optional<PanelStore::Statement> list = _store.Prepare(
        "SELECT " + std::string(KeyColumns) + " FROM panel_api_key WHERE owner_user_id = ? ORDER BY id DESC", error);
    if (!list)
        return keys;
    list->Bind(1, ownerUserId);
    while (list->Step(error))
        keys.push_back(*Read(*list));
    return keys;
}

std::vector<PanelApiKey> PanelApiKeys::ListAll(std::string& error)
{
    error.clear();
    std::vector<PanelApiKey> keys;
    std::optional<PanelStore::Statement> list = _store.Prepare(
        "SELECT " + std::string(KeyColumns) + ", panel_user.username FROM panel_api_key"
        " JOIN panel_user ON panel_user.id = panel_api_key.owner_user_id ORDER BY panel_api_key.id DESC", error);
    if (!list)
        return keys;
    while (list->Step(error))
    {
        PanelApiKey key = *Read(*list);
        key.OwnerUsername = list->Text(14);
        keys.push_back(std::move(key));
    }
    return keys;
}

bool PanelApiKeys::CidrsAllow(PanelApiKey const& key, std::string_view address) const
{
    if (key.Cidrs.empty())
        return true;
    std::vector<std::string> problems;
    TrustedProxies const allowed = TrustedProxies::Parse(Join(key.Cidrs), &problems, "cidrs");
    if (!problems.empty())
        return false;
    return allowed.Holds(address);
}

bool PanelApiKeys::NoteUsed(PanelApiKey const& key, std::string_view address, int64 nowEpochMs, std::string& error)
{
    error.clear();
    if (key.LastUsedEpochMs && nowEpochMs - *key.LastUsedEpochMs < LastUsedWriteMs)
        return true;
    std::optional<PanelStore::Statement> note = _store.Prepare(
        "UPDATE panel_api_key SET last_used_epoch_ms = ?, last_used_address = ? WHERE id = ?", error);
    if (!note)
        return false;
    note->Bind(1, nowEpochMs);
    note->Bind(2, address);
    note->Bind(3, key.Id);
    return note->Run(error);
}

void PanelApiKeys::RefusedOutsideCidrs(PanelApiKey const& key, std::string_view address, std::string const& userAgent)
{
    if (!_audit)
        return;
    AuditEvent event;
    event.Name = "panel:apikey.cidr_refused";
    event.Actor = AuditActor::Token;
    event.ActorId = key.PublicId;
    event.ActorName = key.Name;
    event.Address = std::string(address);
    event.UserAgent = userAgent;
    event.Result = AuditResult::Refused;
    event.Reason = "the address is outside the key's allowed CIDRs";
    event.On("api_key", key.PublicId, key.Name).On("panel_user", std::to_string(key.OwnerUserId));
    std::string error;
    if (!_audit(event, {}, error))
        error.clear();
}

PanelApiKeyAuth PanelApiKeys::Authenticate(std::string_view text, std::string_view address, std::string const& userAgent, std::string& error)
{
    error.clear();
    PanelApiKeyAuth auth;
    std::string_view const publicId = PublicIdOf(text);
    if (publicId.empty())
    {
        auth.Result = PanelApiKeyResult::UnknownKey;
        return auth;
    }
    std::optional<std::vector<uint8>> const secret = Base32::Decode(text.substr(text.find('.') + 1), Base32::Alphabet::Rfc4648);
    if (!secret || secret->size() != SecretBytes)
    {
        auth.Result = PanelApiKeyResult::WrongSecret;
        return auth;
    }
    std::optional<PanelStore::Statement> find = _store.Prepare("SELECT secret_hash FROM panel_api_key WHERE public_id = ?", error);
    if (!find)
    {
        auth.Result = PanelApiKeyResult::StoreFailed;
        return auth;
    }
    find->Bind(1, publicId);
    if (!find->Step(error))
    {
        if (!error.empty())
        {
            auth.Result = PanelApiKeyResult::StoreFailed;
            return auth;
        }
        Ambrose::Crypto::ConstantTimeEquals(HashOf(*secret), std::string(64, '0'));
        auth.Result = PanelApiKeyResult::UnknownKey;
        return auth;
    }
    std::string const presented = HashOf(*secret);
    std::string const stored = find->Text(0);
    find.reset();
    if (!Ambrose::Crypto::ConstantTimeEquals(presented, stored))
    {
        auth.Result = PanelApiKeyResult::WrongSecret;
        return auth;
    }
    std::optional<PanelApiKey> key = FindByPublicId(publicId, error);
    if (!key)
    {
        auth.Result = error.empty() ? PanelApiKeyResult::UnknownKey : PanelApiKeyResult::StoreFailed;
        return auth;
    }
    int64 const now = PanelStore::NowEpochMs();
    if (key->ExpiresEpochMs && now >= *key->ExpiresEpochMs)
    {
        auth.Result = PanelApiKeyResult::Expired;
        auth.Key = *key;
        return auth;
    }
    if (key->RevokedEpochMs && !(key->GraceExpiresEpochMs && now < *key->GraceExpiresEpochMs))
    {
        auth.Result = PanelApiKeyResult::Revoked;
        auth.Key = *key;
        return auth;
    }
    if (!CidrsAllow(*key, address))
    {
        RefusedOutsideCidrs(*key, address, userAgent);
        auth.Result = PanelApiKeyResult::OutsideCidrs;
        auth.Key = *key;
        return auth;
    }
    if (!NoteUsed(*key, address, now, error))
    {
        auth.Result = PanelApiKeyResult::StoreFailed;
        return auth;
    }
    auth.Result = PanelApiKeyResult::Ok;
    auth.Key = *key;
    return auth;
}

PanelApiKeyResult PanelApiKeys::Revoke(PanelApiKeyCall const& call, std::string_view publicId, bool anyOwner, std::string& error)
{
    error.clear();
    std::optional<PanelApiKey> key = FindByPublicId(publicId, error);
    if (!key)
        return error.empty() ? PanelApiKeyResult::UnknownKey : PanelApiKeyResult::StoreFailed;
    if (!anyOwner && key->OwnerUserId != call.UserId)
        return PanelApiKeyResult::UnknownKey;
    if (key->RevokedEpochMs && !(key->GraceExpiresEpochMs && PanelStore::NowEpochMs() < *key->GraceExpiresEpochMs))
        return PanelApiKeyResult::Revoked;

    AuditEvent event = KeyEvent(call, "panel:apikey.revoked");
    event.On("api_key", key->PublicId, key->Name).On("panel_user", std::to_string(key->OwnerUserId));
    int64 const id = key->Id;
    auto const change = [&](AuditEvent&, std::string& failure)
    {
        std::optional<PanelStore::Statement> revoke = _store.Prepare(
            "UPDATE panel_api_key SET revoked_epoch_ms = ?, grace_expires_epoch_ms = NULL WHERE id = ?", failure);
        if (!revoke)
            return false;
        revoke->Bind(1, PanelStore::NowEpochMs());
        revoke->Bind(2, id);
        return revoke->Run(failure);
    };
    bool const recorded = _audit ? _audit(event, change, error) : change(event, error);
    return recorded ? PanelApiKeyResult::Ok : PanelApiKeyResult::StoreFailed;
}

PanelApiKeyResult PanelApiKeys::Rotate(PanelApiKeyCall const& call, std::string_view publicId, uint32 graceSeconds, bool anyOwner,
    PanelApiKeyIssued& issued, std::string& error)
{
    error.clear();
    if (graceSeconds > MaxRotateGraceSeconds)
    {
        error = "a rotation takes a grace of at most 3600 seconds";
        return PanelApiKeyResult::BadExpiry;
    }
    std::optional<PanelApiKey> old = FindByPublicId(publicId, error);
    if (!old)
        return error.empty() ? PanelApiKeyResult::UnknownKey : PanelApiKeyResult::StoreFailed;
    if (!anyOwner && old->OwnerUserId != call.UserId)
        return PanelApiKeyResult::UnknownKey;
    if (old->RevokedEpochMs)
        return PanelApiKeyResult::Revoked;

    PanelApiKeyDraft draft;
    draft.OwnerUserId = old->OwnerUserId;
    draft.Name = old->Name;
    draft.Permissions = old->Permissions;
    draft.Scopes = old->Scopes;
    draft.Cidrs = old->Cidrs;
    draft.ExpiresEpochMs = old->ExpiresEpochMs;
    draft.NoExpiry = !old->ExpiresEpochMs;
    std::string problem;
    PanelApiKeyResult const checked = CheckDraft(draft, call.Rights, problem);
    if (checked != PanelApiKeyResult::Ok)
    {
        error = std::string(Explain(checked)) + ": " + problem;
        return checked;
    }
    if (draft.NoExpiry && !NoExpiryAllowed())
    {
        error = "the old key never expires, which the owner's policy no longer allows; revoke it instead";
        return PanelApiKeyResult::BadExpiry;
    }

    std::string newPublicId;
    std::array<uint8, SecretBytes> newSecret{};
    std::string newText;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        if (!Mint(newPublicId, newSecret, newText))
        {
            error = "a key could not be minted";
            return PanelApiKeyResult::StoreFailed;
        }
        std::optional<PanelApiKey> const clash = FindByPublicId(newPublicId, error);
        if (!clash && !error.empty())
            return PanelApiKeyResult::StoreFailed;
        if (!clash)
            break;
        if (attempt == 7)
        {
            error = "a unique public id could not be minted";
            return PanelApiKeyResult::StoreFailed;
        }
    }
    std::string const newHash = HashOf(newSecret);

    AuditEvent event = KeyEvent(call, "panel:apikey.rotated");
    event.On("api_key", old->PublicId, old->Name).On("api_key", newPublicId, draft.Name).On("panel_user", std::to_string(old->OwnerUserId));
    nlohmann::json properties;
    properties["grace_seconds"] = graceSeconds;
    event.Properties = properties.dump();

    int64 const maxPerUser = MaxPerUser();
    int64 const now = PanelStore::NowEpochMs();
    int64 const oldId = old->Id;
    bool done = false;
    bool limited = false;
    auto const change = [&](AuditEvent&, std::string& failure)
    {
        std::optional<PanelStore::Statement> count = _store.Prepare(
            "SELECT COUNT(*) FROM panel_api_key WHERE owner_user_id = ? AND revoked_epoch_ms IS NULL", failure);
        if (!count)
            return false;
        count->Bind(1, old->OwnerUserId);
        if (!count->Step(failure))
            return false;
        if (count->Int64(0) >= maxPerUser)
        {
            limited = true;
            failure = std::string(Explain(PanelApiKeyResult::TooMany));
            return false;
        }
        count.reset();
        std::optional<PanelStore::Statement> insert = _store.Prepare(
            "INSERT INTO panel_api_key (public_id, secret_hash, owner_user_id, name, permissions, scopes, cidrs,"
            " created_epoch_ms, expires_epoch_ms) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", failure);
        if (!insert)
            return false;
        insert->Bind(1, newPublicId);
        insert->Bind(2, newHash);
        insert->Bind(3, old->OwnerUserId);
        insert->Bind(4, draft.Name);
        insert->Bind(5, DumpArray(draft.Permissions));
        insert->Bind(6, DumpArray(draft.Scopes));
        insert->Bind(7, DumpArray(draft.Cidrs));
        insert->Bind(8, now);
        if (draft.ExpiresEpochMs)
            insert->Bind(9, *draft.ExpiresEpochMs);
        else
            insert->BindNull(9);
        if (!insert->Run(failure))
            return false;
        int64 const replacement = _store.LastInsertId();
        std::optional<PanelStore::Statement> retire = _store.Prepare(
            "UPDATE panel_api_key SET revoked_epoch_ms = ?,"
            " grace_expires_epoch_ms = ?, replaced_by = ? WHERE id = ?", failure);
        if (!retire)
            return false;
        retire->Bind(1, now);
        if (graceSeconds > 0)
            retire->Bind(2, now + static_cast<int64>(graceSeconds) * 1000);
        else
            retire->BindNull(2);
        retire->Bind(3, replacement);
        retire->Bind(4, oldId);
        if (!retire->Run(failure))
            return false;
        done = true;
        return true;
    };
    bool const recorded = _audit ? _audit(event, change, error) : change(event, error);
    if (limited)
        return PanelApiKeyResult::TooMany;
    if (!recorded || !done)
        return PanelApiKeyResult::StoreFailed;

    std::optional<PanelApiKey> const made = FindByPublicId(newPublicId, error);
    if (!made)
        return PanelApiKeyResult::StoreFailed;
    issued.Key = *made;
    issued.Text = newText;
    return PanelApiKeyResult::Ok;
}

bool PanelApiKeys::KeyAllows(std::string_view principal, std::string_view permission, std::string_view app, std::string& error)
{
    error.clear();
    if (PanelPermission const* catalog = PanelPermissions::Find(permission))
    {
        if (catalog->SessionOnly)
            return false;
    }
    std::string_view const publicId = PublicIdOfPrincipal(principal);
    if (publicId.empty())
        return false;
    std::optional<PanelApiKey> const key = FindByPublicId(publicId, error);
    if (!key || !error.empty())
        return false;
    if (!key->Live(PanelStore::NowEpochMs()))
        return false;
    bool held = false;
    for (std::string const& granted : key->Permissions)
        if (granted == permission)
            held = true;
    if (!held)
        return false;
    std::string const scope = app.empty() ? "panel" : std::string(app);
    for (std::string const& allowed : key->Scopes)
        if (allowed == scope)
            return true;
    return false;
}

std::set<std::string, std::less<>> PanelApiKeys::EffectiveRights(std::string_view principal,
    std::set<std::string, std::less<>> const& ownerRights, std::string& error)
{
    error.clear();
    std::set<std::string, std::less<>> rights;
    std::string_view const publicId = PublicIdOfPrincipal(principal);
    if (publicId.empty())
        return rights;
    std::optional<PanelApiKey> const key = FindByPublicId(publicId, error);
    if (!key || !error.empty() || !key->Live(PanelStore::NowEpochMs()))
        return rights;
    for (std::string const& permission : key->Permissions)
    {
        if (PanelPermission const* catalog = PanelPermissions::Find(permission))
        {
            if (catalog->SessionOnly)
                continue;
        }
        if (ownerRights.contains(permission))
            rights.insert(permission);
    }
    return rights;
}
