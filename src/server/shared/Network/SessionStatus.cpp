/*
 * Project Ambrose by Imjustchico
 * Names session statuses for logs and joins the statuses in a mask into readable text.
 */

#include "SessionStatus.h"

std::string_view SessionStatuses::GetName(SessionStatus status) noexcept
{
    switch (status)
    {
        case SessionStatus::Connected: return "Connected";
        case SessionStatus::Authenticated: return "Authenticated";
        case SessionStatus::CharacterSelected: return "CharacterSelected";
        case SessionStatus::LoggedIn: return "LoggedIn";
        case SessionStatus::InWorld: return "InWorld";
    }
    return "Unknown";
}

std::string SessionStatuses::Describe(SessionStatusMask mask)
{
    if (mask == 0)
        return "no status";
    std::string text;
    for (std::size_t index = 0; index < Count; ++index)
    {
        SessionStatus const status = static_cast<SessionStatus>(index);
        if (!Allows(mask, status))
            continue;
        if (!text.empty())
            text += " or ";
        text += GetName(status);
    }
    return text;
}
