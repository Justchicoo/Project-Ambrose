/*
 * Project Ambrose by Imjustchico
 * File appender type 2 that renders prefixed lines into a shared LogFile.
 */

#ifndef AMBROSE_APPENDERFILE_H
#define AMBROSE_APPENDERFILE_H

#include "AppenderRegistry.h"
#include "LogFile.h"

#include <optional>

struct AppenderFileSettings
{
    std::filesystem::path Path;
    LogFileOptions Options;
};

class AppenderFile : public Appender
{
public:
    static constexpr std::size_t MaxFields = 5;
    static constexpr uint64 MinRotationSize = 1024;
    static constexpr uint32 MaxBackupsLimit = 1000;
    static constexpr uint32 MaxFlushIntervalMs = 60000;

    AppenderFile(AppenderDefinition const& definition, std::shared_ptr<LogFile> file, bool utc);

    static AppenderTypeInfo GetTypeInfo();
    static std::optional<AppenderFileSettings> ParseSettings(AppenderDefinition const& definition, std::filesystem::path const& logsDir, LogConfigResult& result);

    std::shared_ptr<LogFile> const& GetFile() const noexcept;
    void Flush() override;

protected:
    void WriteMessage(LogMessage const& message) override;

private:
    std::shared_ptr<LogFile> _file;
    bool _utc;
    bool _flushEveryLine;
};

#endif
