/*
 * Project Ambrose by Imjustchico
 * What a failed sign-in costs: failures are counted against the account, and separately against the account and the address together, over a window that ends on its own; only failures count, so no amount of signing in successfully buys an attacker more guesses, and a success clears only the counts for that account, so guessing at one account from an address is not forgiven by signing in to another from the same address.
 */

#ifndef AMBROSE_PANELSIGNIN_H
#define AMBROSE_PANELSIGNIN_H

#include "Types.h"

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

struct PanelSignInVerdict
{
    bool Allowed = true;
    uint32 RetryAfterSeconds = 0;
    bool FirstThisWindow = false;
    std::string Counted;
};

class PanelSignInThrottle
{
public:
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;

    static constexpr uint32 DefaultFailures = 20;
    static constexpr std::chrono::seconds DefaultWindow{ 60 };
    static constexpr std::size_t MaxTracked = 4096;

    explicit PanelSignInThrottle(TimeSource timeSource = [] { return Clock::now(); });

    PanelSignInThrottle(PanelSignInThrottle const&) = delete;
    PanelSignInThrottle& operator=(PanelSignInThrottle const&) = delete;

    void SetLimits(uint32 failures, std::chrono::seconds window);
    uint32 GetFailures() const;
    std::chrono::seconds GetWindow() const;

    PanelSignInVerdict Check(std::string_view username, std::string_view address);
    void Failed(std::string_view username, std::string_view address);
    void Succeeded(std::string_view username, std::string_view address);
    void Clear();
    std::size_t Tracked() const;

private:
    struct Count
    {
        uint32 Failures = 0;
        Clock::time_point Started{};
        bool Reported = false;
    };

    static std::string UserKey(std::string_view username);
    static std::string PairKey(std::string_view username, std::string_view address);

    bool Over(Count& count, Clock::time_point now, uint32& retryAfter, bool& first);
    void Add(std::string const& key, Clock::time_point now);
    void DropOldest(Clock::time_point now);

    mutable std::mutex _mutex;
    TimeSource _timeSource;
    uint32 _failures = DefaultFailures;
    std::chrono::seconds _window{ DefaultWindow };
    std::map<std::string, Count, std::less<>> _counts;
};

#endif
