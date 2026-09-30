/*
 * Project Ambrose by Imjustchico
 * The panel's one store of single-use sign-in links, the owner claim and password links of 17.46 beside the local and pairing links a desktop program opens a panel with: a token is 32 random bytes written as base64url and kept only as its SHA-256, whose first sixteen characters name the link, each kind lasts as long as it is meant to by a wall clock the caller may replace, so a link printed before a restart still works after it, a spend is one conditional statement that answers whether the link was redeemed, had run out, was already used or was never issued, an operator holds at most sixteen open links of a kind with the oldest spent to make room, and rows a day past their expiry are dropped; every caller holds the panel's store lock, so a spend never lands inside another request's transaction.
 */

#ifndef AMBROSE_PANELLINKS_H
#define AMBROSE_PANELLINKS_H

#include "PanelAudit.h"
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

enum class PanelLinkKind : uint8
{
    OwnerClaim,
    Password,
    Local,
    Pairing
};

enum class PanelLinkState : uint8
{
    Redeemed,
    Expired,
    Spent,
    Unknown,
    StoreFailed
};

struct PanelLink
{
    std::string Id = {};
    PanelLinkKind Kind = PanelLinkKind::Local;
    std::optional<int64> UserId = {};
    std::string Issuer = {};
    std::string Address = {};
    int64 CreatedEpochMs = 0;
    int64 ExpiresEpochMs = 0;
    std::optional<int64> UsedEpochMs = {};
    std::string UsedAddress = {};
};

struct PanelLinkMinted
{
    std::string Token = {};
    std::string Id = {};
    std::string Hash = {};
    int64 CreatedEpochMs = 0;
    int64 ExpiresEpochMs = 0;
};

struct PanelLinkAsk
{
    PanelLinkKind Kind = PanelLinkKind::Local;
    std::string Username = {};
    std::string Address = {};
};

struct PanelLinkIssuer
{
    AuditActor Actor = AuditActor::System;
    std::string Id = {};
    std::string Name = {};
    std::string RemoteAddress = {};
};

struct PanelLinkIssued
{
    PanelLinkKind Kind = PanelLinkKind::Local;
    std::string Token = {};
    std::string Id = {};
    int64 UserId = 0;
    std::string Username = {};
    bool CreatedOwner = false;
    int64 ExpiresEpochMs = 0;
    std::string Url = {};
    std::string Host = {};
    uint16 Port = 0;
    std::string Fingerprint = {};
};

struct PanelLinkRefusal
{
    int Status = 0;
    std::string Code = {};
    std::string Message = {};
    std::string Field = {};
};

class PanelLinks
{
public:
    using EpochClock = std::function<int64()>;

    static constexpr std::size_t TokenBytes = 32;
    static constexpr std::size_t TokenLength = 43;
    static constexpr std::size_t IdLength = 16;
    static constexpr std::size_t MaxTokenBytes = 512;
    static constexpr std::size_t MaxOpenPerUser = 16;
    static constexpr std::chrono::hours Retention{ 24 };

    explicit PanelLinks(PanelStore& store);

    PanelLinks(PanelLinks const&) = delete;
    PanelLinks& operator=(PanelLinks const&) = delete;

    void SetClock(EpochClock clock);
    int64 Now() const;

    static std::chrono::milliseconds LifetimeOf(PanelLinkKind kind) noexcept;
    static std::string_view NameOf(PanelLinkKind kind) noexcept;
    static bool ParseKind(std::string_view text, PanelLinkKind& kind) noexcept;
    static std::string HashOf(std::string_view token);

    PanelLinkMinted Mint(PanelLinkKind kind) const;
    bool Keep(PanelLinkMinted const& minted, PanelLinkKind kind, std::optional<int64> userId, std::string_view issuer, std::string_view address, std::string& error);
    PanelLinkState Peek(std::vector<PanelLinkKind> const& kinds, std::string_view token, PanelLink& link, std::string& error);
    PanelLinkState Spend(std::vector<PanelLinkKind> const& kinds, std::string_view token, std::string_view address, PanelLink& link, std::string& error);
    bool SpendEvery(PanelLinkKind kind, std::string& error);
    bool AnyOpen(PanelLinkKind kind, std::string& error);
    std::vector<PanelLink> List(std::string& error);

private:
    static std::string KindList(std::size_t count);
    static PanelLink Read(PanelStore::Statement const& row);

    PanelStore& _store;
    mutable std::mutex _clockMutex;
    EpochClock _clock;
};

#endif
