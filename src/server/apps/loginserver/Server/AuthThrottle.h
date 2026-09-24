/*
 * Project Ambrose by Imjustchico
 * Limits login guesses per client address, an IPv6 client by its /64 network: attempts are reserved while in flight so parallel connections cannot outrun the limit, failures lock the address out for a while, and a bounded table prunes a few buckets per call and evicts old entries when full.
 */

#ifndef AMBROSE_AUTHTHROTTLE_H
#define AMBROSE_AUTHTHROTTLE_H

#include "Types.h"

#include <asio/ip/address.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <unordered_map>

enum class AuthAdmission : uint8
{
    Untracked,
    Reserved,
    LockedOut,
    TooManyInFlight
};

enum class AuthLockState : uint8
{
    NotLocked,
    LockedNow,
    AlreadyLocked
};

class AuthThrottle
{
public:
    using Clock = std::chrono::steady_clock;
    using Key = std::array<uint8, 16>;

    static constexpr std::size_t DefaultMaxTrackedAddresses = std::size_t{ 1 } << 20;
    static constexpr std::size_t PruneBucketsPerCall = 4;
    static constexpr std::size_t EvictionScanBuckets = 64;

    explicit AuthThrottle(std::size_t maxTrackedAddresses = DefaultMaxTrackedAddresses);

    AuthAdmission Begin(asio::ip::address const& address, uint32 maxAttempts, std::chrono::seconds lockout, Clock::time_point now);
    AuthLockState Finish(asio::ip::address const& address, AuthAdmission admission, bool failed, uint32 maxAttempts, std::chrono::seconds lockout, Clock::time_point now);
    std::optional<std::chrono::seconds> GetLockout(asio::ip::address const& address, Clock::time_point now) const;
    bool Reset(asio::ip::address const& address);
    void Clear();
    std::size_t GetTrackedCount() const;
    std::size_t GetInFlightCount(asio::ip::address const& address) const;

    static Key GetKey(asio::ip::address const& address) noexcept;

private:
    struct Entry
    {
        uint32 Failures = 0;
        uint32 InFlight = 0;
        Clock::time_point LastFailure;
        Clock::time_point LockedUntil;
    };

    struct KeyHash
    {
        std::size_t operator()(Key const& key) const noexcept;
    };

    using Table = std::unordered_map<Key, Entry, KeyHash>;

    static bool IsIdle(Entry const& entry, std::chrono::seconds lockout, Clock::time_point now) noexcept;
    Entry* FindOrCreate(Key const& key, std::chrono::seconds lockout, Clock::time_point now);
    void PruneSome(std::chrono::seconds lockout, Clock::time_point now);
    bool EvictOne(std::chrono::seconds lockout, Clock::time_point now);

    std::size_t _maxTracked;
    mutable std::mutex _mutex;
    Table _entries;
    std::size_t _cursor = 0;
};

#endif
