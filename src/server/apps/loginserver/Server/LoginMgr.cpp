/*
 * Project Ambrose by Imjustchico
 * Swaps login settings atomically and logs their problems, rations authentication failure log lines across every session, and tracks one live session per account, forgetting closed ones and handing back a replaced session so its new owner can kick it.
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
    auto replacement = std::make_shared<LoginSettings const>(std::move(settings));
    std::lock_guard const lock(_settingsMutex);
    _settings = std::move(replacement);
}

std::shared_ptr<LoginSettings const> LoginMgr::GetSettings() const
{
    std::lock_guard const lock(_settingsMutex);
    return _settings;
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
    _throttle.Clear();
    std::lock_guard const lock(_accountsMutex);
    _accounts.clear();
}
