/*
 * Project Ambrose by Imjustchico
 * Browser sessions for the page the admin API serves: signing in trades the admin token once for a random cookie secret the listener keeps only as its SHA-256, each session carries its own CSRF token and idle and absolute lifetimes, the count is capped with the least recently used session ending first, and one call ends them all when the token rotates, so the token itself never waits in a browser.
 */

#ifndef AMBROSE_ADMINSESSIONS_H
#define AMBROSE_ADMINSESSIONS_H

#include "SHA256.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

struct AdminSession
{
    std::string Secret;
    std::string Csrf;
};

struct SessionHolder
{
    std::string Csrf;
    std::string Principal;
};

class SessionSource
{
public:
    virtual ~SessionSource() = default;

    virtual std::optional<SessionHolder> Hold(std::string_view secret) = 0;
};

class AdminSessions : public SessionSource
{
public:
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;

    static constexpr std::size_t MaxSessions = 64;
    static constexpr std::size_t SecretBytes = 32;
    static constexpr std::size_t MaxSecretLength = 128;

    explicit AdminSessions(TimeSource timeSource = [] { return Clock::now(); });

    AdminSessions(AdminSessions const&) = delete;
    AdminSessions& operator=(AdminSessions const&) = delete;

    void SetLifetimes(std::chrono::seconds idle, std::chrono::seconds absolute);
    std::chrono::seconds GetIdleLifetime() const;
    std::chrono::seconds GetAbsoluteLifetime() const;

    AdminSession Open();
    std::optional<std::string> Find(std::string_view secret);
    std::optional<SessionHolder> Hold(std::string_view secret) override;
    bool Close(std::string_view secret);
    void CloseAll();
    std::size_t Count() const;

private:
    struct Entry
    {
        std::string Csrf;
        Clock::time_point Created;
        Clock::time_point LastSeen;
    };

    bool Expired(Entry const& entry, Clock::time_point now) const;
    void DropExpired(Clock::time_point now);

    mutable std::mutex _mutex;
    TimeSource _timeSource;
    std::chrono::seconds _idle{ std::chrono::hours(12) };
    std::chrono::seconds _absolute{ std::chrono::hours(24 * 7) };
    std::map<SHA256::Digest, Entry> _sessions;
};

#endif
