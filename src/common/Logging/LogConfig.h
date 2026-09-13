/*
 * Project Ambrose by Imjustchico
 * Parses and validates LogsDir, Log.*, Console.Colors, Appender.* and Logger.* into LogSettings with file and line issues.
 */

#ifndef AMBROSE_LOGCONFIG_H
#define AMBROSE_LOGCONFIG_H

#include "Appender.h"
#include "ConfigMgr.h"

#include <optional>
#include <utility>

struct LoggerDefinition
{
    std::string Name;
    LogLevel Level = LogLevel::Disabled;
    std::vector<std::string> Appenders;
    std::filesystem::path File;
    std::size_t Line = 0;

    ConfigIssue MakeIssue(std::string const& message) const;
};

struct LogSettings
{
    static constexpr uint32 DefaultQueueSize = 65536;
    static constexpr uint32 MinQueueSize = 1024;
    static constexpr uint32 MaxQueueSize = 16777216;
    static constexpr uint32 DefaultPendingBuffer = 1000;
    static constexpr uint32 MaxPendingBuffer = 100000;

    std::filesystem::path LogsDir = "logs";
    bool AsyncEnable = false;
    uint32 AsyncQueueSize = DefaultQueueSize;
    LogOverflowPolicy AsyncQueueFull = LogOverflowPolicy::Wait;
    bool Utc = false;
    uint32 PendingBuffer = DefaultPendingBuffer;
    ConsoleColorMode ConsoleColors = ConsoleColorMode::Auto;
    std::vector<AppenderDefinition> Appenders;
    std::vector<LoggerDefinition> Loggers;

    AppenderDefinition const* FindAppender(std::string_view name) const;
    LoggerDefinition const* FindLogger(std::string_view name) const;

    static LogSettings Bootstrap();
};

struct LogConfigResult
{
    std::vector<ConfigIssue> Errors;
    std::vector<ConfigIssue> Warnings;
    std::vector<std::string> InactiveAppenders;

    bool Succeeded() const noexcept { return Errors.empty(); }
    void Merge(LogConfigResult const& other);
};

class LogConfig
{
public:
    static constexpr std::size_t MaxLoggers = 65535;

    static LogSettings Read(ConfigMgr const& config, LogConfigResult& result);
    static LogSettings Parse(std::vector<std::pair<std::string, ConfigEntry>> const& entries, LogConfigResult& result);
    static std::vector<std::string> SplitFields(std::string_view value);
    static std::optional<AppenderDefinition> ParseAppender(std::string const& key, ConfigEntry const& entry, LogConfigResult& result);
    static std::optional<LoggerDefinition> ParseLogger(std::string const& key, ConfigEntry const& entry, LogConfigResult& result);
    static std::filesystem::path Utf8Path(std::string_view utf8);
    static std::vector<std::string_view> GetOptionNames();
};

#endif
