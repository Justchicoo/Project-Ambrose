/*
 * Project Ambrose by Imjustchico
 * Log levels, appender type ids, appender flags, console colors, and their names and parsers.
 */

#ifndef AMBROSE_LOGCOMMON_H
#define AMBROSE_LOGCOMMON_H

#include "EnumFlag.h"
#include "Types.h"

#include <optional>
#include <string_view>

enum class LogLevel : uint8
{
    Disabled = 0,
    Trace = 1,
    Debug = 2,
    Info = 3,
    Warn = 4,
    Error = 5,
    Fatal = 6
};

inline constexpr uint8 LogLevelNever = 7;

enum class AppenderType : uint8
{
    None = 0,
    Console = 1,
    File = 2,
    Stream = 3,
    DB = 4
};

inline constexpr uint8 AppenderTypeFirstModule = 100;

enum class AppenderFlags : uint8
{
    None = 0x00,
    PrefixTimestamp = 0x01,
    PrefixLevel = 0x02,
    PrefixCategory = 0x04,
    TimestampFileName = 0x08,
    BackupExistingFile = 0x10,
    PrefixThread = 0x20
};

DEFINE_ENUM_FLAG(AppenderFlags);

inline constexpr uint8 AppenderFlagsMask = 0x3F;

enum class ConsoleColor : uint8
{
    Black = 0,
    Red,
    Green,
    Brown,
    Blue,
    Magenta,
    Cyan,
    Grey,
    Yellow,
    LightRed,
    LightGreen,
    LightBlue,
    LightMagenta,
    LightCyan,
    White,
    Default
};

enum class ConsoleColorMode : uint8
{
    Never = 0,
    Auto = 1,
    Always = 2
};

enum class LogOverflowPolicy : uint8
{
    Wait = 0,
    Drop = 1
};

constexpr bool IsLevelEnabled(LogLevel threshold, LogLevel message) noexcept
{
    return threshold != LogLevel::Disabled && message != LogLevel::Disabled && message >= threshold;
}

constexpr bool HasAppenderFlag(AppenderFlags flags, AppenderFlags flag) noexcept
{
    return (static_cast<uint8>(flags) & static_cast<uint8>(flag)) != 0;
}

namespace Ambrose::Logging
{
    std::string_view GetLogLevelName(LogLevel level) noexcept;
    std::string_view GetLogLevelPaddedName(LogLevel level) noexcept;
    std::optional<LogLevel> ParseLogLevel(std::string_view text) noexcept;
    std::string_view GetAppenderTypeName(AppenderType type) noexcept;
    std::optional<AppenderType> ParseAppenderType(std::string_view text) noexcept;
    std::optional<AppenderFlags> ParseAppenderFlags(std::string_view text) noexcept;
    std::optional<ConsoleColor> ParseConsoleColor(std::string_view text) noexcept;
    std::optional<uint64> ParseByteSize(std::string_view text) noexcept;
    bool IsCategoryWithin(std::string_view category, std::string_view prefix) noexcept;
}

#endif
