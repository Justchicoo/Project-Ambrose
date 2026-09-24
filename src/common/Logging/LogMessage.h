/*
 * Project Ambrose by Imjustchico
 * One log record, the place in the code it was written at, the format template it was written from, which is what groups two errors raised at one line as one error however their arguments differed, plus prefix rendering under a layout, the coloring spans each rendered part covers, multi-line splitting, control-character escaping and UTF-8 repair. The template is copied rather than pointed at, because a caller may build one at runtime through fmt::runtime and a view of that would outlive it.
 */

#ifndef AMBROSE_LOGMESSAGE_H
#define AMBROSE_LOGMESSAGE_H

#include "LogCommon.h"
#include "LogSource.h"

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
    Body,
    Value,
    Punctuation,
    Padding
};

enum class LogTimestampStyle : uint8
{
    Full,
    Short,
    Off
};

struct LogLayout
{
    static constexpr uint16 DefaultCategoryWidth = 18;
    static constexpr uint16 MaxCategoryWidth = 64;

    LogTimestampStyle Timestamp = LogTimestampStyle::Full;
    uint16 CategoryWidth = 0;
    bool SuppressCategory = false;
};

struct LogSpan
{
    std::size_t Offset = 0;
    std::size_t Length = 0;
    LogPart Part = LogPart::Body;
};

struct LogMessage
{
    LogLevel Level = LogLevel::Info;
    std::string Category;
    std::string Text;
    std::chrono::system_clock::time_point Time;
    uint64 Sequence = 0;
    uint64 ThreadId = 0;
    LogSource Source;
    std::string Template;
    bool Nested = false;

    void AppendPrefix(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans = nullptr) const;
    void AppendPrefix(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans, LogLayout const& layout) const;
    void AppendLines(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans = nullptr) const;
    void AppendLines(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans, LogLayout const& layout) const;

    static void AppendCategoryColumn(std::string& out, std::string_view category, uint16 width, std::vector<LogSpan>* spans);

    static void AppendSanitized(std::string& out, std::string_view line);
    static uint64 CurrentOsThreadId() noexcept;
};

#endif
