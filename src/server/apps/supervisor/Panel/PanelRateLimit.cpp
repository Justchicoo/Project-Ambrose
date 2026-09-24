/*
 * Project Ambrose by Imjustchico
 * Holds one token bucket per signed-in user and one per address, refilled by the time since it was last touched and capped at the burst; a cost of zero passes untouched, a cost larger than the whole burst is refused outright, and every refusal says how many whole seconds until enough has refilled and whether this is the first refusal for that caller in the last minute, which is what the one audit row per caller and minute is written from.
 */

#include "PanelRateLimit.h"

#include <algorithm>
#include <cmath>
#include <utility>

PanelRateLimit::PanelRateLimit(TimeSource timeSource) : _timeSource(std::move(timeSource))
{
}

void PanelRateLimit::SetLimits(uint32 burst, double perSecond)
{
    std::lock_guard lock(_mutex);
    _burst = std::max<uint32>(burst, 1);
    _perSecond = std::max(perSecond, 0.0);
}

uint32 PanelRateLimit::GetBurst() const
{
    std::lock_guard lock(_mutex);
    return _burst;
}

double PanelRateLimit::GetPerSecond() const
{
    std::lock_guard lock(_mutex);
    return _perSecond;
}

PanelRateLimit::Bucket& PanelRateLimit::Find(std::string const& key, Clock::time_point now)
{
    auto found = _buckets.find(key);
    if (found == _buckets.end())
    {
        if (_buckets.size() >= MaxTracked)
            DropOldest(now);
        found = _buckets.emplace(key, Bucket{ static_cast<double>(_burst), now, {}, false }).first;
    }
    return found->second;
}

void PanelRateLimit::DropOldest(Clock::time_point now)
{
    auto oldest = _buckets.end();
    for (auto it = _buckets.begin(); it != _buckets.end(); ++it)
    {
        if (it->second.Tokens < static_cast<double>(_burst) && now - it->second.Filled < std::chrono::hours(1))
            continue;
        if (oldest == _buckets.end() || it->second.Filled < oldest->second.Filled)
            oldest = it;
    }
    if (oldest == _buckets.end())
        oldest = _buckets.begin();
    if (oldest != _buckets.end())
        _buckets.erase(oldest);
}

bool PanelRateLimit::Note(Bucket& bucket, Clock::time_point now)
{
    if (bucket.Recorded && now - bucket.LastRecorded < std::chrono::minutes(1))
        return false;
    bucket.Recorded = true;
    bucket.LastRecorded = now;
    return true;
}

bool PanelRateLimit::TakeFrom(Bucket& bucket, uint32 cost, Clock::time_point now, uint32& retryAfter)
{
    double const seconds = std::chrono::duration<double>(now - bucket.Filled).count();
    bucket.Tokens = std::min(static_cast<double>(_burst), bucket.Tokens + std::max(seconds, 0.0) * _perSecond);
    bucket.Filled = now;
    if (bucket.Tokens >= static_cast<double>(cost))
    {
        bucket.Tokens -= static_cast<double>(cost);
        return true;
    }
    double const missing = static_cast<double>(cost) - bucket.Tokens;
    double const wait = _perSecond > 0.0 ? missing / _perSecond : 0.0;
    uint32 const whole = wait > 0.0 ? static_cast<uint32>(std::ceil(wait)) : 0;
    retryAfter = std::max(retryAfter, std::max<uint32>(whole, 1));
    return false;
}

PanelRateVerdict PanelRateLimit::Take(std::string_view user, std::string_view address, uint32 cost)
{
    PanelRateVerdict verdict;
    if (cost == 0)
        return verdict;

    std::lock_guard lock(_mutex);
    Clock::time_point const now = _timeSource();
    std::string const userKey = user.empty() ? std::string() : "user:" + std::string(user);
    std::string const addressKey = address.empty() ? std::string() : "address:" + std::string(address);
    Bucket* const userBucket = userKey.empty() ? nullptr : &Find(userKey, now);
    Bucket* const addressBucket = addressKey.empty() ? nullptr : &Find(addressKey, now);

    if (cost > _burst)
    {
        verdict.Allowed = false;
        verdict.RetryAfterSeconds = 1;
        verdict.Bucket = "cost";
        if (Bucket* const held = userBucket != nullptr ? userBucket : addressBucket)
            verdict.FirstThisMinute = Note(*held, now);
        else
            verdict.FirstThisMinute = true;
        return verdict;
    }

    uint32 retryAfter = 0;
    bool const userAllowed = userBucket == nullptr || TakeFrom(*userBucket, cost, now, retryAfter);
    bool const addressAllowed = addressBucket == nullptr || (userAllowed && TakeFrom(*addressBucket, cost, now, retryAfter));
    if (userAllowed && addressAllowed)
        return verdict;

    verdict.Allowed = false;
    verdict.RetryAfterSeconds = retryAfter;
    verdict.Bucket = userAllowed ? "address" : "user";
    verdict.FirstThisMinute = Note(userAllowed ? *addressBucket : *userBucket, now);
    return verdict;
}

void PanelRateLimit::Forget(std::string_view user, std::string_view address)
{
    std::lock_guard lock(_mutex);
    if (!user.empty())
        _buckets.erase("user:" + std::string(user));
    if (!address.empty())
        _buckets.erase("address:" + std::string(address));
}

void PanelRateLimit::Clear()
{
    std::lock_guard lock(_mutex);
    _buckets.clear();
}

std::size_t PanelRateLimit::Tracked() const
{
    std::lock_guard lock(_mutex);
    return _buckets.size();
}
