/*
 * Project Ambrose by Imjustchico
 * The live log as a feed of the stream layer: a subscription filters by level and category, and each record is written as the JSON a log line takes; the sessions, backlog, resume, dropped markers and pump are the layer's own.
 */

#ifndef AMBROSE_LOGSTREAM_H
#define AMBROSE_LOGSTREAM_H

#include "LogMessage.h"
#include "LogStreamHub.h"
#include "StreamService.h"
#include "Types.h"

#include <optional>
#include <string>
#include <vector>

struct LogStreamRequest
{
    LogLevel MinLevel = LogLevel::Trace;
    std::vector<std::string> Categories;
    std::optional<uint64> After;
    std::size_t QueueCapacity = LogStreamHub::DefaultSubscriberCapacity;
};

struct LogStreamTraits
{
    using Record = LogMessage;
    using Filter = LogStreamFilter;
    using Request = LogStreamRequest;

    static LogStreamFilter FilterOf(LogStreamRequest const& request);
    static std::string Encode(LogMessage const& record);
    static std::optional<LogStreamRequest> Parse(std::string const& text, std::string& error);
};

using LogStreamSink = StreamSink;
using LogStreamSession = StreamSession<LogStreamTraits>;

extern template class StreamSession<LogStreamTraits>;
extern template class StreamService<LogStreamTraits>;

class LogStreamService : public StreamService<LogStreamTraits>
{
public:
    explicit LogStreamService(LogStreamHub& hub);

    static std::optional<LogStreamRequest> ParseRequest(std::string const& text, std::string& error);
    static std::string EncodeRecord(LogMessage const& record);
    static std::string BacklogJson(LogStreamHub const& hub, uint64 after, std::size_t max);
    static std::string EncodeDropped(uint64 from, uint64 to, uint64 count);
    static std::string EncodeHello(uint64 latest, uint64 oldest, std::size_t backlog);
    static std::string EncodeProblem(std::string const& code, std::string const& message);
};

#endif
