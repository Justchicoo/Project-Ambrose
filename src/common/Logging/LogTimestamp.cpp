/*
 * Project Ambrose by Imjustchico
 * Formats YYYY-MM-DD_HH:MM:SS.mmm prefixes and YYYY-MM-DD_HH-MM-SS file stamps without allocating.
 */

#include "LogTimestamp.h"

#include <fmt/format.h>

namespace
{
    struct SecondCache
    {
        int64 Second;
        bool Valid;
        char Text[20];
    };

    struct ThreadTimestampCache
    {
        SecondCache Zones[2];
        char Output[LogTimestamp::PrefixLength + 1];
    };

    thread_local ThreadTimestampCache Cache{};

    void SplitMilliseconds(std::chrono::system_clock::time_point time, int64& seconds, int32& milliseconds) noexcept
    {
        int64 const total = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
        seconds = total / 1000;
        int64 remainder = total % 1000;
        if (remainder < 0)
        {
            remainder += 1000;
            --seconds;
        }
        milliseconds = static_cast<int32>(remainder);
    }

    std::tm SafeBreakDown(int64 seconds, bool utc) noexcept
    {
        std::tm parts{};
        if (!LogTimestamp::BreakDown(static_cast<std::time_t>(seconds), utc, parts))
            parts = std::tm{};
        return parts;
    }

    int ClampYear(std::tm const& parts) noexcept
    {
        int const year = parts.tm_year + 1900;
        return year < 0 ? 0 : (year > 9999 ? 9999 : year);
    }
}

bool LogTimestamp::BreakDown(std::time_t seconds, bool utc, std::tm& out) noexcept
{
#ifdef _WIN32
    return (utc ? gmtime_s(&out, &seconds) : localtime_s(&out, &seconds)) == 0;
#else
    return (utc ? gmtime_r(&seconds, &out) : localtime_r(&seconds, &out)) != nullptr;
#endif
}

std::string_view LogTimestamp::FormatPrefix(std::chrono::system_clock::time_point time, bool utc) noexcept
{
    int64 seconds = 0;
    int32 milliseconds = 0;
    SplitMilliseconds(time, seconds, milliseconds);
    SecondCache& zone = Cache.Zones[utc ? 1 : 0];
    if (!zone.Valid || zone.Second != seconds)
    {
        std::tm const parts = SafeBreakDown(seconds, utc);
        fmt::format_to_n(zone.Text, 19, "{:04}-{:02}-{:02}_{:02}:{:02}:{:02}", ClampYear(parts), parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
        zone.Text[19] = '\0';
        zone.Second = seconds;
        zone.Valid = true;
    }
    fmt::format_to_n(Cache.Output, PrefixLength, "{}.{:03}", std::string_view(zone.Text, 19), milliseconds);
    Cache.Output[PrefixLength] = '\0';
    return std::string_view(Cache.Output, PrefixLength);
}

std::array<char, LogTimestamp::FileNameLength> LogTimestamp::FormatFileName(std::chrono::system_clock::time_point time, bool utc) noexcept
{
    int64 seconds = 0;
    int32 milliseconds = 0;
    SplitMilliseconds(time, seconds, milliseconds);
    std::tm const parts = SafeBreakDown(seconds, utc);
    std::array<char, FileNameLength> name{};
    fmt::format_to_n(name.data(), name.size(), "{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", ClampYear(parts), parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    return name;
}
