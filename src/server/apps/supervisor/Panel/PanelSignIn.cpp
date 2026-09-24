/*
 * Project Ambrose by Imjustchico
 * Keeps one count per account and one per account and address together, each over a window that starts at its first failure and ends on its own, so a run of guesses is held back for what is left of the window and then forgiven; the name is folded before it is counted, so guessing at a name in another case is the same guessing, and a success clears both of that account's counts and nobody else's.
 */

#include "PanelSignIn.h"
#include "StringUtil.h"

#include <algorithm>
#include <cmath>
#include <utility>

PanelSignInThrottle::PanelSignInThrottle(TimeSource timeSource) : _timeSource(std::move(timeSource))
{
}

void PanelSignInThrottle::SetLimits(uint32 failures, std::chrono::seconds window)
{
    std::lock_guard const lock(_mutex);
    _failures = std::max<uint32>(failures, 1);
    _window = std::max(window, std::chrono::seconds(1));
}

uint32 PanelSignInThrottle::GetFailures() const
{
    std::lock_guard const lock(_mutex);
    return _failures;
}

std::chrono::seconds PanelSignInThrottle::GetWindow() const
{
    std::lock_guard const lock(_mutex);
    return _window;
}

std::string PanelSignInThrottle::UserKey(std::string_view username)
{
    return "user:" + Ambrose::ToLower(username);
}

std::string PanelSignInThrottle::PairKey(std::string_view username, std::string_view address)
{
    return "pair:" + Ambrose::ToLower(username) + "|" + std::string(address);
}

bool PanelSignInThrottle::Over(Count& count, Clock::time_point now, uint32& retryAfter, bool& first)
{
    if (now - count.Started >= _window)
    {
        count.Failures = 0;
        count.Reported = false;
        return false;
    }
    if (count.Failures < _failures)
        return false;
    double const left = std::chrono::duration<double>(_window - (now - count.Started)).count();
    retryAfter = std::max(retryAfter, static_cast<uint32>(std::max(std::ceil(left), 1.0)));
    if (!count.Reported)
    {
        count.Reported = true;
        first = true;
    }
    return true;
}

PanelSignInVerdict PanelSignInThrottle::Check(std::string_view username, std::string_view address)
{
    PanelSignInVerdict verdict;
    if (username.empty())
        return verdict;

    std::lock_guard const lock(_mutex);
    Clock::time_point const now = _timeSource();
    auto const look = [&](std::string const& key, char const* named)
    {
        auto const found = _counts.find(key);
        if (found == _counts.end())
            return;
        bool first = false;
        if (Over(found->second, now, verdict.RetryAfterSeconds, first))
        {
            verdict.Allowed = false;
            verdict.FirstThisWindow = verdict.FirstThisWindow || first;
            if (verdict.Counted.empty())
                verdict.Counted = named;
        }
    };
    look(UserKey(username), "account");
    if (!address.empty())
        look(PairKey(username, address), "account and address");
    return verdict;
}

void PanelSignInThrottle::Add(std::string const& key, Clock::time_point now)
{
    auto found = _counts.find(key);
    if (found == _counts.end())
    {
        if (_counts.size() >= MaxTracked)
            DropOldest(now);
        found = _counts.emplace(key, Count{ 0, now, false }).first;
    }
    if (now - found->second.Started >= _window)
    {
        found->second.Failures = 0;
        found->second.Started = now;
        found->second.Reported = false;
    }
    ++found->second.Failures;
}

void PanelSignInThrottle::DropOldest(Clock::time_point now)
{
    auto oldest = _counts.end();
    for (auto it = _counts.begin(); it != _counts.end(); ++it)
    {
        if (now - it->second.Started < _window)
            continue;
        if (oldest == _counts.end() || it->second.Started < oldest->second.Started)
            oldest = it;
    }
    if (oldest == _counts.end())
        oldest = _counts.begin();
    if (oldest != _counts.end())
        _counts.erase(oldest);
}

void PanelSignInThrottle::Failed(std::string_view username, std::string_view address)
{
    if (username.empty())
        return;
    std::lock_guard const lock(_mutex);
    Clock::time_point const now = _timeSource();
    Add(UserKey(username), now);
    if (!address.empty())
        Add(PairKey(username, address), now);
}

void PanelSignInThrottle::Succeeded(std::string_view username, std::string_view address)
{
    if (username.empty())
        return;
    std::lock_guard const lock(_mutex);
    _counts.erase(UserKey(username));
    if (!address.empty())
        _counts.erase(PairKey(username, address));
}

void PanelSignInThrottle::Clear()
{
    std::lock_guard const lock(_mutex);
    _counts.clear();
}

std::size_t PanelSignInThrottle::Tracked() const
{
    std::lock_guard const lock(_mutex);
    return _counts.size();
}
