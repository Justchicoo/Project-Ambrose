/*
 * Project Ambrose by Imjustchico
 * Validates file appender fields and writes each record's prefixed lines into its shared log file.
 */

#include "AppenderFile.h"
#include "LogConfig.h"
#include "LogFileRegistry.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <system_error>

AppenderFile::AppenderFile(AppenderDefinition const& definition, std::shared_ptr<LogFile> file, bool utc)
    : Appender(definition.Name, definition.Type, definition.Level, definition.Flags), _file(std::move(file)), _utc(utc), _flushEveryLine(true)
{
}

AppenderTypeInfo AppenderFile::GetTypeInfo()
{
    AppenderTypeInfo info;
    info.Type = AppenderType::File;
    info.Name = "File";
    info.Create = [](AppenderDefinition const& definition, AppenderCreateContext const& context, LogConfigResult& result) -> std::shared_ptr<Appender>
    {
        LogConfigResult local;
        std::optional<AppenderFileSettings> settings = ParseSettings(definition, context.LogsDir, local);
        result.Errors.insert(result.Errors.end(), local.Errors.begin(), local.Errors.end());
        if (!settings)
            return nullptr;
        settings->Options.Utc = context.Utc;
        settings->Options.LoadTime = context.LoadTime;
        std::string error;
        std::shared_ptr<LogFile> file = context.Files.Acquire(settings->Path, settings->Options, error);
        if (!file)
        {
            result.Errors.push_back(definition.MakeIssue(error));
            return nullptr;
        }
        auto appender = std::make_shared<AppenderFile>(definition, std::move(file), context.Utc);
        appender->_flushEveryLine = settings->Options.FlushInterval.count() == 0;
        return appender;
    };
    return info;
}

std::optional<AppenderFileSettings> AppenderFile::ParseSettings(AppenderDefinition const& definition, std::filesystem::path const& logsDir, LogConfigResult& result)
{
    std::vector<std::string> const& fields = definition.Fields;
    if (fields.size() > MaxFields)
    {
        result.Errors.push_back(definition.MakeIssue("too many fields; File takes Type,Level,Flags,FileName[,Mode[,MaxFileSize[,MaxBackups[,FlushIntervalMs]]]]"));
        return std::nullopt;
    }
    auto field = [&](std::size_t index) -> std::string_view
    {
        return index < fields.size() ? Ambrose::Trim(fields[index]) : std::string_view();
    };

    std::string_view const fileName = field(0);
    if (fileName.empty())
    {
        result.Errors.push_back(definition.MakeIssue("a File appender needs a file name, for example 2,2,7,Server.log"));
        return std::nullopt;
    }
    if (fileName.find("%s") != std::string_view::npos)
    {
        result.Errors.push_back(definition.MakeIssue("file names cannot contain '%s'; set flag 0x08 to add a timestamp to the name"));
        return std::nullopt;
    }

    AppenderFileSettings settings;
    std::string_view const mode = field(1);
    if (mode.empty() || Ambrose::EqualsIgnoreCase(mode, "a"))
        settings.Options.Mode = LogFileMode::Append;
    else if (Ambrose::EqualsIgnoreCase(mode, "w"))
        settings.Options.Mode = LogFileMode::Truncate;
    else
    {
        result.Errors.push_back(definition.MakeIssue(fmt::format("mode '{}' is not valid; use a to append or w to rewrite on start", mode)));
        return std::nullopt;
    }

    if (std::string_view const size = field(2); !size.empty())
    {
        std::optional<uint64> const bytes = Ambrose::Logging::ParseByteSize(size);
        if (!bytes || (*bytes != 0 && *bytes < MinRotationSize))
        {
            result.Errors.push_back(definition.MakeIssue(fmt::format("MaxFileSize '{}' is not valid; use 0 or a size of at least 1K, such as 64M", size)));
            return std::nullopt;
        }
        settings.Options.MaxFileSize = *bytes;
    }

    if (std::string_view const backups = field(3); !backups.empty())
    {
        std::optional<uint32> const count = Ambrose::StringTo<uint32>(backups);
        if (!count || *count > MaxBackupsLimit)
        {
            result.Errors.push_back(definition.MakeIssue(fmt::format("MaxBackups '{}' is out of range; use 0-{}", backups, MaxBackupsLimit)));
            return std::nullopt;
        }
        settings.Options.MaxBackups = *count;
    }

    if (std::string_view const interval = field(4); !interval.empty())
    {
        std::optional<uint32> const milliseconds = Ambrose::StringTo<uint32>(interval);
        if (!milliseconds || *milliseconds > MaxFlushIntervalMs)
        {
            result.Errors.push_back(definition.MakeIssue(fmt::format("FlushIntervalMs '{}' is out of range; use 0-{}", interval, MaxFlushIntervalMs)));
            return std::nullopt;
        }
        settings.Options.FlushInterval = std::chrono::milliseconds(*milliseconds);
    }

    settings.Options.TimestampName = HasAppenderFlag(definition.Flags, AppenderFlags::TimestampFileName);
    settings.Options.BackupExisting = HasAppenderFlag(definition.Flags, AppenderFlags::BackupExistingFile);
    if (settings.Options.BackupExisting && settings.Options.Mode == LogFileMode::Append)
    {
        result.Warnings.push_back(definition.MakeIssue("flag 0x10 backs up the file only with mode w; it is ignored"));
        settings.Options.BackupExisting = false;
    }

    std::filesystem::path path = LogConfig::Utf8Path(fileName);
    if (path.is_relative())
        path = logsDir / path;
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    settings.Path = (error ? path : absolute).lexically_normal();
    return settings;
}

std::shared_ptr<LogFile> const& AppenderFile::GetFile() const noexcept
{
    return _file;
}

void AppenderFile::Flush()
{
    _file->Flush();
}

void AppenderFile::WriteMessage(LogMessage const& message)
{
    std::string lines;
    lines.reserve(message.Text.size() + 64);
    message.AppendLines(lines, GetFlags(), _utc);
    _file->WriteLines(lines, _flushEveryLine || message.Level >= LogLevel::Error);
}
