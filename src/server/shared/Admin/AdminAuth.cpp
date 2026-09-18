/*
 * Project Ambrose by Imjustchico
 * Reads the Bearer scheme, compares the token in constant time, spends one of the caller's failure tokens on every wrong answer, refuses a caller whose bucket is empty, folds every loopback address and every IPv6 /64 into one bucket, and forgets idle callers, an emptied bucket last, so the map cannot grow without bound.
 */

#include "AdminAuth.h"
#include "ConstantTime.h"
#include "IpAddress.h"
#include "StringUtil.h"

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

AdminAuth::Attempts::Attempts(uint32 capacity, double perSecond, TimeSource const& timeSource, Clock::time_point seen)
    : Bucket(capacity, perSecond, timeSource), LastSeen(seen)
{
}

AdminAuth::AdminAuth(uint32 failureBurst, double failuresPerSecond, TimeSource timeSource)
    : _timeSource(std::move(timeSource)), _failureBurst(std::max<uint32>(failureBurst, 1)), _failuresPerSecond(std::max(failuresPerSecond, 0.0))
{
}

void AdminAuth::SetLimits(uint32 failureBurst, double failuresPerSecond)
{
    std::lock_guard const lock(_mutex);
    uint32 const burst = std::max<uint32>(failureBurst, 1);
    double const refill = std::max(failuresPerSecond, 0.0);
    if (burst == _failureBurst && refill == _failuresPerSecond)
        return;
    _failureBurst = burst;
    _failuresPerSecond = refill;
    _attempts.clear();
}

void AdminAuth::SetToken(std::string token)
{
    std::lock_guard const lock(_mutex);
    _token = std::move(token);
}

bool AdminAuth::HasToken() const
{
    std::lock_guard const lock(_mutex);
    return !_token.empty();
}

std::optional<std::string_view> AdminAuth::BearerToken(std::string_view authorization)
{
    std::string_view const header = Ambrose::Trim(authorization);
    constexpr std::string_view scheme = "Bearer";
    if (header.size() <= scheme.size() || !Ambrose::EqualsIgnoreCase(header.substr(0, scheme.size()), scheme))
        return std::nullopt;
    char const separator = header[scheme.size()];
    if (separator != ' ' && separator != '\t')
        return std::nullopt;
    std::string_view const token = Ambrose::Trim(header.substr(scheme.size() + 1));
    if (token.empty())
        return std::nullopt;
    return token;
}

std::string AdminAuth::BucketKey(std::string const& address)
{
    std::optional<asio::ip::address> const parsed = Ambrose::Asio::MakeAddress(address);
    if (!parsed)
        return address;
    asio::ip::address const caller = Ambrose::Asio::Unmap(*parsed);
    if (Ambrose::Asio::IsLoopback(caller))
        return "loopback";
    if (!caller.is_v6())
        return caller.to_string();
    asio::ip::address_v6::bytes_type bytes = caller.to_v6().to_bytes();
    for (std::size_t index = bytes.size() / 2; index < bytes.size(); ++index)
        bytes[index] = 0;
    return asio::ip::address_v6(bytes).to_string() + "/64";
}

AdminAuthResult AdminAuth::Check(std::string const& address, std::string_view authorization)
{
    std::lock_guard const lock(_mutex);
    Clock::time_point const now = _timeSource();
    std::string const key = BucketKey(address);
    auto entry = _attempts.find(key);
    if (entry == _attempts.end())
    {
        Prune(now);
        entry = _attempts.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(_failureBurst, _failuresPerSecond, _timeSource, now)).first;
    }
    entry->second.LastSeen = now;

    if (entry->second.Bucket.GetAvailableTokens() < 1.0)
        return AdminAuthResult::RateLimited;

    std::optional<std::string_view> const presented = BearerToken(authorization);
    if (!_token.empty() && presented && Ambrose::Crypto::ConstantTimeEquals(*presented, _token))
        return AdminAuthResult::Ok;

    entry->second.Bucket.TryConsume();
    return AdminAuthResult::Unauthorized;
}

std::size_t AdminAuth::GetTrackedAddresses() const
{
    std::lock_guard const lock(_mutex);
    return _attempts.size();
}

void AdminAuth::Forget(std::string const& address)
{
    std::lock_guard const lock(_mutex);
    _attempts.erase(BucketKey(address));
}

void AdminAuth::Prune(Clock::time_point now)
{
    if (_attempts.size() < MaxTrackedAddresses)
        return;
    for (auto entry = _attempts.begin(); entry != _attempts.end();)
    {
        bool const idle = now - entry->second.LastSeen >= IdleAddressLifetime;
        if (idle && entry->second.Bucket.GetAvailableTokens() >= static_cast<double>(_failureBurst))
            entry = _attempts.erase(entry);
        else
            ++entry;
    }
    if (_attempts.size() < MaxTrackedAddresses)
        return;
    std::vector<std::pair<Clock::time_point, std::string>> spare;
    std::vector<std::pair<Clock::time_point, std::string>> emptied;
    for (auto& entry : _attempts)
    {
        if (entry.second.Bucket.GetAvailableTokens() < 1.0)
            emptied.emplace_back(entry.second.LastSeen, entry.first);
        else
            spare.emplace_back(entry.second.LastSeen, entry.first);
    }
    std::sort(spare.begin(), spare.end());
    std::sort(emptied.begin(), emptied.end());
    std::size_t drop = _attempts.size() - MaxTrackedAddresses + 1;
    for (std::size_t index = 0; index < spare.size() && drop != 0; ++index, --drop)
        _attempts.erase(spare[index].second);
    for (std::size_t index = 0; index < emptied.size() && drop != 0; ++index, --drop)
        _attempts.erase(emptied[index].second);
}
