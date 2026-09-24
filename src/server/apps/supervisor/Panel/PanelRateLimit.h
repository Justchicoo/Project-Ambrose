/*
 * Project Ambrose by Imjustchico
 * What a route costs and what a caller has spent: every route says as it registers how much work answering it is, a route that says nothing is uncosted and is never held back, and the rest are counted against the signed-in user and against the address in two token buckets that refill over time, so one costly caller cannot crowd out a status read, and a request that would go over, or that costs more than the whole burst, is refused with how long to wait and recorded once per caller and minute.
 */

#ifndef AMBROSE_PANELRATELIMIT_H
#define AMBROSE_PANELRATELIMIT_H

#include "Types.h"

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

struct PanelRateVerdict
{
    bool Allowed = true;
    uint32 RetryAfterSeconds = 0;
    bool FirstThisMinute = false;
    std::string Bucket;
};

class PanelRateLimit
{
public:
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;

    static constexpr uint32 DefaultBurst = 120;
    static constexpr double DefaultPerSecond = 2.0;
    static constexpr std::size_t MaxTracked = 4096;

    explicit PanelRateLimit(TimeSource timeSource = [] { return Clock::now(); });

    PanelRateLimit(PanelRateLimit const&) = delete;
    PanelRateLimit& operator=(PanelRateLimit const&) = delete;

    void SetLimits(uint32 burst, double perSecond);
    uint32 GetBurst() const;
    double GetPerSecond() const;

    PanelRateVerdict Take(std::string_view user, std::string_view address, uint32 cost);
    void Forget(std::string_view user, std::string_view address);
    void Clear();
    std::size_t Tracked() const;

private:
    struct Bucket
    {
        double Tokens = 0.0;
        Clock::time_point Filled{};
        Clock::time_point LastRecorded{};
        bool Recorded = false;
    };

    bool Note(Bucket& bucket, Clock::time_point now);
    bool TakeFrom(Bucket& bucket, uint32 cost, Clock::time_point now, uint32& retryAfter);
    Bucket& Find(std::string const& key, Clock::time_point now);
    void DropOldest(Clock::time_point now);

    mutable std::mutex _mutex;
    TimeSource _timeSource;
    uint32 _burst = DefaultBurst;
    double _perSecond = DefaultPerSecond;
    std::map<std::string, Bucket, std::less<>> _buckets;
};

#endif
