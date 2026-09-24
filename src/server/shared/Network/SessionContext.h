/*
 * Project Ambrose by Imjustchico
 * State every session of one listener shares: the live session settings and the allocator for nonzero session ids that are reused only after their session closes.
 */

#ifndef AMBROSE_SESSIONCONTEXT_H
#define AMBROSE_SESSIONCONTEXT_H

#include "SessionSettings.h"
#include "Types.h"

#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>

class SessionContext
{
public:
    static constexpr std::size_t IdCount = 65535;

    explicit SessionContext(SessionSettings settings = {});

    SessionContext(SessionContext const&) = delete;
    SessionContext& operator=(SessionContext const&) = delete;

    SessionSettings GetSettings() const;
    void SetSettings(SessionSettings const& settings);

    std::optional<uint16> AllocateId();
    void ReleaseId(uint16 id);
    bool IsIdInUse(uint16 id) const;
    std::size_t GetActiveIdCount() const;

private:
    mutable std::mutex _mutex;
    SessionSettings _settings;
    std::vector<bool> _inUse;
    std::size_t _active = 0;
    uint16 _next = 1;
};

#endif
