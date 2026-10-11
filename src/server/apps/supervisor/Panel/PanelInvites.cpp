/*
 * Project Ambrose by Imjustchico
 * Makes an invite's token, its SHA-256 and its expiry before anything is written, so an audit row written first can already name it; keeps it with one insert after the per-inviter and per-scope buckets have had their say; spends it with one UPDATE that changes a row only while it is unused, unrevoked and unexpired and returns it, so a second claim of the same link finds nothing to spend; and reads rows back to list them and to tell a live link from a dead one without ever writing a token anywhere.
 */

#include "PanelInvites.h"
#include "Base64.h"
#include "CryptoRandom.h"
#include "PanelPermissions.h"
#include "PanelUsers.h"
#include "SHA256.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    constexpr std::string_view Columns =
        "id, scope, COALESCE(role, ''), grants, COALESCE(target_username, ''), COALESCE(created_by, 0),"
        " created_epoch_ms, expires_epoch_ms, used_epoch_ms, used_by, revoked_epoch_ms, note";

    std::string HexOf(SHA256::Digest const& digest)
    {
        std::string text;
        text.reserve(digest.size() * 2);
        for (uint8 byte : digest)
            fmt::format_to(std::back_inserter(text), "{:02x}", byte);
        return text;
    }
}

PanelInvites::PanelInvites(PanelStore& store)
    : _store(store), _clock([] { return PanelStore::NowEpochMs(); })
{
    _inviterLimit.SetLimits(MaxPerInviterPerHour, static_cast<double>(MaxPerInviterPerHour) / 3600.0);
    _scopeLimit.SetLimits(MaxPerScopePerHour, static_cast<double>(MaxPerScopePerHour) / 3600.0);
}

void PanelInvites::SetClock(EpochClock clock)
{
    std::lock_guard const lock(_clockMutex);
    _clock = clock ? std::move(clock) : EpochClock([] { return PanelStore::NowEpochMs(); });
}

int64 PanelInvites::Now() const
{
    EpochClock clock;
    {
        std::lock_guard const lock(_clockMutex);
        clock = _clock;
    }
    return clock();
}

std::string PanelInvites::HashOf(std::string_view token)
{
    return HexOf(SHA256::GetDigestOf(token));
}

bool PanelInvites::ValidScope(std::string_view scope) noexcept
{
    if (scope.empty() || scope.size() > 64)
        return false;
    if (scope.front() < 'a' || scope.front() > 'z')
        if (scope.front() < '0' || scope.front() > '9')
            return false;
    return std::all_of(scope.begin(), scope.end(), [](char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

PanelInvite PanelInvites::Read(PanelStore::Statement const& row)
{
    PanelInvite invite;
    invite.Id = row.Int64(0);
    invite.Scope = row.Text(1);
    invite.Role = row.Text(2);
    nlohmann::json const grants = nlohmann::json::parse(row.Text(3), nullptr, false);
    if (grants.is_array())
        for (nlohmann::json const& grant : grants)
            if (grant.is_string())
                invite.Grants.push_back(grant.get<std::string>());
    invite.TargetUsername = row.Text(4);
    invite.CreatedBy = row.Int64(5);
    invite.CreatedEpochMs = row.Int64(6);
    invite.ExpiresEpochMs = row.Int64(7);
    if (!row.IsNull(8))
        invite.UsedEpochMs = row.Int64(8);
    if (!row.IsNull(9))
        invite.UsedBy = row.Int64(9);
    if (!row.IsNull(10))
        invite.RevokedEpochMs = row.Int64(10);
    invite.Note = row.Text(11);
    return invite;
}

std::string PanelInvites::GrantsJson(std::vector<std::string> const& grants)
{
    nlohmann::json array = nlohmann::json::array();
    for (std::string const& grant : grants)
        array.push_back(grant);
    return array.dump();
}

PanelInviteResult PanelInvites::Create(PanelInviteDraft draft, PanelInviteMinted& minted, std::string& error)
{
    error.clear();
    if (!ValidScope(draft.Scope))
        return PanelInviteResult::BadScope;
    if (!draft.Role.empty())
    {
        PanelRole role = PanelRole::Viewer;
        if (!PanelPermissions::ParseRole(draft.Role, role))
            return PanelInviteResult::BadRole;
    }
    std::sort(draft.Grants.begin(), draft.Grants.end());
    draft.Grants.erase(std::unique(draft.Grants.begin(), draft.Grants.end()), draft.Grants.end());
    for (std::string const& grant : draft.Grants)
        if (!PanelPermissions::Holds(grant))
            return PanelInviteResult::BadGrant;
    if (!draft.TargetUsername.empty())
        draft.TargetUsername = PanelUsers::Fold(draft.TargetUsername);
    if (draft.Note.size() > MaxNoteChars)
        draft.Note.resize(MaxNoteChars);

    PanelRateVerdict const byInviter = _inviterLimit.Take(fmt::format("inviter:{}", draft.CreatedBy), "", 1);
    if (!byInviter.Allowed)
        return PanelInviteResult::RateLimitedInviter;
    PanelRateVerdict const byScope = _scopeLimit.Take("", fmt::format("scope:{}", draft.Scope), 1);
    if (!byScope.Allowed)
        return PanelInviteResult::RateLimitedScope;

    std::array<uint8, TokenBytes> const bytes = Ambrose::Crypto::GetRandomArray<TokenBytes>();
    minted.Token = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    int64 const now = Now();
    int64 const ttl = draft.TtlMs > 0 ? draft.TtlMs : std::chrono::duration_cast<std::chrono::milliseconds>(DefaultTtl).count();
    minted.ExpiresEpochMs = now + ttl;

    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT INTO panel_invite (token_hash, scope, role, grants, target_username, created_by, created_epoch_ms, expires_epoch_ms, note)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
        error);
    if (!insert)
        return PanelInviteResult::StoreFailed;
    insert->Bind(1, HashOf(minted.Token));
    insert->Bind(2, draft.Scope);
    if (draft.Role.empty())
        insert->BindNull(3);
    else
        insert->Bind(3, draft.Role);
    insert->Bind(4, GrantsJson(draft.Grants));
    if (draft.TargetUsername.empty())
        insert->BindNull(5);
    else
        insert->Bind(5, draft.TargetUsername);
    if (draft.CreatedBy == 0)
        insert->BindNull(6);
    else
        insert->Bind(6, draft.CreatedBy);
    insert->Bind(7, now);
    insert->Bind(8, minted.ExpiresEpochMs);
    insert->Bind(9, draft.Note);
    if (!insert->Run(error))
        return PanelInviteResult::StoreFailed;
    insert.reset();

    std::optional<PanelStore::Statement> id = _store.Prepare("SELECT last_insert_rowid()", error);
    if (!id || !id->Step(error))
        return PanelInviteResult::StoreFailed;
    minted.Id = id->Int64(0);
    return PanelInviteResult::Ok;
}

std::vector<PanelInvite> PanelInvites::List(std::string& error) const
{
    error.clear();
    std::vector<PanelInvite> invites;
    std::optional<PanelStore::Statement> query = _store.Prepare(
        fmt::format("SELECT {} FROM panel_invite ORDER BY created_epoch_ms DESC, id DESC", Columns), error);
    if (!query)
        return invites;
    while (query->Step(error))
        invites.push_back(Read(*query));
    return invites;
}

bool PanelInvites::Revoke(int64 id, int64 nowEpochMs, std::string& error)
{
    error.clear();
    std::optional<PanelStore::Statement> update = _store.Prepare(
        "UPDATE panel_invite SET revoked_epoch_ms = ? WHERE id = ? AND used_epoch_ms IS NULL AND revoked_epoch_ms IS NULL", error);
    if (!update)
        return false;
    update->Bind(1, nowEpochMs);
    update->Bind(2, id);
    if (!update->Run(error))
        return false;
    return _store.Changed() > 0;
}

bool PanelInvites::MarkUsed(int64 id, int64 usedBy, int64 nowEpochMs, std::string& error)
{
    error.clear();
    std::optional<PanelStore::Statement> update = _store.Prepare(
        "UPDATE panel_invite SET used_epoch_ms = ?, used_by = ? WHERE id = ? AND used_epoch_ms IS NULL AND revoked_epoch_ms IS NULL", error);
    if (!update)
        return false;
    update->Bind(1, nowEpochMs);
    if (usedBy == 0)
        update->BindNull(2);
    else
        update->Bind(2, usedBy);
    update->Bind(3, id);
    if (!update->Run(error))
        return false;
    return _store.Changed() > 0;
}

bool PanelInvites::Find(std::string_view token, PanelInvite& invite, std::string& error) const
{
    error.clear();
    if (token.empty() || token.size() > MaxTokenBytes)
        return false;
    std::optional<PanelStore::Statement> query = _store.Prepare(
        fmt::format("SELECT {} FROM panel_invite WHERE token_hash = ?", Columns), error);
    if (!query)
        return false;
    query->Bind(1, HashOf(token));
    if (!query->Step(error) || !error.empty())
        return false;
    invite = Read(*query);
    return true;
}

bool PanelInvites::FindLive(std::string_view token, PanelInvite& invite, std::string& error) const
{
    error.clear();
    if (token.empty() || token.size() > MaxTokenBytes)
        return false;
    std::optional<PanelStore::Statement> query = _store.Prepare(
        fmt::format("SELECT {} FROM panel_invite WHERE token_hash = ?", Columns), error);
    if (!query)
        return false;
    query->Bind(1, HashOf(token));
    if (!query->Step(error) || !error.empty())
        return false;
    invite = Read(*query);
    query.reset();
    if (invite.UsedEpochMs || invite.RevokedEpochMs || Now() >= invite.ExpiresEpochMs)
        return false;
    return true;
}

PanelInviteSpend PanelInvites::Spend(std::string_view token, int64 usedBy, PanelInvite& invite, std::string& error)
{
    error.clear();
    if (token.empty() || token.size() > MaxTokenBytes)
        return PanelInviteSpend::Invalid;
    int64 const now = Now();
    std::optional<PanelStore::Statement> update = _store.Prepare(
        fmt::format("UPDATE panel_invite SET used_epoch_ms = ?, used_by = ? WHERE token_hash = ? AND used_epoch_ms IS NULL"
                    " AND revoked_epoch_ms IS NULL RETURNING {}",
            Columns),
        error);
    if (!update)
        return PanelInviteSpend::StoreFailed;
    update->Bind(1, now);
    if (usedBy == 0)
        update->BindNull(2);
    else
        update->Bind(2, usedBy);
    update->Bind(3, HashOf(token));
    bool const found = update->Step(error);
    if (!error.empty())
        return PanelInviteSpend::StoreFailed;
    if (found)
    {
        invite = Read(*update);
        if (!update->Run(error))
            return PanelInviteSpend::StoreFailed;
        return now >= invite.ExpiresEpochMs ? PanelInviteSpend::Expired : PanelInviteSpend::Spent;
    }
    update.reset();

    std::optional<PanelStore::Statement> query = _store.Prepare(
        fmt::format("SELECT {} FROM panel_invite WHERE token_hash = ?", Columns), error);
    if (!query)
        return PanelInviteSpend::StoreFailed;
    query->Bind(1, HashOf(token));
    if (!query->Step(error) || !error.empty())
        return PanelInviteSpend::Invalid;
    invite = Read(*query);
    if (invite.UsedEpochMs || invite.RevokedEpochMs)
        return PanelInviteSpend::Invalid;
    return PanelInviteSpend::Expired;
}

bool PanelInvites::ForgetRateLimits()
{
    _inviterLimit.Clear();
    _scopeLimit.Clear();
    return true;
}
