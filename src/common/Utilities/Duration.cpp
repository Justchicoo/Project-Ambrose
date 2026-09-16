/*
 * Project Ambrose by Imjustchico
 * Parses durations written as numbers with s, m, h, d or w units, a bare number meaning seconds, rejecting zero, unknown units, missing numbers and overflow.
 */

#include "Duration.h"
#include "Types.h"

#include <limits>

std::optional<Seconds> Ambrose::ParseDuration(std::string_view text)
{
    uint64 const limit = static_cast<uint64>(std::numeric_limits<Seconds::rep>::max());
    uint64 total = 0;
    uint64 value = 0;
    bool digits = false;
    auto const add = [&](uint64 unit)
    {
        if (!digits || value > limit / unit || total > limit - value * unit)
            return false;
        total += value * unit;
        value = 0;
        digits = false;
        return true;
    };

    for (char const c : text)
    {
        if (c >= '0' && c <= '9')
        {
            uint64 const digit = static_cast<uint64>(c - '0');
            if (value > (limit - digit) / 10)
                return std::nullopt;
            value = value * 10 + digit;
            digits = true;
            continue;
        }
        uint64 unit = 0;
        switch (c)
        {
            case 's': case 'S': unit = 1; break;
            case 'm': case 'M': unit = 60; break;
            case 'h': case 'H': unit = 3600; break;
            case 'd': case 'D': unit = 86400; break;
            case 'w': case 'W': unit = 604800; break;
            default: return std::nullopt;
        }
        if (!add(unit))
            return std::nullopt;
    }
    if (digits && !add(1))
        return std::nullopt;
    if (total == 0)
        return std::nullopt;
    return Seconds(static_cast<Seconds::rep>(total));
}
