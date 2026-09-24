/*
 * Project Ambrose by Imjustchico
 * Bearer token authentication for the admin API: the live token, a constant-time comparison the right token passes whatever its caller's budget holds, a token bucket per caller that limits failed attempts, where every loopback address and every IPv6 /64 counts as one caller so rotating a source address buys no extra budget, a refusal for a request that carries no caller address at all, and a least-recently-seen order that keeps forgetting a caller off the request path.
 */

#ifndef AMBROSE_ADMINAUTH_H
#define AMBROSE_ADMINAUTH_H

#include "TokenBucket.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

enum class AdminAuthResult
{
    Ok,
    Unauthorized,
    RateLimited,
    Forbidden
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
    using Order = std::list<std::string const*>;

    struct Attempts
    {
        Attempts(uint32 capacity, double perSecond, TimeSource const& timeSource, Clock::time_point seen);

        TokenBucket Bucket;
        Clock::time_point LastSeen;
        Order::iterator Place{};
        bool Drained = false;
    };

    using Tracked = std::map<std::string, Attempts>;

    bool HoldsTheToken(std::string_view authorization) const;
    void Touch(Tracked::iterator caller, Clock::time_point now);
    void Drop(Tracked::iterator caller);
    void ForgetIdle(Order& order, Clock::time_point now);
    void MakeRoom(Clock::time_point now);

    mutable std::mutex _mutex;
    TimeSource _timeSource;
    std::string _token;
    uint32 _failureBurst;
    double _failuresPerSecond;
    Tracked _attempts;
    Order _spare;
    Order _drained;
};

#endif
