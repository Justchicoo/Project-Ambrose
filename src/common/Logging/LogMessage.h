/*
 * Project Ambrose by Imjustchico
 * One log record plus prefix rendering, multi-line splitting, control-character escaping and UTF-8 repair.
 */

#ifndef AMBROSE_LOGMESSAGE_H
#define AMBROSE_LOGMESSAGE_H

#include "LogCommon.h"

#include <chrono>
#include <string>
#include <string_view>

struct LogMessage
{
    LogLevel Level = LogLevel::Info;
    std::string Category;
    std::string Text;
    std::chrono::system_clock::time_point Time;
    uint64 Sequence = 0;
    uint64 ThreadId = 0;
    bool Nested = false;

    void AppendPrefix(std::string& out, AppenderFlags flags, bool utc) const;
    void AppendLines(std::string& out, AppenderFlags flags, bool utc) const;

    static void AppendSanitized(std::string& out, std::string_view line);
    static uint64 CurrentOsThreadId() noexcept;
};

#endif
