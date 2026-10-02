/*
 * Project Ambrose by Imjustchico
 * Reads the protected windows field by field, refusing an hour past 23, a minute past 59, an empty window or a zone the standard library's time zone database does not hold, and finds the local minute of the day in that zone through std::chrono's own database; a window whose end is before its start covers the minutes from its start to midnight and from midnight to its end, and its end minute is outside it.
 */

#include "ProtectedHours.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <exception>

namespace
{
    std::optional<uint32> ReadClock(std::string_view text)
    {
        text = Ambrose::Trim(text);
        std::size_t const colon = text.find(':');
        if (colon == std::string_view::npos || colon == 0 || colon > 2 || text.size() - colon != 3)
            return std::nullopt;
        std::optional<uint32> const hour = Ambrose::StringTo<uint32>(text.substr(0, colon));
        std::optional<uint32> const minute = Ambrose::StringTo<uint32>(text.substr(colon + 1));
        if (!hour || !minute || *hour > 23 || *minute > 59)
            return std::nullopt;
        return *hour * 60 + *minute;
    }

    std::string Clock(uint32 minute)
    {
        return fmt::format("{:02}:{:02}", minute / 60, minute % 60);
    }
}

std::string ProtectedWindow::Describe() const
{
    return fmt::format("{}-{}", Clock(FromMinute), Clock(ToMinute));
}

std::optional<ProtectedHours> ProtectedHours::Parse(std::string_view windows, std::string_view zone, std::string& error)
{
    ProtectedHours hours;
    std::string_view const named = Ambrose::Trim(zone);
    hours._zone = named.empty() ? std::string(DefaultZone) : std::string(named);
    try
    {
        std::chrono::locate_zone(hours._zone);
    }
    catch (std::exception const&)
    {
        error = fmt::format("Power.ProtectedHoursZone names {}, which the time zone database does not hold", hours._zone);
        return std::nullopt;
    }
    std::string_view rest = Ambrose::Trim(windows);
    while (!rest.empty())
    {
        std::size_t const comma = rest.find(',');
        std::string_view const part = Ambrose::Trim(rest.substr(0, comma));
        rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
        std::size_t const dash = part.find('-');
        std::optional<uint32> const from = dash == std::string_view::npos ? std::nullopt : ReadClock(part.substr(0, dash));
        std::optional<uint32> const to = dash == std::string_view::npos ? std::nullopt : ReadClock(part.substr(dash + 1));
        if (!from || !to)
        {
            error = fmt::format("Power.ProtectedHours has \"{}\", which is not a window written HH:MM-HH:MM", part);
            return std::nullopt;
        }
        if (*from == *to)
        {
            error = fmt::format("Power.ProtectedHours has \"{}\", which starts and ends at the same minute", part);
            return std::nullopt;
        }
        hours._windows.push_back(ProtectedWindow{ *from, *to });
    }
    return hours;
}

std::optional<ProtectedWindow> ProtectedHours::Covering(std::chrono::system_clock::time_point when) const
{
    if (_windows.empty())
        return std::nullopt;
    std::chrono::zoned_time const local(std::chrono::locate_zone(_zone), std::chrono::floor<std::chrono::minutes>(when));
    std::chrono::local_time<std::chrono::minutes> const moment = local.get_local_time();
    uint32 const minute = static_cast<uint32>((moment - std::chrono::floor<std::chrono::days>(moment)).count());
    for (ProtectedWindow const& window : _windows)
    {
        bool const inside = window.FromMinute < window.ToMinute ? minute >= window.FromMinute && minute < window.ToMinute
            : minute >= window.FromMinute || minute < window.ToMinute;
        if (inside)
            return window;
    }
    return std::nullopt;
}

std::string ProtectedHours::Describe(ProtectedWindow const& window) const
{
    return fmt::format("{} {}", window.Describe(), _zone);
}
