/*
 * Project Ambrose by Imjustchico
 * State the login server's sessions share: the live login settings, the failed-login throttle, a log budget for authentication failures, and which session holds each account, applying the duplicate login policy when a second session claims one.
 */

#ifndef AMBROSE_LOGINMGR_H
#define AMBROSE_LOGINMGR_H

#include "AuthThrottle.h"
#include "LoginSettings.h"
#include "TokenBucket.h"

#include <memory>
#include <mutex>
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

    AuthThrottle& GetThrottle() noexcept { return _throttle; }
    bool AllowAuthLog();

    AccountClaim ClaimAccount(uint64 accountId, std::shared_ptr<LoginSession> const& session, DuplicateLoginPolicy policy);
    void ReleaseAccount(uint64 accountId, LoginSession const* session);
    std::shared_ptr<LoginSession> FindAccountSession(uint64 accountId) const;
    std::size_t GetAccountSessionCount() const;

    void Reset();

private:
    LoginMgr();

    static bool IsLive(std::shared_ptr<LoginSession> const& session) noexcept;

    mutable std::mutex _settingsMutex;
    std::shared_ptr<LoginSettings const> _settings;
    AuthThrottle _throttle;
    std::mutex _logMutex;
    TokenBucket _authLogs;
    mutable std::mutex _accountsMutex;
    std::unordered_map<uint64, std::weak_ptr<LoginSession>> _accounts;
};

#define sLoginMgr LoginMgr::Instance()

#endif
