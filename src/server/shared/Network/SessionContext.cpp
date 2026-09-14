/*
 * Project Ambrose by Imjustchico
 * Hands out session ids in rotating order so a released id is the last to come back, and guards settings and ids with one mutex.
 */

#include "SessionContext.h"

SessionContext::SessionContext(SessionSettings settings) : _settings(settings), _inUse(IdCount + 1, false)
{
}

SessionSettings SessionContext::GetSettings() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _settings;
}

void SessionContext::SetSettings(SessionSettings const& settings)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _settings = settings;
}

std::optional<uint16> SessionContext::AllocateId()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_active == IdCount)
        return std::nullopt;
    while (_inUse[_next])
        _next = _next == IdCount ? 1 : static_cast<uint16>(_next + 1);
    uint16 const id = _next;
    _inUse[id] = true;
    ++_active;
    _next = _next == IdCount ? 1 : static_cast<uint16>(_next + 1);
    return id;
}

void SessionContext::ReleaseId(uint16 id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (id == 0 || !_inUse[id])
        return;
    _inUse[id] = false;
    --_active;
}

bool SessionContext::IsIdInUse(uint16 id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return id != 0 && _inUse[id];
}

std::size_t SessionContext::GetActiveIdCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _active;
}
