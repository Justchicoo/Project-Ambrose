/*
 * Project Ambrose by Imjustchico
 * Keys addresses as 16 bytes with IPv4 and IPv4-mapped addresses kept whole and other IPv6 addresses cut to their /64, reserves in-flight attempts against the limit, forgets failures older than the lockout, locks at the limit, prunes a few buckets per call and evicts the least useful entry when the table is full.
 */

#include "AuthThrottle.h"

#include <algorithm>
#include <cstring>

AuthThrottle::AuthThrottle(std::size_t maxTrackedAddresses) : _maxTracked(std::max<std::size_t>(maxTrackedAddresses, 1))
{
}

AuthThrottle::Key AuthThrottle::GetKey(asio::ip::address const& address) noexcept
{
    if (address.is_v4())
        return asio::ip::make_address_v6(asio::ip::v4_mapped, address.to_v4()).to_bytes();
    if (address.to_v6().is_v4_mapped())
        return address.to_v6().to_bytes();
    Key key = address.to_v6().to_bytes();
    std::memset(key.data() + 8, 0, 8);
    return key;
}

std::size_t AuthThrottle::KeyHash::operator()(Key const& key) const noexcept
{
    uint64 high = 0;
    uint64 low = 0;
    std::memcpy(&high, key.data(), 8);
    std::memcpy(&low, key.data() + 8, 8);
    uint64 const mixed = (high ^ (low + 0x9E3779B97F4A7C15ull + (high << 6) + (high >> 2))) * 0xFF51AFD7ED558CCDull;
    return static_cast<std::size_t>(mixed ^ (mixed >> 33));
}

bool AuthThrottle::IsIdle(Entry const& entry, std::chrono::seconds lockout, Clock::time_point now) noexcept
{
    return entry.InFlight == 0 && entry.LockedUntil <= now && (entry.Failures == 0 || now - entry.LastFailure >= lockout);
}

AuthAdmission AuthThrottle::Begin(asio::ip::address const& address, uint32 maxAttempts, std::chrono::seconds lockout, Clock::time_point now)
{
    if (maxAttempts == 0)
        return AuthAdmission::Untracked;
    Key const key = GetKey(address);
    std::lock_guard const lock(_mutex);
    PruneSome(lockout, now);
    Entry* const entry = FindOrCreate(key, lockout, now);
    if (!entry)
        return AuthAdmission::Untracked;
    if (entry->LockedUntil > now)
        return AuthAdmission::LockedOut;
    if (entry->Failures != 0 && now - entry->LastFailure >= lockout)
        entry->Failures = 0;
    if (entry->Failures + entry->InFlight >= maxAttempts)
        return AuthAdmission::TooManyInFlight;
    ++entry->InFlight;
    return AuthAdmission::Reserved;
}

AuthLockState AuthThrottle::Finish(asio::ip::address const& address, AuthAdmission admission, bool failed, uint32 maxAttempts, std::chrono::seconds lockout, Clock::time_point now)
{
    Key const key = GetKey(address);
    std::lock_guard const lock(_mutex);
    auto found = _entries.find(key);
    if (found != _entries.end() && admission == AuthAdmission::Reserved && found->second.InFlight > 0)
        --found->second.InFlight;
    if (!failed || maxAttempts == 0)
    {
        if (found != _entries.end() && found->second.Failures == 0 && IsIdle(found->second, lockout, now))
            _entries.erase(found);
        return AuthLockState::NotLocked;
    }

    Entry* const entry = found != _entries.end() ? &found->second : FindOrCreate(key, lockout, now);
    if (!entry)
        return AuthLockState::NotLocked;
    if (entry->LockedUntil > now)
        return AuthLockState::AlreadyLocked;
    if (entry->Failures != 0 && now - entry->LastFailure >= lockout)
        entry->Failures = 0;
    ++entry->Failures;
    entry->LastFailure = now;
    if (entry->Failures < maxAttempts)
        return AuthLockState::NotLocked;
    entry->Failures = 0;
    entry->LockedUntil = now + lockout;
    return AuthLockState::LockedNow;
}

std::optional<std::chrono::seconds> AuthThrottle::GetLockout(asio::ip::address const& address, Clock::time_point now) const
{
    std::lock_guard const lock(_mutex);
    auto const entry = _entries.find(GetKey(address));
    if (entry == _entries.end() || entry->second.LockedUntil <= now)
        return std::nullopt;
    return std::chrono::ceil<std::chrono::seconds>(entry->second.LockedUntil - now);
}

bool AuthThrottle::Reset(asio::ip::address const& address)
{
    std::lock_guard const lock(_mutex);
    auto const entry = _entries.find(GetKey(address));
    if (entry == _entries.end())
        return false;
    if (entry->second.InFlight == 0)
    {
        _entries.erase(entry);
        return true;
    }
    entry->second.Failures = 0;
    entry->second.LockedUntil = Clock::time_point{};
    return true;
}

void AuthThrottle::Clear()
{
    std::lock_guard const lock(_mutex);
    _entries.clear();
    _cursor = 0;
}

std::size_t AuthThrottle::GetTrackedCount() const
{
    std::lock_guard const lock(_mutex);
    return _entries.size();
}

std::size_t AuthThrottle::GetInFlightCount(asio::ip::address const& address) const
{
    std::lock_guard const lock(_mutex);
    auto const entry = _entries.find(GetKey(address));
    return entry == _entries.end() ? 0 : entry->second.InFlight;
}

AuthThrottle::Entry* AuthThrottle::FindOrCreate(Key const& key, std::chrono::seconds lockout, Clock::time_point now)
{
    auto const found = _entries.find(key);
    if (found != _entries.end())
        return &found->second;
    if (_entries.size() >= _maxTracked && !EvictOne(lockout, now))
        return nullptr;
    return &_entries.emplace(key, Entry{}).first->second;
}

void AuthThrottle::PruneSome(std::chrono::seconds lockout, Clock::time_point now)
{
    std::size_t const buckets = _entries.bucket_count();
    if (_entries.empty() || buckets == 0)
        return;
    std::array<Key, 16> idle;
    std::size_t count = 0;
    for (std::size_t scanned = 0; scanned < PruneBucketsPerCall && count < idle.size(); ++scanned)
    {
        std::size_t const bucket = _cursor++ % buckets;
        for (auto entry = _entries.cbegin(bucket); entry != _entries.cend(bucket) && count < idle.size(); ++entry)
            if (IsIdle(entry->second, lockout, now))
                idle[count++] = entry->first;
    }
    for (std::size_t i = 0; i < count; ++i)
        _entries.erase(idle[i]);
}

bool AuthThrottle::EvictOne(std::chrono::seconds lockout, Clock::time_point now)
{
    std::size_t const buckets = _entries.bucket_count();
    if (_entries.empty() || buckets == 0)
        return false;
    std::optional<Key> victim;
    bool victimLocked = true;
    Clock::time_point victimFailure;
    for (std::size_t scanned = 0; scanned < std::min(EvictionScanBuckets, buckets); ++scanned)
    {
        std::size_t const bucket = _cursor++ % buckets;
        for (auto entry = _entries.cbegin(bucket); entry != _entries.cend(bucket); ++entry)
        {
            Entry const& state = entry->second;
            if (state.InFlight != 0)
                continue;
            if (IsIdle(state, lockout, now))
            {
                _entries.erase(entry->first);
                return true;
            }
            bool const locked = state.LockedUntil > now;
            if (!victim || (victimLocked && !locked) || (victimLocked == locked && state.LastFailure < victimFailure))
            {
                victim = entry->first;
                victimLocked = locked;
                victimFailure = state.LastFailure;
            }
        }
    }
    if (!victim)
        return false;
    _entries.erase(*victim);
    return true;
}
