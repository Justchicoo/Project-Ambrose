/*
 * Project Ambrose by Imjustchico
 * Thread-safe local or UTC time breakdown with per-thread per-second caching for prefixes and file names.
 */

#ifndef AMBROSE_LOGTIMESTAMP_H
#define AMBROSE_LOGTIMESTAMP_H

#include "Types.h"

#include <array>
#include <chrono>
#include <ctime>
#include <string_view>

class LogTimestamp
{
public:
    static constexpr std::size_t PrefixLength = 23;
    static constexpr std::size_t FileNameLength = 19;

    static std::string_view FormatPrefix(std::chrono::system_clock::time_point time, bool utc) noexcept;
    static std::array<char, FileNameLength> FormatFileName(std::chrono::system_clock::time_point time, bool utc) noexcept;
    static bool BreakDown(std::time_t seconds, bool utc, std::tm& out) noexcept;
};

#endif
