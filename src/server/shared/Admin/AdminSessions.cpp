/*
 * Project Ambrose by Imjustchico
 * Opens a session with a fresh 256-bit secret and CSRF token, finds one by the SHA-256 of the secret a cookie presents, ends it once it has been idle or alive too long, and ends the least recently used one when the cap is reached.
 */

#include "AdminSessions.h"
#include "Base64.h"
#include "CryptoRandom.h"

#include <algorithm>
#include <array>
#include <utility>

AdminSessions::AdminSessions(TimeSource timeSource) : _timeSource(std::move(timeSource))
{
}

void AdminSessions::SetLifetimes(std::chrono::seconds idle, std::chrono::seconds absolute)
{
    std::lock_guard const lock(_mutex);
    _idle = idle;
    _absolute = absolute;
}

std::chrono::seconds AdminSessions::GetIdleLifetime() const
{
    std::lock_guard const lock(_mutex);
    return _idle;
}

std::chrono::seconds AdminSessions::GetAbsoluteLifetime() const
{
    std::lock_guard const lock(_mutex);
    return _absolute;
}

AdminSession AdminSessions::Open()
{
    std::array<uint8, SecretBytes> const secret = Ambrose::Crypto::GetRandomArray<SecretBytes>();
    std::array<uint8, SecretBytes> const csrf = Ambrose::Crypto::GetRandomArray<SecretBytes>();
    AdminSession opened{ Base64::Encode(secret, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted),
        Base64::Encode(csrf, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted) };

    Clock::time_point const now = _timeSource();
    std::lock_guard const lock(_mutex);
    DropExpired(now);
    if (_sessions.size() >= MaxSessions)
    {
        auto const oldest = std::min_element(_sessions.begin(), _sessions.end(), [](auto const& left, auto const& right) { return left.second.LastSeen < right.second.LastSeen; });
        _sessions.erase(oldest);
    }
    _sessions.insert_or_assign(SHA256::GetDigestOf(opened.Secret), Entry{ opened.Csrf, now, now });
    return opened;
}

std::optional<std::string> AdminSessions::Find(std::string_view secret)
{
    if (secret.empty() || secret.size() > MaxSecretLength)
        return std::nullopt;
    SHA256::Digest const key = SHA256::GetDigestOf(secret);
    Clock::time_point const now = _timeSource();
    std::lock_guard const lock(_mutex);
    auto const found = _sessions.find(key);
    if (found == _sessions.end())
        return std::nullopt;
    if (Expired(found->second, now))
    {
        _sessions.erase(found);
        return std::nullopt;
    }
    found->second.LastSeen = now;
    return found->second.Csrf;
}

bool AdminSessions::Close(std::string_view secret)
{
    if (secret.empty() || secret.size() > MaxSecretLength)
        return false;
    SHA256::Digest const key = SHA256::GetDigestOf(secret);
    std::lock_guard const lock(_mutex);
    return _sessions.erase(key) != 0;
}

void AdminSessions::CloseAll()
{
    std::lock_guard const lock(_mutex);
    _sessions.clear();
}

std::size_t AdminSessions::Count() const
{
    std::lock_guard const lock(_mutex);
    return _sessions.size();
}

bool AdminSessions::Expired(Entry const& entry, Clock::time_point now) const
{
    return now - entry.LastSeen >= _idle || now - entry.Created >= _absolute;
}

void AdminSessions::DropExpired(Clock::time_point now)
{
    std::erase_if(_sessions, [this, now](auto const& entry) { return Expired(entry.second, now); });
}

std::optional<SessionHolder> AdminSessions::Hold(std::string_view secret)
{
    std::optional<std::string> csrf = Find(secret);
    if (!csrf)
        return std::nullopt;
    return SessionHolder{ std::move(*csrf), std::string() };
}
