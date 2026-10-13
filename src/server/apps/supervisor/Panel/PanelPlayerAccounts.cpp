/*
 * Project Ambrose by Imjustchico
 * Writes player account state, expiring hashed one-use links and a UTC-day mail cap through prepared panel-store statements; callers serialize each state change with its audit row using Panel::Record.
 */

#include "PanelPlayerAccounts.h"

#include "Base64.h"
#include "CryptoRandom.h"
#include "SHA256.h"

#include <algorithm>

std::string_view PanelPlayerAccounts::NameOf(PlayerTokenKind kind) noexcept
{
    switch (kind)
    {
        case PlayerTokenKind::Verification: return "verify_email";
        case PlayerTokenKind::PasswordReset: return "password_reset";
    }
    return {};
}

std::string PanelPlayerAccounts::HashOf(std::string_view value)
{
    return Base64::Encode(SHA256::GetDigestOf(value), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
}

PlayerTokenMinted PanelPlayerAccounts::Mint()
{
    PlayerTokenMinted minted;
    minted.Token = Base64::Encode(Ambrose::Crypto::GetRandomArray<32>(), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    minted.Hash = HashOf(minted.Token);
    return minted;
}

bool PanelPlayerAccounts::Register(uint64 accountId, std::string_view username, std::string_view state, int64 now, std::string& error)
{
    std::optional<PanelStore::Statement> row = _store.Prepare(
        "INSERT INTO player_registration (account_id, username, state, created_epoch_ms, updated_epoch_ms) VALUES (?, ?, ?, ?, ?)", error);
    if (!row)
        return false;
    row->Bind(1, static_cast<int64>(accountId));
    row->Bind(2, username);
    row->Bind(3, state);
    row->Bind(4, now);
    row->Bind(5, now);
    return row->Run(error);
}

bool PanelPlayerAccounts::SetState(uint64 accountId, std::string_view state, int64 now, std::string& error)
{
    std::optional<PanelStore::Statement> row = _store.Prepare(
        "UPDATE player_registration SET state = ?, updated_epoch_ms = ? WHERE account_id = ?", error);
    if (!row)
        return false;
    row->Bind(1, state);
    row->Bind(2, now);
    row->Bind(3, static_cast<int64>(accountId));
    if (!row->Run(error))
        return false;
    if (_store.Changed() != 1)
    {
        error = "the player registration no longer exists";
        return false;
    }
    return true;
}

bool PanelPlayerAccounts::KeepToken(PlayerTokenKind kind, uint64 accountId, PlayerTokenMinted const& token, std::string_view addressHash, int64 created, int64 expires, std::string& error)
{
    std::optional<PanelStore::Statement> old = _store.Prepare(
        "UPDATE player_account_token SET used_epoch_ms = ? WHERE account_id = ? AND kind = ? AND used_epoch_ms IS NULL", error);
    if (!old)
        return false;
    old->Bind(1, created);
    old->Bind(2, static_cast<int64>(accountId));
    old->Bind(3, NameOf(kind));
    if (!old->Run(error))
        return false;

    std::optional<PanelStore::Statement> row = _store.Prepare(
        "INSERT INTO player_account_token (id, kind, account_id, token_hash, address_hash, created_epoch_ms, expires_epoch_ms) VALUES (?, ?, ?, ?, ?, ?, ?)", error);
    if (!row)
        return false;
    row->Bind(1, token.Hash.substr(0, 16));
    row->Bind(2, NameOf(kind));
    row->Bind(3, static_cast<int64>(accountId));
    row->Bind(4, token.Hash);
    row->Bind(5, addressHash);
    row->Bind(6, created);
    row->Bind(7, expires);
    return row->Run(error);
}

PlayerTokenState PanelPlayerAccounts::Peek(PlayerTokenKind kind, std::string_view token, int64 now, PlayerToken& found, std::string& error)
{
    if (token.empty() || token.size() > 512)
        return PlayerTokenState::Unknown;
    std::optional<PanelStore::Statement> row = _store.Prepare(
        "SELECT account_id, expires_epoch_ms, used_epoch_ms FROM player_account_token WHERE kind = ? AND token_hash = ?", error);
    if (!row)
        return PlayerTokenState::StoreFailed;
    row->Bind(1, NameOf(kind));
    row->Bind(2, HashOf(token));
    if (!row->Step(error))
        return error.empty() ? PlayerTokenState::Unknown : PlayerTokenState::StoreFailed;
    found.AccountId = static_cast<uint64>(row->Int64(0));
    found.ExpiresEpochMs = row->Int64(1);
    if (!row->IsNull(2))
        return PlayerTokenState::Spent;
    if (found.ExpiresEpochMs <= now)
        return PlayerTokenState::Expired;
    return PlayerTokenState::Valid;
}

bool PanelPlayerAccounts::Spend(PlayerTokenKind kind, std::string_view token, std::string_view addressHash, int64 now, std::string& error)
{
    std::optional<PanelStore::Statement> row = _store.Prepare(
        "UPDATE player_account_token SET used_epoch_ms = ?, used_address_hash = ? WHERE kind = ? AND token_hash = ? AND used_epoch_ms IS NULL AND expires_epoch_ms > ?", error);
    if (!row)
        return false;
    row->Bind(1, now);
    row->Bind(2, addressHash);
    row->Bind(3, NameOf(kind));
    row->Bind(4, HashOf(token));
    row->Bind(5, now);
    if (!row->Run(error))
        return false;
    if (_store.Changed() != 1)
    {
        error = "the player link was already used, has expired or does not exist";
        return false;
    }
    return true;
}

bool PanelPlayerAccounts::Restore(PlayerTokenKind kind, std::string_view token, int64 now, std::string& error)
{
    std::optional<PanelStore::Statement> row = _store.Prepare(
        "UPDATE player_account_token SET used_epoch_ms = NULL, used_address_hash = NULL WHERE kind = ? AND token_hash = ? AND used_epoch_ms IS NOT NULL AND expires_epoch_ms > ?", error);
    if (!row)
        return false;
    row->Bind(1, NameOf(kind));
    row->Bind(2, HashOf(token));
    row->Bind(3, now);
    if (!row->Run(error))
        return false;
    if (_store.Changed() != 1)
    {
        error = "the player link could not be restored because it expired or changed";
        return false;
    }
    return true;
}

bool PanelPlayerAccounts::TakeDailyMail(std::string_view addressHash, int64 epochMs, std::string& error)
{
    int64 const day = epochMs / 86400000;
    std::optional<PanelStore::Statement> row = _store.Prepare(
        "INSERT INTO player_mail_daily (address_hash, utc_day, count) VALUES (?, ?, 1) "
        "ON CONFLICT (address_hash, utc_day) DO UPDATE SET count = count + 1 WHERE count < ?", error);
    if (!row)
        return false;
    row->Bind(1, addressHash);
    row->Bind(2, day);
    row->Bind(3, DailyMailLimit);
    if (!row->Run(error))
        return false;
    if (_store.Changed() != 1)
    {
        error = "the daily mail limit for this address has been reached";
        return false;
    }
    return true;
}

std::optional<PlayerRegistration> PanelPlayerAccounts::Find(uint64 accountId, std::string& error)
{
    std::optional<PanelStore::Statement> row = _store.Prepare(
        "SELECT account_id, username, state, created_epoch_ms, updated_epoch_ms FROM player_registration WHERE account_id = ?", error);
    if (!row)
        return std::nullopt;
    row->Bind(1, static_cast<int64>(accountId));
    if (!row->Step(error))
        return std::nullopt;
    return PlayerRegistration{
        static_cast<uint64>(row->Int64(0)),
        row->Text(1),
        row->Text(2),
        row->Int64(3),
        row->Int64(4)
    };
}

std::vector<PlayerRegistration> PanelPlayerAccounts::List(std::size_t limit, std::string& error)
{
    std::vector<PlayerRegistration> registrations;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT account_id, username, state, created_epoch_ms, updated_epoch_ms FROM player_registration ORDER BY created_epoch_ms DESC LIMIT ?", error);
    if (!rows)
        return registrations;
    rows->Bind(1, static_cast<int64>(std::clamp<std::size_t>(limit, 1, 500)));
    while (rows->Step(error))
    {
        registrations.push_back({
            static_cast<uint64>(rows->Int64(0)),
            rows->Text(1),
            rows->Text(2),
            rows->Int64(3),
            rows->Int64(4)
        });
    }
    return registrations;
}
