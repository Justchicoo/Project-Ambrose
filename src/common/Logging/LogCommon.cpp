/*
 * Project Ambrose by Imjustchico
 * Names and parsers for log levels, appender types, appender flags, console colors, and byte sizes.
 */

#include "LogCommon.h"
#include "StringUtil.h"

#include <array>
#include <limits>

namespace
{
    struct LevelName
    {
        std::string_view Name;
        std::string_view Padded;
    };

    constexpr std::array<LevelName, 7> LevelNames{ {
        { "DISABLED", "OFF  " },
        { "TRACE", "TRACE" },
        { "DEBUG", "DEBUG" },
        { "INFO", "INFO " },
        { "WARN", "WARN " },
        { "ERROR", "ERROR" },
        { "FATAL", "FATAL" }
    } };
}

std::string_view Ambrose::Logging::GetLogLevelName(LogLevel level) noexcept
{
    std::size_t const index = static_cast<std::size_t>(level);
    return index < LevelNames.size() ? LevelNames[index].Name : std::string_view("UNKNOWN");
}

std::string_view Ambrose::Logging::GetLogLevelPaddedName(LogLevel level) noexcept
{
    std::size_t const index = static_cast<std::size_t>(level);
    return index < LevelNames.size() ? LevelNames[index].Padded : std::string_view("?????");
}

std::optional<LogLevel> Ambrose::Logging::ParseLogLevel(std::string_view text) noexcept
{
    text = Ambrose::Trim(text);
    if (std::optional<uint8> const number = Ambrose::StringTo<uint8>(text))
    {
        if (*number <= static_cast<uint8>(LogLevel::Fatal))
            return static_cast<LogLevel>(*number);
        return std::nullopt;
    }
    static constexpr std::array<std::pair<std::string_view, LogLevel>, 8> Names{ {
        { "disabled", LogLevel::Disabled },
        { "trace", LogLevel::Trace },
        { "debug", LogLevel::Debug },
        { "info", LogLevel::Info },
        { "warn", LogLevel::Warn },
        { "warning", LogLevel::Warn },
        { "error", LogLevel::Error },
        { "fatal", LogLevel::Fatal }
    } };
    for (auto const& [name, level] : Names)
        if (Ambrose::EqualsIgnoreCase(text, name))
            return level;
    return std::nullopt;
}

std::string_view Ambrose::Logging::GetAppenderTypeName(AppenderType type) noexcept
{
    switch (type)
    {
        case AppenderType::None: return "None";
        case AppenderType::Console: return "Console";
        case AppenderType::File: return "File";
        case AppenderType::Stream: return "Stream";
        case AppenderType::DB: return "DB";
    }
    return static_cast<uint8>(type) >= AppenderTypeFirstModule ? std::string_view("Module") : std::string_view("Core");
}

std::optional<AppenderType> Ambrose::Logging::ParseAppenderType(std::string_view text) noexcept
{
    text = Ambrose::Trim(text);
    if (std::optional<uint8> const number = Ambrose::StringTo<uint8>(text))
    {
        if (*number == 0)
            return std::nullopt;
        return static_cast<AppenderType>(*number);
    }
    static constexpr std::array<std::pair<std::string_view, AppenderType>, 4> Names{ {
        { "Console", AppenderType::Console },
        { "File", AppenderType::File },
        { "Stream", AppenderType::Stream },
        { "DB", AppenderType::DB }
    } };
    for (auto const& [name, type] : Names)
        if (Ambrose::EqualsIgnoreCase(text, name))
            return type;
    return std::nullopt;
}

std::optional<AppenderFlags> Ambrose::Logging::ParseAppenderFlags(std::string_view text) noexcept
{
    text = Ambrose::Trim(text);
    std::optional<uint32> value;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        value = Ambrose::StringTo<uint32>(text.substr(2), 16);
    else
        value = Ambrose::StringTo<uint32>(text);
    if (!value || (*value & ~static_cast<uint32>(AppenderFlagsMask)) != 0)
        return std::nullopt;
    return static_cast<AppenderFlags>(*value);
}

std::optional<ConsoleColor> Ambrose::Logging::ParseConsoleColor(std::string_view text) noexcept
{
    std::optional<uint8> const number = Ambrose::StringTo<uint8>(Ambrose::Trim(text));
    if (!number || *number > static_cast<uint8>(ConsoleColor::Default))
        return std::nullopt;
    return static_cast<ConsoleColor>(*number);
}

std::optional<uint64> Ambrose::Logging::ParseByteSize(std::string_view text) noexcept
{
    text = Ambrose::Trim(text);
    if (text.empty())
        return std::nullopt;
    uint64 multiplier = 1;
    switch (text.back())
    {
        case 'k': case 'K': multiplier = uint64{ 1 } << 10; break;
        case 'm': case 'M': multiplier = uint64{ 1 } << 20; break;
        case 'g': case 'G': multiplier = uint64{ 1 } << 30; break;
        default: break;
    }
    if (multiplier != 1)
        text.remove_suffix(1);
    std::optional<uint64> const number = Ambrose::StringTo<uint64>(text);
    if (!number || *number > std::numeric_limits<uint64>::max() / multiplier)
        return std::nullopt;
    return *number * multiplier;
}

bool Ambrose::Logging::IsCategoryWithin(std::string_view category, std::string_view prefix) noexcept
{
    if (prefix.empty())
        return true;
    if (category.size() < prefix.size() || category.substr(0, prefix.size()) != prefix)
        return false;
    return category.size() == prefix.size() || category[prefix.size()] == '.';
}
