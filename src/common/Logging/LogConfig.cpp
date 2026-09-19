/*
 * Project Ambrose by Imjustchico
 * Turns logging config entries into validated appender and logger definitions, collecting every error and warning.
 */

#include "LogConfig.h"
#include "AppenderConsole.h"
#include "AppenderFile.h"
#include "AppenderStream.h"
#include "LogFileRegistry.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <map>
#include <set>

namespace
{
    constexpr std::string_view AppenderPrefix = "Appender.";
    constexpr std::string_view LoggerPrefix = "Logger.";
    constexpr std::string_view LogPrefix = "Log.";
    constexpr std::string_view LevelHelp = "use 0-6 or disabled, trace, debug, info, warn, error, fatal";

    bool StartsWith(std::string_view text, std::string_view prefix)
    {
        return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
    }

    ConfigIssue EntryIssue(ConfigEntry const& entry, std::string_view key, std::string const& message)
    {
        return ConfigIssue{ entry.File, entry.Line, fmt::format("{}: {}", key, message) };
    }

    bool IsWordStart(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    bool IsWordChar(char c)
    {
        return IsWordStart(c) || (c >= '0' && c <= '9') || c == '_';
    }

    std::string JoinNames(std::vector<AppenderDefinition> const& appenders)
    {
        std::string names;
        for (AppenderDefinition const& appender : appenders)
        {
            if (!names.empty())
                names.append(", ");
            names.append(appender.Name);
        }
        return names.empty() ? std::string("none") : names;
    }

    std::optional<bool> ParseBool(std::string_view text)
    {
        return Ambrose::StringTo<bool>(Ambrose::Trim(text));
    }
}

ConfigIssue LoggerDefinition::MakeIssue(std::string const& message) const
{
    return ConfigIssue{ File, Line, fmt::format("Logger.{}: {}", Name, message) };
}

AppenderDefinition const* LogSettings::FindAppender(std::string_view name) const
{
    for (AppenderDefinition const& appender : Appenders)
        if (appender.Name == name)
            return &appender;
    return nullptr;
}

LoggerDefinition const* LogSettings::FindLogger(std::string_view name) const
{
    for (LoggerDefinition const& logger : Loggers)
        if (logger.Name == name)
            return &logger;
    return nullptr;
}

LogSettings LogSettings::Bootstrap()
{
    LogSettings settings;
    AppenderDefinition console;
    console.Name = "Console";
    console.Type = AppenderType::Console;
    console.Level = LogLevel::Info;
    console.Flags = AppenderFlags::PrefixLevel;
    settings.Appenders.push_back(std::move(console));
    LoggerDefinition root;
    root.Name = "root";
    root.Level = LogLevel::Info;
    root.Appenders.push_back("Console");
    settings.Loggers.push_back(std::move(root));
    return settings;
}

void LogConfigResult::Merge(LogConfigResult const& other)
{
    Errors.insert(Errors.end(), other.Errors.begin(), other.Errors.end());
    Warnings.insert(Warnings.end(), other.Warnings.begin(), other.Warnings.end());
    InactiveAppenders.insert(InactiveAppenders.end(), other.InactiveAppenders.begin(), other.InactiveAppenders.end());
}

std::filesystem::path LogConfig::Utf8Path(std::string_view utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::vector<std::string_view> LogConfig::GetOptionNames()
{
    return { "LogsDir", "Console.Colors", "Console.Timestamp", "Console.CategoryWidth", "Console.RepeatCategory", "Log.Async.Enable", "Log.Async.QueueSize", "Log.Async.QueueFull", "Log.Utc", "Log.PendingBuffer" };
}

std::vector<std::string> LogConfig::SplitFields(std::string_view value)
{
    std::vector<std::string> fields;
    std::string current;
    bool quoted = false;
    auto finish = [&]()
    {
        std::string_view field = Ambrose::Trim(current);
        if (field.size() >= 2 && field.front() == '"' && field.back() == '"')
            field = field.substr(1, field.size() - 2);
        fields.emplace_back(field);
        current.clear();
    };
    for (char const c : value)
    {
        if (c == '"')
            quoted = !quoted;
        if (c == ',' && !quoted)
        {
            finish();
            continue;
        }
        current.push_back(c);
    }
    finish();
    return fields;
}

std::optional<AppenderDefinition> LogConfig::ParseAppender(std::string const& key, ConfigEntry const& entry, LogConfigResult& result)
{
    std::string_view const name = std::string_view(key).substr(AppenderPrefix.size());
    if (name.find('.') != std::string_view::npos)
    {
        result.Errors.push_back(EntryIssue(entry, key, "appender names cannot contain '.'"));
        return std::nullopt;
    }
    if (name.empty() || !IsWordStart(name.front()) || !std::all_of(name.begin(), name.end(), IsWordChar))
    {
        result.Errors.push_back(EntryIssue(entry, key, "appender names start with a letter and use letters, digits and '_'"));
        return std::nullopt;
    }
    std::vector<std::string> fields = SplitFields(entry.Value);
    if (fields.size() < 3)
    {
        result.Errors.push_back(EntryIssue(entry, key, "expected Type,Level,Flags[,fields...], for example 1,3,7"));
        return std::nullopt;
    }
    AppenderDefinition definition;
    definition.Name = std::string(name);
    definition.File = entry.File;
    definition.Line = entry.Line;

    std::optional<AppenderType> const type = Ambrose::Logging::ParseAppenderType(fields[0]);
    if (!type)
    {
        result.Errors.push_back(EntryIssue(entry, key, fmt::format("type '{}' is not valid; use 1-255 or Console, File, Stream, DB", fields[0])));
        return std::nullopt;
    }
    std::optional<LogLevel> const level = Ambrose::Logging::ParseLogLevel(fields[1]);
    if (!level)
    {
        result.Errors.push_back(EntryIssue(entry, key, fmt::format("level '{}' is out of range; {}", fields[1], LevelHelp)));
        return std::nullopt;
    }
    std::optional<AppenderFlags> const flags = Ambrose::Logging::ParseAppenderFlags(fields[2]);
    if (!flags)
    {
        result.Errors.push_back(EntryIssue(entry, key, fmt::format("flags '{}' are not valid; combine 0x01 time, 0x02 level, 0x04 category, 0x08 timestamped file name, 0x10 backup, 0x20 thread", fields[2])));
        return std::nullopt;
    }
    definition.Type = *type;
    definition.Level = *level;
    definition.Flags = *flags;
    definition.Fields.assign(std::make_move_iterator(fields.begin() + 3), std::make_move_iterator(fields.end()));
    return definition;
}

std::optional<LoggerDefinition> LogConfig::ParseLogger(std::string const& key, ConfigEntry const& entry, LogConfigResult& result)
{
    std::string_view const name = std::string_view(key).substr(LoggerPrefix.size());
    if (StartsWith(name, "root."))
    {
        result.Errors.push_back(EntryIssue(entry, key, "root has no children; name the category directly"));
        return std::nullopt;
    }
    for (std::string_view const segment : Ambrose::Tokenize(name, '.', true))
    {
        if (segment.empty() || !std::all_of(segment.begin(), segment.end(), IsWordChar))
        {
            result.Errors.push_back(EntryIssue(entry, key, "logger names are dot-separated segments of letters, digits and '_'"));
            return std::nullopt;
        }
    }
    std::string_view const value = Ambrose::Trim(entry.Value);
    std::size_t const comma = value.find(',');
    if (comma == std::string_view::npos)
    {
        result.Errors.push_back(EntryIssue(entry, key, "expected Level,Appender[ Appender...], for example 3,Console Server"));
        return std::nullopt;
    }
    std::optional<LogLevel> const level = Ambrose::Logging::ParseLogLevel(value.substr(0, comma));
    if (!level)
    {
        result.Errors.push_back(EntryIssue(entry, key, fmt::format("level '{}' is out of range; {}", Ambrose::Trim(value.substr(0, comma)), LevelHelp)));
        return std::nullopt;
    }
    LoggerDefinition definition;
    definition.Name = std::string(name);
    definition.Level = *level;
    definition.File = entry.File;
    definition.Line = entry.Line;
    std::string list(value.substr(comma + 1));
    std::replace(list.begin(), list.end(), ',', ' ');
    std::set<std::string, std::less<>> seen;
    for (std::string_view const token : Ambrose::Tokenize(list, ' ', false))
    {
        std::string_view const appender = Ambrose::Trim(token);
        if (appender.empty())
            continue;
        if (!seen.emplace(appender).second)
        {
            result.Warnings.push_back(EntryIssue(entry, key, fmt::format("appender '{}' is listed more than once; the duplicate is ignored", appender)));
            continue;
        }
        definition.Appenders.emplace_back(appender);
    }
    if (definition.Appenders.empty())
        result.Warnings.push_back(EntryIssue(entry, key, fmt::format("no appenders are listed, so messages routed to '{}' are dropped", definition.Name)));
    return definition;
}

LogSettings LogConfig::Read(ConfigMgr const& config, LogConfigResult& result)
{
    std::vector<std::pair<std::string, ConfigEntry>> entries;
    std::set<std::string, std::less<>> added;
    auto add = [&](std::string const& key)
    {
        if (!added.emplace(key).second)
            return;
        if (std::optional<ConfigEntry> entry = config.Resolve(key))
            entries.emplace_back(key, std::move(*entry));
    };
    for (std::string_view const option : GetOptionNames())
        add(std::string(option));
    for (std::string_view const prefix : { LogPrefix, AppenderPrefix, LoggerPrefix })
        for (std::string const& key : config.GetKeysByString(prefix))
            add(key);
    return Parse(entries, result);
}

LogSettings LogConfig::Parse(std::vector<std::pair<std::string, ConfigEntry>> const& entries, LogConfigResult& result)
{
    LogSettings settings;
    std::set<std::string, std::less<>> malformedAppenders;
    bool rootKeySeen = false;
    for (auto const& [key, entry] : entries)
    {
        std::string_view const value = Ambrose::Trim(entry.Value);
        if (key == "LogsDir")
        {
            settings.LogsDir = Utf8Path(value);
            continue;
        }
        if (key == "Console.Colors")
        {
            std::optional<uint8> const mode = Ambrose::StringTo<uint8>(value);
            if (!mode || *mode > 2)
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is not valid; use 0 never, 1 when the output is a terminal, or 2 always", value)));
            else
                settings.ConsoleColors = static_cast<ConsoleColorMode>(*mode);
            continue;
        }
        if (key == "Console.Timestamp")
        {
            std::string const style = Ambrose::ToLower(std::string(value));
            if (style == "short")
                settings.ConsoleTimestamp = LogTimestampStyle::Short;
            else if (style == "full")
                settings.ConsoleTimestamp = LogTimestampStyle::Full;
            else if (style == "off")
                settings.ConsoleTimestamp = LogTimestampStyle::Off;
            else
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is not valid; use short for the time alone, full for the date and time, or off", value)));
            continue;
        }
        if (key == "Console.CategoryWidth")
        {
            std::optional<uint16> const width = Ambrose::StringTo<uint16>(value);
            if (!width || *width > LogLayout::MaxCategoryWidth)
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is not valid; use 0 to write the category inline and unpadded, or a width up to {}", value, LogLayout::MaxCategoryWidth)));
            else
                settings.ConsoleCategoryWidth = *width;
            continue;
        }
        if (key == "Console.RepeatCategory")
        {
            std::optional<bool> const flag = ParseBool(value);
            if (!flag)
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is not a boolean; use 1, 0, true, false, yes or no", value)));
            else
                settings.ConsoleRepeatCategory = *flag;
            continue;
        }
        if (key == "Log.Async.Enable" || key == "Log.Utc")
        {
            std::optional<bool> const flag = ParseBool(value);
            if (!flag)
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is not a boolean; use 1, 0, true, false, yes or no", value)));
            else if (key == "Log.Utc")
                settings.Utc = *flag;
            else
                settings.AsyncEnable = *flag;
            continue;
        }
        if (key == "Log.Async.QueueSize")
        {
            std::optional<uint32> const size = Ambrose::StringTo<uint32>(value);
            if (!size || *size < LogSettings::MinQueueSize || *size > LogSettings::MaxQueueSize)
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is out of range; use {}-{}", value, LogSettings::MinQueueSize, LogSettings::MaxQueueSize)));
            else
                settings.AsyncQueueSize = *size;
            continue;
        }
        if (key == "Log.Async.QueueFull")
        {
            std::optional<uint8> const policy = Ambrose::StringTo<uint8>(value);
            if (!policy || *policy > 1)
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is not valid; use 0 to wait for room or 1 to drop the line", value)));
            else
                settings.AsyncQueueFull = static_cast<LogOverflowPolicy>(*policy);
            continue;
        }
        if (key == "Log.PendingBuffer")
        {
            std::optional<uint32> const size = Ambrose::StringTo<uint32>(value);
            if (!size || *size > LogSettings::MaxPendingBuffer)
                result.Errors.push_back(EntryIssue(entry, key, fmt::format("'{}' is out of range; use 0-{}", value, LogSettings::MaxPendingBuffer)));
            else
                settings.PendingBuffer = *size;
            continue;
        }
        if (StartsWith(key, LogPrefix))
        {
            result.Warnings.push_back(EntryIssue(entry, key, fmt::format("{} is not a logging option", key)));
            continue;
        }
        if (StartsWith(key, AppenderPrefix))
        {
            if (std::optional<AppenderDefinition> definition = ParseAppender(key, entry, result))
                settings.Appenders.push_back(std::move(*definition));
            else
                malformedAppenders.emplace(key.substr(AppenderPrefix.size()));
            continue;
        }
        if (StartsWith(key, LoggerPrefix))
        {
            rootKeySeen = rootKeySeen || key == "Logger.root";
            if (std::optional<LoggerDefinition> definition = ParseLogger(key, entry, result))
                settings.Loggers.push_back(std::move(*definition));
        }
    }

    std::map<std::filesystem::path, std::string> filePaths;
    bool streamSeen = false;
    for (AppenderDefinition const& appender : settings.Appenders)
    {
        switch (appender.Type)
        {
            case AppenderType::Console:
                AppenderConsole::ValidateFields(appender, result);
                if (HasAppenderFlag(appender.Flags, AppenderFlags::TimestampFileName) || HasAppenderFlag(appender.Flags, AppenderFlags::BackupExistingFile))
                    result.Warnings.push_back(appender.MakeIssue("flags 0x08 and 0x10 apply only to File appenders; they are ignored"));
                break;
            case AppenderType::File:
                if (std::optional<AppenderFileSettings> const file = AppenderFile::ParseSettings(appender, settings.LogsDir, result))
                {
                    auto const [existing, inserted] = filePaths.emplace(LogFileRegistry::MakeKey(file->Path), appender.Name);
                    if (!inserted)
                        result.Errors.push_back(appender.MakeIssue(fmt::format("writes to the same file as Appender.{}", existing->second)));
                }
                break;
            case AppenderType::Stream:
                AppenderStream::ParseBacklog(appender, result);
                if (streamSeen)
                    result.Errors.push_back(appender.MakeIssue("only one Stream appender is allowed"));
                streamSeen = true;
                break;
            default:
                break;
        }
    }

    std::set<std::string, std::less<>> used;
    bool rootSeen = false;
    for (LoggerDefinition const& logger : settings.Loggers)
    {
        rootSeen = rootSeen || logger.Name == "root";
        for (std::string const& name : logger.Appenders)
        {
            if (!settings.FindAppender(name) && !malformedAppenders.contains(name))
                result.Errors.push_back(logger.MakeIssue(fmt::format("references appender '{}', which is not defined; defined appenders: {}", name, JoinNames(settings.Appenders))));
            used.insert(name);
        }
    }
    if (!rootSeen && !rootKeySeen)
        result.Errors.push_back(ConfigIssue{ {}, 0, "Logger.root is required, for example Logger.root = 3,Console Server" });
    if (settings.Loggers.size() > MaxLoggers)
        result.Errors.push_back(ConfigIssue{ {}, 0, fmt::format("at most {} loggers are allowed", MaxLoggers) });
    for (AppenderDefinition const& appender : settings.Appenders)
        if (!used.contains(appender.Name))
            result.Warnings.push_back(appender.MakeIssue("no logger uses this appender"));

    std::stable_partition(settings.Loggers.begin(), settings.Loggers.end(), [](LoggerDefinition const& logger) { return logger.Name == "root"; });
    return settings;
}
