/*
 * Project Ambrose by Imjustchico
 * One-time invite links that bring new operators in: a token of 32 random bytes kept only as its SHA-256, carrying the scope, role and extra grants the new operator gets, an optional username it was made for, who made it and when it runs out after 72 hours by default, spent by one conditional statement so two claims of the same link cannot both win, listed and revoked by holders of users.invite, and held back per inviter and per scope so one operator cannot flood the panel with links.
 */

#ifndef AMBROSE_PANELINVITES_H
#define AMBROSE_PANELINVITES_H

#include "PanelRateLimit.h"
#include "PanelStore.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class PanelInviteResult : uint8
{
    Ok,
    BadScope,
    BadRole,
    BadGrant,
    RateLimitedInviter,
    RateLimitedScope,
    StoreFailed
};

enum class PanelInviteSpend : uint8
{
    Spent,
    Expired,
    Invalid,
    StoreFailed
};

struct PanelInvite
{
    int64 Id = 0;
    std::string Scope = {};
    std::string Role = {};
    std::vector<std::string> Grants = {};
    std::string TargetUsername = {};
    int64 CreatedBy = 0;
    int64 CreatedEpochMs = 0;
    int64 ExpiresEpochMs = 0;
    std::optional<int64> UsedEpochMs = {};
    std::optional<int64> UsedBy = {};
    std::optional<int64> RevokedEpochMs = {};
    std::string Note = {};
};

struct PanelInviteMinted
{
    std::string Token = {};
    int64 Id = 0;
    int64 ExpiresEpochMs = 0;
};

struct PanelInviteDraft
{
    std::string Scope = {};
    std::string Role = {};
    std::vector<std::string> Grants = {};
    std::string TargetUsername = {};
    int64 CreatedBy = 0;
    int64 TtlMs = 0;
    std::string Note = {};
};

class PanelInvites
{
public:
    using EpochClock = std::function<int64()>;

    static constexpr std::size_t TokenBytes = 32;
    static constexpr std::size_t TokenLength = 43;
    static constexpr std::size_t MaxTokenBytes = 512;
    static constexpr std::chrono::hours DefaultTtl{ 72 };
    static constexpr uint32 MaxPerInviterPerHour = 10;
    static constexpr uint32 MaxPerScopePerHour = 30;
    static constexpr std::size_t MaxNoteChars = 280;

    explicit PanelInvites(PanelStore& store);

    PanelInvites(PanelInvites const&) = delete;
    PanelInvites& operator=(PanelInvites const&) = delete;

    void SetClock(EpochClock clock);
    int64 Now() const;

    static std::string HashOf(std::string_view token);
    static bool ValidScope(std::string_view scope) noexcept;

    PanelInviteResult Create(PanelInviteDraft draft, PanelInviteMinted& minted, std::string& error);
    std::vector<PanelInvite> List(std::string& error) const;
    bool Revoke(int64 id, int64 nowEpochMs, std::string& error);
    bool MarkUsed(int64 id, int64 usedBy, int64 nowEpochMs, std::string& error);
    bool Find(std::string_view token, PanelInvite& invite, std::string& error) const;
    bool FindLive(std::string_view token, PanelInvite& invite, std::string& error) const;
    PanelInviteSpend Spend(std::string_view token, int64 usedBy, PanelInvite& invite, std::string& error);

private:
    static PanelInvite Read(PanelStore::Statement const& row);
    static std::string GrantsJson(std::vector<std::string> const& grants);

    PanelStore& _store;
    PanelRateLimit _inviterLimit;
    PanelRateLimit _scopeLimit;
    mutable std::mutex _clockMutex;
    EpochClock _clock;
};

#endif
