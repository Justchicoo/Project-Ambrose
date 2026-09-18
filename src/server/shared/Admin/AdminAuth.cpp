/*
 * Project Ambrose by Imjustchico
 * Reads the Bearer scheme and compares the token in constant time before any budget is consulted, so the right token always answers, refuses outright a request that names no caller address rather than billing every such request to one bucket, spends one of the caller's failure tokens on every wrong answer, refuses a caller whose bucket is empty, folds every loopback address and every IPv6 /64 into one bucket, and forgets the least recently seen caller, an emptied bucket last, so the map cannot grow without bound.
 */

#include "AdminAuth.h"
#include "ConstantTime.h"
#include "IpAddress.h"
#include "StringUtil.h"

#include <algorithm>
#include <tuple>
#include <utility>

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
    _spare.clear();
    _drained.clear();
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

bool AdminAuth::HoldsTheToken(std::string_view authorization) const
{
    if (_token.empty())
        return false;
    std::optional<std::string_view> const presented = BearerToken(authorization);
    return presented && Ambrose::Crypto::ConstantTimeEquals(*presented, _token);
}

AdminAuthResult AdminAuth::Check(std::string const& address, std::string_view authorization)
{
    std::lock_guard const lock(_mutex);
    if (address.empty())
        return AdminAuthResult::Unauthorized;
    if (HoldsTheToken(authorization))
        return AdminAuthResult::Ok;

    Clock::time_point const now = _timeSource();
    std::string const key = BucketKey(address);
    Tracked::iterator caller = _attempts.find(key);
    if (caller == _attempts.end())
    {
        MakeRoom(now);
        caller = _attempts.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(_failureBurst, _failuresPerSecond, _timeSource, now)).first;
        caller->second.Place = _spare.insert(_spare.end(), &caller->first);
    }

    bool const spent = caller->second.Bucket.TryConsume();
    Touch(caller, now);
    return spent ? AdminAuthResult::Unauthorized : AdminAuthResult::RateLimited;
}

std::size_t AdminAuth::GetTrackedAddresses() const
{
    std::lock_guard const lock(_mutex);
    return _attempts.size();
}

void AdminAuth::Forget(std::string const& address)
{
    std::lock_guard const lock(_mutex);
    Tracked::iterator const caller = _attempts.find(BucketKey(address));
    if (caller != _attempts.end())
        Drop(caller);
}

void AdminAuth::Touch(Tracked::iterator caller, Clock::time_point now)
{
    caller->second.LastSeen = now;
    bool const drained = caller->second.Bucket.GetAvailableTokens() < 1.0;
    Order& from = caller->second.Drained ? _drained : _spare;
    Order& to = drained ? _drained : _spare;
    to.splice(to.end(), from, caller->second.Place);
    caller->second.Drained = drained;
}

void AdminAuth::Drop(Tracked::iterator caller)
{
    (caller->second.Drained ? _drained : _spare).erase(caller->second.Place);
    _attempts.erase(caller);
}

void AdminAuth::ForgetIdle(Order& order, Clock::time_point now)
{
    while (!order.empty())
    {
        Tracked::iterator const oldest = _attempts.find(*order.front());
        if (now - oldest->second.LastSeen < IdleAddressLifetime || oldest->second.Bucket.GetAvailableTokens() < static_cast<double>(_failureBurst))
            return;
        Drop(oldest);
    }
}

void AdminAuth::MakeRoom(Clock::time_point now)
{
    if (_attempts.size() < MaxTrackedAddresses)
        return;
    ForgetIdle(_spare, now);
    ForgetIdle(_drained, now);
    if (_attempts.size() < MaxTrackedAddresses)
        return;
    Order const& order = _spare.empty() ? _drained : _spare;
    Drop(_attempts.find(*order.front()));
}
