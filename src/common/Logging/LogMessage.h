/*
 * Project Ambrose by Imjustchico
 * One log record plus prefix rendering, the coloring spans each rendered part covers, multi-line splitting, control-character escaping and UTF-8 repair.
 */

#ifndef AMBROSE_LOGMESSAGE_H
#define AMBROSE_LOGMESSAGE_H

#include "LogCommon.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

enum class LogPart : uint8
{
    Timestamp,
    Level,
    Thread,
    Category,
    Text
};

struct LogSpan
{
    std::size_t Offset = 0;
    std::size_t Length = 0;
    LogPart Part = LogPart::Text;
};

struct LogMessage
{
    LogLevel Level = LogLevel::Info;
    std::string Category;
    std::string Text;
    std::chrono::system_clock::time_point Time;
    uint64 Sequence = 0;
    uint64 ThreadId = 0;
    bool Nested = false;

    void AppendPrefix(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans = nullptr) const;
    void AppendLines(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans = nullptr) const;

    static void AppendSanitized(std::string& out, std::string_view line);
    static uint64 CurrentOsThreadId() noexcept;
};

#endif
