/*
 * Project Ambrose by Imjustchico
 * Short aliases for the std::chrono duration types used by timers and schedulers, and parsing of operator durations such as 90s, 30m, 12h, 7d, 2w or 1d12h.
 */

#ifndef AMBROSE_DURATION_H
#define AMBROSE_DURATION_H

#include <chrono>
#include <optional>
#include <string_view>

using Milliseconds = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;
using Minutes = std::chrono::minutes;
using Hours = std::chrono::hours;

namespace Ambrose
{
    std::optional<Seconds> ParseDuration(std::string_view text);
}

#endif
