/*
 * Project Ambrose by Imjustchico
 * Makes a link's token, hash, id and expiry before anything is written, so an audit row written first can already name it; keeps it with one insert after dropping rows a day past their expiry and spending an operator's oldest open links of that kind beyond sixteen; spends it with one UPDATE that changes a row only while it is unused and returns it, and reads the row back only to tell a used link from one never issued; tokens are looked up by their hash alone and never written anywhere.
 */

#include "PanelLinks.h"
#include "Base64.h"
#include "CryptoRandom.h"
#include "SHA256.h"

#include <fmt/format.h>

#include <array>
#include <utility>

namespace
{
    constexpr std::string_view Columns =
        "id, kind, user_id, issuer, COALESCE(address, ''), created_epoch_ms, expires_epoch_ms, used_epoch_ms, COALESCE(used_address, '')";

    void BindUser(PanelStore::Statement& statement, int index, std::optional<int64> userId)
    {
        if (userId)
            statement.Bind(index, *userId);
        else
            statement.BindNull(index);
    }

    void BindKinds(PanelStore::Statement& statement, int first, std::vector<PanelLinkKind> const& kinds)
    {
        for (std::size_t index = 0; index < kinds.size(); ++index)
            statement.Bind(first + static_cast<int>(index), PanelLinks::NameOf(kinds[index]));
    }
}

PanelLinks::PanelLinks(PanelStore& store) : _store(store), _clock([] { return PanelStore::NowEpochMs(); })
{
}

void PanelLinks::SetClock(EpochClock clock)
{
    std::lock_guard const lock(_clockMutex);
    _clock = clock ? std::move(clock) : EpochClock([] { return PanelStore::NowEpochMs(); });
}

int64 PanelLinks::Now() const
{
    EpochClock clock;
    {
        std::lock_guard const lock(_clockMutex);
        clock = _clock;
    }
    return clock();
}

std::chrono::milliseconds PanelLinks::LifetimeOf(PanelLinkKind kind) noexcept
{
    switch (kind)
    {
        case PanelLinkKind::OwnerClaim: return std::chrono::minutes(30);
        case PanelLinkKind::Password: return std::chrono::minutes(30);
        case PanelLinkKind::Local: return std::chrono::seconds(60);
        case PanelLinkKind::Pairing: break;
    }
    return std::chrono::minutes(10);
}

std::string_view PanelLinks::NameOf(PanelLinkKind kind) noexcept
{
    switch (kind)
    {
        case PanelLinkKind::OwnerClaim: return "owner_claim";
        case PanelLinkKind::Password: return "password";
        case PanelLinkKind::Local: return "local";
        case PanelLinkKind::Pairing: break;
    }
    return "pairing";
}

bool PanelLinks::ParseKind(std::string_view text, PanelLinkKind& kind) noexcept
{
    for (PanelLinkKind const candidate : { PanelLinkKind::OwnerClaim, PanelLinkKind::Password, PanelLinkKind::Local, PanelLinkKind::Pairing })
    {
        if (NameOf(candidate) == text)
        {
            kind = candidate;
            return true;
        }
    }
    return false;
}

std::string PanelLinks::HashOf(std::string_view token)
{
    return Base64::Encode(SHA256::GetDigestOf(token), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
}

std::string PanelLinks::KindList(std::size_t count)
{
    std::string list;
    for (std::size_t index = 0; index < count; ++index)
        list += index == 0 ? "?" : ", ?";
    return list;
}

PanelLink PanelLinks::Read(PanelStore::Statement const& row)
{
    PanelLink link;
    link.Id = row.Text(0);
    if (!ParseKind(row.Text(1), link.Kind))
        link.Kind = PanelLinkKind::Local;
    if (!row.IsNull(2))
        link.UserId = row.Int64(2);
    link.Issuer = row.Text(3);
    link.Address = row.Text(4);
    link.CreatedEpochMs = row.Int64(5);
    link.ExpiresEpochMs = row.Int64(6);
    if (!row.IsNull(7))
        link.UsedEpochMs = row.Int64(7);
    link.UsedAddress = row.Text(8);
    return link;
}

PanelLinkMinted PanelLinks::Mint(PanelLinkKind kind) const
{
    std::array<uint8, TokenBytes> const bytes = Ambrose::Crypto::GetRandomArray<TokenBytes>();
    PanelLinkMinted minted;
    minted.Token = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    minted.Hash = HashOf(minted.Token);
    minted.Id = minted.Hash.substr(0, IdLength);
    minted.CreatedEpochMs = Now();
    minted.ExpiresEpochMs = minted.CreatedEpochMs + LifetimeOf(kind).count();
    return minted;
}

bool PanelLinks::Keep(PanelLinkMinted const& minted, PanelLinkKind kind, std::optional<int64> userId, std::string_view issuer, std::string_view address, std::string& error)
{
    int64 const now = Now();
    std::optional<PanelStore::Statement> drop = _store.Prepare("DELETE FROM panel_link WHERE expires_epoch_ms < ?", error);
    if (!drop)
        return false;
    drop->Bind(1, now - std::chrono::duration_cast<std::chrono::milliseconds>(Retention).count());
    if (!drop->Run(error))
        return false;
    drop.reset();

    std::optional<PanelStore::Statement> count = _store.Prepare(
        "SELECT COUNT(*) FROM panel_link WHERE user_id IS ? AND kind = ? AND used_epoch_ms IS NULL AND expires_epoch_ms > ?", error);
    if (!count)
        return false;
    BindUser(*count, 1, userId);
    count->Bind(2, NameOf(kind));
    count->Bind(3, now);
    if (!count->Step(error))
    {
        if (error.empty())
            error = "the open links could not be counted";
        return false;
    }
    int64 const held = count->Int64(0);
    count.reset();

    if (held >= static_cast<int64>(MaxOpenPerUser))
    {
        std::optional<PanelStore::Statement> spend = _store.Prepare(
            "UPDATE panel_link SET used_epoch_ms = ? WHERE id IN (SELECT id FROM panel_link WHERE user_id IS ? AND kind = ? AND used_epoch_ms IS NULL"
            " AND expires_epoch_ms > ? ORDER BY created_epoch_ms, rowid LIMIT ?)", error);
        if (!spend)
            return false;
        spend->Bind(1, now);
        BindUser(*spend, 2, userId);
        spend->Bind(3, NameOf(kind));
        spend->Bind(4, now);
        spend->Bind(5, held - static_cast<int64>(MaxOpenPerUser) + 1);
        if (!spend->Run(error))
            return false;
    }

    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT INTO panel_link (id, kind, token_hash, user_id, issuer, address, created_epoch_ms, expires_epoch_ms) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", error);
    if (!insert)
        return false;
    insert->Bind(1, minted.Id);
    insert->Bind(2, NameOf(kind));
    insert->Bind(3, minted.Hash);
    BindUser(*insert, 4, userId);
    insert->Bind(5, issuer);
    if (address.empty())
        insert->BindNull(6);
    else
        insert->Bind(6, address);
    insert->Bind(7, minted.CreatedEpochMs);
    insert->Bind(8, minted.ExpiresEpochMs);
    return insert->Run(error);
}

PanelLinkState PanelLinks::Peek(std::vector<PanelLinkKind> const& kinds, std::string_view token, PanelLink& link, std::string& error)
{
    error.clear();
    if (token.empty() || token.size() > MaxTokenBytes || kinds.empty())
        return PanelLinkState::Unknown;
    std::optional<PanelStore::Statement> rows = _store.Prepare(fmt::format("SELECT {} FROM panel_link WHERE token_hash = ? AND kind IN ({})", Columns, KindList(kinds.size())), error);
    if (!rows)
        return PanelLinkState::StoreFailed;
    rows->Bind(1, HashOf(token));
    BindKinds(*rows, 2, kinds);
    if (!rows->Step(error))
        return error.empty() ? PanelLinkState::Unknown : PanelLinkState::StoreFailed;
    link = Read(*rows);
    if (link.UsedEpochMs)
        return PanelLinkState::Spent;
    return Now() >= link.ExpiresEpochMs ? PanelLinkState::Expired : PanelLinkState::Redeemed;
}

PanelLinkState PanelLinks::Spend(std::vector<PanelLinkKind> const& kinds, std::string_view token, std::string_view address, PanelLink& link, std::string& error)
{
    error.clear();
    if (token.empty() || token.size() > MaxTokenBytes || kinds.empty())
        return PanelLinkState::Unknown;
    int64 const now = Now();
    std::string const hash = HashOf(token);
    std::optional<PanelStore::Statement> update = _store.Prepare(fmt::format(
        "UPDATE panel_link SET used_epoch_ms = ?, used_address = ? WHERE token_hash = ? AND used_epoch_ms IS NULL AND kind IN ({}) RETURNING {}",
        KindList(kinds.size()), Columns), error);
    if (!update)
        return PanelLinkState::StoreFailed;
    update->Bind(1, now);
    if (address.empty())
        update->BindNull(2);
    else
        update->Bind(2, address);
    update->Bind(3, hash);
    BindKinds(*update, 4, kinds);
    bool const found = update->Step(error);
    if (!error.empty())
        return PanelLinkState::StoreFailed;
    if (found)
    {
        link = Read(*update);
        if (!update->Run(error))
            return PanelLinkState::StoreFailed;
        return now >= link.ExpiresEpochMs ? PanelLinkState::Expired : PanelLinkState::Redeemed;
    }
    update.reset();

    std::optional<PanelStore::Statement> rows = _store.Prepare(fmt::format("SELECT {} FROM panel_link WHERE token_hash = ? AND kind IN ({})", Columns, KindList(kinds.size())), error);
    if (!rows)
        return PanelLinkState::StoreFailed;
    rows->Bind(1, hash);
    BindKinds(*rows, 2, kinds);
    if (!rows->Step(error))
        return error.empty() ? PanelLinkState::Unknown : PanelLinkState::StoreFailed;
    link = Read(*rows);
    return PanelLinkState::Spent;
}

bool PanelLinks::SpendEvery(PanelLinkKind kind, std::string& error)
{
    std::optional<PanelStore::Statement> update = _store.Prepare("UPDATE panel_link SET used_epoch_ms = ? WHERE kind = ? AND used_epoch_ms IS NULL", error);
    if (!update)
        return false;
    update->Bind(1, Now());
    update->Bind(2, NameOf(kind));
    return update->Run(error);
}

bool PanelLinks::AnyOpen(PanelLinkKind kind, std::string& error)
{
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT 1 FROM panel_link WHERE kind = ? AND used_epoch_ms IS NULL AND expires_epoch_ms > ? LIMIT 1", error);
    if (!rows)
        return false;
    rows->Bind(1, NameOf(kind));
    rows->Bind(2, Now());
    return rows->Step(error);
}

std::vector<PanelLink> PanelLinks::List(std::string& error)
{
    std::vector<PanelLink> links;
    std::optional<PanelStore::Statement> rows = _store.Prepare(fmt::format("SELECT {} FROM panel_link ORDER BY created_epoch_ms DESC, id", Columns), error);
    if (!rows)
        return links;
    while (rows->Step(error))
        links.push_back(Read(*rows));
    return links;
}
