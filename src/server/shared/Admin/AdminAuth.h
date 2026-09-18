/*
 * Project Ambrose by Imjustchico
 * Bearer token authentication for the admin API: the live token, a constant-time comparison, and a token bucket per caller that limits failed attempts, where every loopback address and every IPv6 /64 counts as one caller so rotating a source address buys no extra budget.
 */

#ifndef AMBROSE_ADMINAUTH_H
#define AMBROSE_ADMINAUTH_H

#include "TokenBucket.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

enum class AdminAuthResult
{
    Ok,
    Unauthorized,
    RateLimited
};

class AdminAuth
{
public:
    using Clock = TokenBucket::Clock;
    using TimeSource = TokenBucket::TimeSource;

    static constexpr std::size_t MaxTrackedAddresses = 4096;
    static constexpr std::chrono::seconds IdleAddressLifetime{ 300 };

    AdminAuth(uint32 failureBurst, double failuresPerSecond, TimeSource timeSource = [] { return Clock::now(); });

    AdminAuth(AdminAuth const&) = delete;
    AdminAuth& operator=(AdminAuth const&) = delete;

    void SetLimits(uint32 failureBurst, double failuresPerSecond);
    void SetToken(std::string token);
    bool HasToken() const;

    AdminAuthResult Check(std::string const& address, std::string_view authorization);
    std::size_t GetTrackedAddresses() const;
    void Forget(std::string const& address);

    static std::optional<std::string_view> BearerToken(std::string_view authorization);
    static std::string BucketKey(std::string const& address);

private:
    struct Attempts
    {
        Attempts(uint32 capacity, double perSecond, TimeSource const& timeSource, Clock::time_point seen);

        TokenBucket Bucket;
        Clock::time_point LastSeen;
    };

    void Prune(Clock::time_point now);

    mutable std::mutex _mutex;
    TimeSource _timeSource;
    std::string _token;
    uint32 _failureBurst;
    double _failuresPerSecond;
    std::map<std::string, Attempts> _attempts;
};

#endif
