/*
 * Project Ambrose by Imjustchico
 * The live log's subscriber: the level and category filter a subscription holds, and the log's instance of the stream layer's bounded drop-oldest subscriber ring.
 */

#ifndef AMBROSE_LOGSUBSCRIPTION_H
#define AMBROSE_LOGSUBSCRIPTION_H

#include "LogMessage.h"
#include "StreamSubscription.h"

#include <string>
#include <string_view>
#include <vector>

struct LogStreamFilter
{
    LogLevel MinLevel = LogLevel::Trace;
    std::vector<std::string> Categories;

    bool Matches(LogMessage const& message) const noexcept
    {
        if (!IsLevelEnabled(MinLevel, message.Level))
            return false;
        return Categories.empty() || MatchesCategory(message.Category);
    }

    bool MatchesCategory(std::string_view category) const noexcept;
};

using LogPopResult = StreamPopResult;
using LogSubscription = StreamSubscription<LogMessage, LogStreamFilter>;

extern template class StreamSubscription<LogMessage, LogStreamFilter>;

#endif
