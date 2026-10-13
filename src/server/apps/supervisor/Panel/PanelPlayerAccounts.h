/*
 * Project Ambrose by Imjustchico
 * Keeps player registrations, single-use email verification and password-reset tokens, and a per-address daily mail budget in the panel store, storing only token and address hashes so a copied store reveals no bearer credentials or raw delivery addresses.
 */

#ifndef AMBROSE_PANELPLAYERACCOUNTS_H
#define AMBROSE_PANELPLAYERACCOUNTS_H

#include "PanelStore.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class PlayerTokenKind : uint8
{
    Verification,
    PasswordReset
};

enum class PlayerTokenState : uint8
{
    Valid,
    Expired,
    Spent,
    Unknown,
    StoreFailed
};

struct PlayerToken
{
    uint64 AccountId = 0;
    int64 ExpiresEpochMs = 0;
};

struct PlayerTokenMinted
{
    std::string Token;
    std::string Hash;
};

struct PlayerRegistration
{
    uint64 AccountId = 0;
    std::string Username;
    std::string State;
    int64 CreatedEpochMs = 0;
    int64 UpdatedEpochMs = 0;
};

class PanelPlayerAccounts
{
public:
    static constexpr uint32 DailyMailLimit = 5;

    explicit PanelPlayerAccounts(PanelStore& store) : _store(store) {}

    static std::string_view NameOf(PlayerTokenKind kind) noexcept;
    static PlayerTokenMinted Mint();
    static std::string HashOf(std::string_view value);

    bool Register(uint64 accountId, std::string_view username, std::string_view state, int64 now, std::string& error);
    bool SetState(uint64 accountId, std::string_view state, int64 now, std::string& error);
    bool KeepToken(PlayerTokenKind kind, uint64 accountId, PlayerTokenMinted const& token, std::string_view addressHash, int64 created, int64 expires, std::string& error);
    PlayerTokenState Peek(PlayerTokenKind kind, std::string_view token, int64 now, PlayerToken& found, std::string& error);
    bool Spend(PlayerTokenKind kind, std::string_view token, std::string_view addressHash, int64 now, std::string& error);
    bool Restore(PlayerTokenKind kind, std::string_view token, int64 now, std::string& error);
    bool TakeDailyMail(std::string_view addressHash, int64 epochMs, std::string& error);
    std::optional<PlayerRegistration> Find(uint64 accountId, std::string& error);
    std::vector<PlayerRegistration> List(std::size_t limit, std::string& error);

private:
    PanelStore& _store;
};

#endif
