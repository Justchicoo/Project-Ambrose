/*
 * Project Ambrose by Imjustchico
 * Swaps login settings atomically and logs their problems, serves a clock tests can freeze and move, rations authentication failure log lines across every session, and tracks one live session per account, forgetting closed ones and handing back a replaced session so its new owner can kick it.
 */

#include "LoginMgr.h"
#include "Log.h"
#include "LoginSession.h"

#include <string>
#include <vector>

LoginMgr& LoginMgr::Instance()
{
    static LoginMgr instance;
    return instance;
}

LoginMgr::LoginMgr() : _settings(std::make_shared<LoginSettings const>()), _authLogs(AuthLogBurst, AuthLogsPerSecond)
{
}

void LoginMgr::LoadSettings(ConfigMgr const& config)
{
    std::vector<std::string> problems;
    LoginSettings settings = LoginSettings::Load(config, &problems);
    for (std::string const& problem : problems)
        LOG_WARN("server.loginserver", "{}", problem);
    SetSettings(std::move(settings));
}

void LoginMgr::SetSettings(LoginSettings settings)
{
    _settings.store(std::make_shared<LoginSettings const>(std::move(settings)));
}

std::shared_ptr<LoginSettings const> LoginMgr::GetSettings() const
{
    return _settings.load();
}

AuthThrottle::Clock::time_point LoginMgr::Now() const noexcept
{
    AuthThrottle::Clock::rep const frozen = _frozenClock.load(std::memory_order_relaxed);
    if (frozen != 0)
        return AuthThrottle::Clock::time_point(AuthThrottle::Clock::duration(frozen));
    return AuthThrottle::Clock::now() + std::chrono::seconds(_clockOffset.load(std::memory_order_relaxed));
}

void LoginMgr::FreezeClock() noexcept
{
    _frozenClock.store(Now().time_since_epoch().count(), std::memory_order_relaxed);
}

void LoginMgr::AdvanceClock(std::chrono::seconds offset) noexcept
{
    AuthThrottle::Clock::rep frozen = _frozenClock.load(std::memory_order_relaxed);
    AuthThrottle::Clock::rep const step = std::chrono::duration_cast<AuthThrottle::Clock::duration>(offset).count();
    while (frozen != 0 && !_frozenClock.compare_exchange_weak(frozen, frozen + step, std::memory_order_relaxed))
    {
    }
    if (frozen == 0)
        _clockOffset.fetch_add(offset.count(), std::memory_order_relaxed);
}

bool LoginMgr::AllowAuthLog()
{
    std::lock_guard const lock(_logMutex);
    return _authLogs.TryConsume();
}

bool LoginMgr::IsLive(std::shared_ptr<LoginSession> const& session) noexcept
{
    return session && session->IsOpen() && !session->IsKicked();
}

AccountClaim LoginMgr::ClaimAccount(uint64 accountId, std::shared_ptr<LoginSession> const& session, DuplicateLoginPolicy policy)
{
    std::lock_guard const lock(_accountsMutex);
    std::weak_ptr<LoginSession>& slot = _accounts[accountId];
    std::shared_ptr<LoginSession> current = slot.lock();
    if (current == session)
        return { true, nullptr };
    if (!IsLive(current))
        current.reset();
    if (current && policy == DuplicateLoginPolicy::Reject)
        return { false, std::move(current) };
    slot = session;
    return { true, std::move(current) };
}

void LoginMgr::ReleaseAccount(uint64 accountId, LoginSession const* session)
{
    std::lock_guard const lock(_accountsMutex);
    auto const entry = _accounts.find(accountId);
    if (entry == _accounts.end())
        return;
    std::shared_ptr<LoginSession> const current = entry->second.lock();
    if (!current || current.get() == session)
        _accounts.erase(entry);
}

std::shared_ptr<LoginSession> LoginMgr::FindAccountSession(uint64 accountId) const
{
    std::lock_guard const lock(_accountsMutex);
    auto const entry = _accounts.find(accountId);
    if (entry == _accounts.end())
        return nullptr;
    std::shared_ptr<LoginSession> session = entry->second.lock();
    return IsLive(session) ? session : nullptr;
}

std::size_t LoginMgr::GetAccountSessionCount() const
{
    std::lock_guard const lock(_accountsMutex);
    std::size_t count = 0;
    for (auto const& entry : _accounts)
        if (IsLive(entry.second.lock()))
            ++count;
    return count;
}

void LoginMgr::Reset()
{
    SetSettings(LoginSettings{});
    _clockOffset.store(0, std::memory_order_relaxed);
    _frozenClock.store(0, std::memory_order_relaxed);
    _throttle.Clear();
    std::lock_guard const lock(_accountsMutex);
    _accounts.clear();
}
