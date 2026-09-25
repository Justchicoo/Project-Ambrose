/*
 * Project Ambrose by Imjustchico
 * State the login server's sessions share: the live login settings, read without locking, the clock idle checks and lockouts read, which tests can freeze and move forward, the failed-login throttle, a log budget for authentication failures, which session holds each account, applying the duplicate login policy when a second session claims one, and the source of character ids, resumed from the highest one ever stored so a new wizard can never be given an id a deleted one still holds.
 */

#ifndef AMBROSE_LOGINMGR_H
#define AMBROSE_LOGINMGR_H

#include "AuthThrottle.h"
#include "GuidGenerator.h"
#include "LoginSettings.h"
#include "TokenBucket.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

class ConfigMgr;
class LoginSession;

struct AccountClaim
{
    bool Claimed = false;
    std::shared_ptr<LoginSession> Previous;
};

class LoginMgr
{
public:
    static LoginMgr& Instance();

    void LoadSettings(ConfigMgr const& config);
    void SetSettings(LoginSettings settings);
    std::shared_ptr<LoginSettings const> GetSettings() const;

    static constexpr uint32 AuthLogBurst = 256;
    static constexpr double AuthLogsPerSecond = 64.0;

    AuthThrottle::Clock::time_point Now() const noexcept;
    void FreezeClock() noexcept;
    void AdvanceClock(std::chrono::seconds offset) noexcept;

    AuthThrottle& GetThrottle() noexcept { return _throttle; }
    bool AllowAuthLog();

    AccountClaim ClaimAccount(uint64 accountId, std::shared_ptr<LoginSession> const& session, DuplicateLoginPolicy policy);
    void ReleaseAccount(uint64 accountId, LoginSession const* session);
    std::shared_ptr<LoginSession> FindAccountSession(uint64 accountId) const;
    std::size_t GetAccountSessionCount() const;

    void ResumeCharacterGuids(uint64 highestUsed) noexcept;
    std::optional<uint64> NextCharacterGuid() noexcept;
    std::optional<uint64> PeekCharacterGuid() const noexcept;

    void Reset();

private:
    LoginMgr();

    static bool IsLive(std::shared_ptr<LoginSession> const& session) noexcept;

    std::atomic<std::shared_ptr<LoginSettings const>> _settings;
    std::atomic<int64> _clockOffset{ 0 };
    std::atomic<AuthThrottle::Clock::rep> _frozenClock{ 0 };
    AuthThrottle _throttle;
    GuidGenerator _characterGuids;
    std::mutex _logMutex;
    TokenBucket _authLogs;
    mutable std::mutex _accountsMutex;
    std::unordered_map<uint64, std::weak_ptr<LoginSession>> _accounts;
};

#define sLoginMgr LoginMgr::Instance()

#endif
