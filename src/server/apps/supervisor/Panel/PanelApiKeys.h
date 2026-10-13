/*
 * Project Ambrose by Imjustchico
 * The panel's personal API keys: a key reads amb_ plus a 12-character public id plus a dot plus a 32-byte secret in base32, only a SHA-256 of the secret is stored and compared in constant time, and the full text is handed out once at creation and never again; each key carries its owner's id, a name, a permission subset, the scopes it may touch, the CIDRs it may come from for IPv4 and IPv6, when it was created and when it expires, when it was revoked and until when a rotated key's old secret still answers, and when and from where it was last used, written at most once a minute; creating one counts the owner's live keys in the same transaction as the insert against the per-user limit, rotating one makes a replacement with the same permissions, scopes and CIDRs and an optional short grace for the old secret, and revoking one ends it on the next request; authenticating one refuses an unknown id or a wrong secret without saying which, an expired or revoked key past its grace, and an address outside its CIDRs with an audit row naming the key id; rights at request time are the key's subset and scopes met against its owner's current permissions, so demoting the owner shrinks every key at once.
 */

#ifndef AMBROSE_PANELAPIKEYS_H
#define AMBROSE_PANELAPIKEYS_H

#include "PanelStore.h"
#include "Types.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct AuditEvent;

enum class PanelApiKeyResult : uint8
{
    Ok,
    TooMany,
    BadName,
    BadPermissions,
    BadScopes,
    BadCidrs,
    BadExpiry,
    UnknownKey,
    WrongSecret,
    Revoked,
    Expired,
    OutsideCidrs,
    StoreFailed
};

struct PanelApiKey
{
    int64 Id = 0;
    std::string PublicId;
    int64 OwnerUserId = 0;
    std::string OwnerUsername;
    std::string Name;
    std::vector<std::string> Permissions;
    std::vector<std::string> Scopes;
    std::vector<std::string> Cidrs;
    int64 CreatedEpochMs = 0;
    std::optional<int64> ExpiresEpochMs;
    std::optional<int64> RevokedEpochMs;
    std::optional<int64> GraceExpiresEpochMs;
    std::optional<int64> ReplacedBy;
    std::optional<int64> LastUsedEpochMs;
    std::string LastUsedAddress;

    bool Live(int64 nowEpochMs) const noexcept;
};

struct PanelApiKeyIssued
{
    PanelApiKey Key;
    std::string Text;
};

struct PanelApiKeyDraft
{
    int64 OwnerUserId = 0;
    std::string Name;
    std::vector<std::string> Permissions;
    std::vector<std::string> Scopes;
    std::vector<std::string> Cidrs;
    std::optional<int64> ExpiresEpochMs;
    bool NoExpiry = false;
};

struct PanelApiKeyCall
{
    int64 UserId = 0;
    std::string Username;
    std::string Address;
    std::string UserAgent;
    std::string ViaKey;
    std::set<std::string, std::less<>> Rights;
};

struct PanelApiKeyAuth
{
    PanelApiKeyResult Result = PanelApiKeyResult::Ok;
    PanelApiKey Key;
};

class PanelApiKeys
{
public:
    using Setting = std::function<std::string(std::string_view key)>;
    using Audit = std::function<bool(AuditEvent&, std::function<bool(AuditEvent&, std::string&)> const&, std::string&)>;

    static constexpr std::string_view Prefix = "amb_";
    static constexpr std::size_t PublicIdLength = 12;
    static constexpr std::size_t SecretBytes = 32;
    static constexpr std::size_t MaxNameLength = 64;
    static constexpr int64 DefaultMaxPerUser = 25;
    static constexpr int64 DefaultExpiryDays = 90;
    static constexpr int64 LastUsedWriteMs = 60000;
    static constexpr uint32 MaxRotateGraceSeconds = 3600;

    explicit PanelApiKeys(PanelStore& store, Setting setting = {}, Audit audit = {});

    PanelApiKeys(PanelApiKeys const&) = delete;
    PanelApiKeys& operator=(PanelApiKeys const&) = delete;

    static bool IsKeyText(std::string_view text) noexcept;
    static std::string_view PublicIdOf(std::string_view text) noexcept;
    static bool IsKeyPrincipal(std::string_view principal) noexcept;
    static std::string_view PublicIdOfPrincipal(std::string_view principal) noexcept;
    static std::string PrincipalOf(std::string_view publicId);
    static std::string_view Explain(PanelApiKeyResult result) noexcept;

    PanelApiKeyResult Create(PanelApiKeyCall const& call, PanelApiKeyDraft const& draft, PanelApiKeyIssued& issued, std::string& error);
    PanelApiKeyResult Revoke(PanelApiKeyCall const& call, std::string_view publicId, bool anyOwner, std::string& error);
    PanelApiKeyResult Rotate(PanelApiKeyCall const& call, std::string_view publicId, uint32 graceSeconds, bool anyOwner, PanelApiKeyIssued& issued, std::string& error);
    PanelApiKeyAuth Authenticate(std::string_view text, std::string_view address, std::string const& userAgent, std::string& error);

    std::optional<PanelApiKey> FindByPublicId(std::string_view publicId, std::string& error);
    std::optional<int64> OwnerOf(std::string_view principal, std::string& error);
    std::vector<PanelApiKey> ListOf(int64 ownerUserId, std::string& error);
    std::vector<PanelApiKey> ListAll(std::string& error);
    bool KeyAllows(std::string_view principal, std::string_view permission, std::string_view app, std::string& error);
    std::set<std::string, std::less<>> EffectiveRights(std::string_view principal, std::set<std::string, std::less<>> const& ownerRights, std::string& error);

    int64 MaxPerUser() const;
    int64 ExpiryDays() const;
    bool NoExpiryAllowed() const;

private:
    static std::optional<PanelApiKey> Read(PanelStore::Statement const& row);
    static std::string HashOf(std::span<uint8 const> secret);
    static bool Mint(std::string& publicId, std::array<uint8, SecretBytes>& secret, std::string& text);

    PanelApiKeyResult CheckDraft(PanelApiKeyDraft const& draft, std::set<std::string, std::less<>> const& grantable, std::string& problem) const;
    bool CidrsAllow(PanelApiKey const& key, std::string_view address) const;
    bool NoteUsed(PanelApiKey const& key, std::string_view address, int64 nowEpochMs, std::string& error);
    void RefusedOutsideCidrs(PanelApiKey const& key, std::string_view address, std::string const& userAgent);
    int64 SettingOr(std::string_view key, int64 fallback) const;

    PanelStore& _store;
    Setting _setting;
    Audit _audit;
};

#endif
