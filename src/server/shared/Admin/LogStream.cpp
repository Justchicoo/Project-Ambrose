/*
 * Project Ambrose by Imjustchico
 * Holds the one compiled copy of the stream layer over live log records, reads a log subscribe request, and writes each record as JSON with every secret redacted before it leaves. A record carries the place in the code it was written at and the template it was written from, added to the shape rather than changing it, so a reader that knows nothing of them reads it as before.
 */

#include "LogStream.h"
#include "LogRedaction.h"
#include "LogSource.h"
#include "LogTimestamp.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <chrono>

template class StreamSession<LogStreamTraits>;
template class StreamService<LogStreamTraits>;

LogStreamService::LogStreamService(LogStreamHub& hub) : StreamService<LogStreamTraits>(hub)
{
}

LogStreamFilter LogStreamTraits::FilterOf(LogStreamRequest const& request)
{
    LogStreamFilter filter;
    filter.MinLevel = request.MinLevel;
    filter.Categories = request.Categories;
    return filter;
}

std::optional<LogStreamRequest> LogStreamTraits::Parse(std::string const& text, std::string& error)
{
    return LogStreamService::ParseRequest(text, error);
}

std::string LogStreamTraits::Encode(LogMessage const& record)
{
    return LogStreamService::EncodeRecord(record);
}

std::optional<LogStreamRequest> LogStreamService::ParseRequest(std::string const& text, std::string& error)
{
    nlohmann::json document = nlohmann::json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        error = "the subscribe message must be a JSON object";
        return std::nullopt;
    }
    LogStreamRequest request;
    if (document.contains("level"))
    {
        if (!document["level"].is_string())
        {
            error = "level must be a level name";
            return std::nullopt;
        }
        std::optional<LogLevel> const level = Ambrose::Logging::ParseLogLevel(document["level"].get<std::string>());
        if (!level)
        {
            error = "level must be one of trace, debug, info, warn, error or fatal";
            return std::nullopt;
        }
        request.MinLevel = *level;
    }
    if (document.contains("categories"))
    {
        if (!document["categories"].is_array())
        {
            error = "categories must be a list of category names";
            return std::nullopt;
        }
        for (nlohmann::json const& entry : document["categories"])
        {
            if (!entry.is_string())
            {
                error = "categories must be a list of category names";
                return std::nullopt;
            }
            request.Categories.push_back(entry.get<std::string>());
        }
    }
    if (document.contains("after"))
    {
        if (!document["after"].is_number_unsigned())
        {
            error = "after must be a sequence number";
            return std::nullopt;
        }
        request.After = document["after"].get<uint64>();
    }
    return request;
}

namespace
{
    nlohmann::json RecordObject(LogMessage const& record);
}

std::string LogStreamService::EncodeRecord(LogMessage const& record)
{
    return RecordObject(record).dump();
}

std::string LogStreamService::BacklogJson(LogStreamHub const& hub, uint64 after, std::size_t max)
{
    return StreamService<LogStreamTraits>::BacklogJson(hub, after, max);
}

namespace
{
nlohmann::json RecordObject(LogMessage const& record)
{
    nlohmann::json body;
    body["type"] = "record";
    body["sequence"] = record.Sequence;
    body["time"] = std::string(LogTimestamp::FormatPrefix(record.Time, true));
    body["epoch_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(record.Time.time_since_epoch()).count();
    body["level"] = Ambrose::ToLower(std::string(Ambrose::Logging::GetLogLevelName(record.Level)));
    body["category"] = record.Category;
    body["message"] = LogRedaction::Redact(record.Text);
    if (record.Source.Known())
        body["source"] = { { "file", LogSourcePath::Portable(record.Source.File) }, { "line", record.Source.Line },
            { "function", std::string(record.Source.Function) } };
    else
        body["source"] = nullptr;
    body["template"] = record.Template;
    return body;
}
}

std::string LogStreamService::EncodeDropped(uint64 from, uint64 to, uint64 count)
{
    return StreamWire::EncodeDropped(from, to, count);
}

std::string LogStreamService::EncodeHello(uint64 latest, uint64 oldest, std::size_t backlog)
{
    return StreamWire::EncodeHello(latest, oldest, backlog);
}

std::string LogStreamService::EncodeProblem(std::string const& code, std::string const& message)
{
    return StreamWire::EncodeProblem(code, message);
}
