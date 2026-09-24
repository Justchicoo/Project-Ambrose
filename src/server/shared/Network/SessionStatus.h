/*
 * Project Ambrose by Imjustchico
 * How far a client session has progressed, shared by every app, and bit masks of the statuses a message handler accepts.
 */

#ifndef AMBROSE_SESSIONSTATUS_H
#define AMBROSE_SESSIONSTATUS_H

#include "Types.h"

#include <string>
#include <string_view>

enum class SessionStatus : uint8
{
    Connected,
    Authenticated,
    CharacterSelected,
    LoggedIn,
    InWorld
};

using SessionStatusMask = uint8;

namespace SessionStatuses
{
    inline constexpr std::size_t Count = 5;

    constexpr SessionStatusMask Bit(SessionStatus status) noexcept
    {
        return static_cast<SessionStatusMask>(1u << static_cast<uint8>(status));
    }

    inline constexpr SessionStatusMask Connected = Bit(SessionStatus::Connected);
    inline constexpr SessionStatusMask Authenticated = Bit(SessionStatus::Authenticated);
    inline constexpr SessionStatusMask CharacterSelected = Bit(SessionStatus::CharacterSelected);
    inline constexpr SessionStatusMask LoggedIn = Bit(SessionStatus::LoggedIn);
    inline constexpr SessionStatusMask InWorld = Bit(SessionStatus::InWorld);
    inline constexpr SessionStatusMask Any = Connected | Authenticated | CharacterSelected | LoggedIn | InWorld;

    constexpr bool Allows(SessionStatusMask mask, SessionStatus status) noexcept
    {
        return (mask & Bit(status)) != 0;
    }

    std::string_view GetName(SessionStatus status) noexcept;
    std::string Describe(SessionStatusMask mask);
}

#endif
