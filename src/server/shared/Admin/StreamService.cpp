/*
 * Project Ambrose by Imjustchico
 * Writes the stream layer's own messages, the same for every feed: the hello that opens a session with the newest and oldest sequence kept, the dropped marker naming a missed range, the problem that refuses a subscribe message, and the page of backlog an HTTP read after a sequence number gets, which says what it could no longer show; a sink that does not speak for itself sends the hello and the dropped marker as those messages and closes when a stream that never drops overflows.
 */

#include "StreamService.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

void StreamSink::Opened(uint64 latest, uint64 oldest, std::size_t backlog)
{
    Send(StreamWire::EncodeHello(latest, oldest, backlog));
}

void StreamSink::Dropped(uint64 from, uint64 to, uint64 count)
{
    Send(StreamWire::EncodeDropped(from, to, count));
}

void StreamSink::Overflowed()
{
    Close("overflowed");
}

std::string StreamWire::EncodeDropped(uint64 from, uint64 to, uint64 count)
{
    nlohmann::json body;
    body["type"] = "dropped";
    body["from"] = from;
    body["to"] = to;
    body["count"] = count;
    return body.dump();
}

std::string StreamWire::EncodeHello(uint64 latest, uint64 oldest, std::size_t backlog)
{
    nlohmann::json body;
    body["type"] = "hello";
    body["latest"] = latest;
    body["oldest"] = oldest;
    body["backlog"] = backlog;
    return body.dump();
}

std::string StreamWire::EncodeProblem(std::string const& code, std::string const& message)
{
    nlohmann::json body;
    body["type"] = "problem";
    body["code"] = code;
    body["message"] = message;
    return body.dump();
}

std::string StreamWire::BacklogPage(uint64 oldest, uint64 latest, uint64 after, std::vector<std::string> const& records)
{
    std::string page = fmt::format(R"({{"schema":1,"oldest":{},"latest":{},"dropped":)", oldest, latest);
    if (after != 0 && oldest > 0 && after + 1 < oldest)
        page += fmt::format(R"({{"from":{},"to":{},"count":{}}})", after + 1, oldest - 1, oldest - after - 1);
    else
        page += "null";
    page += R"(,"records":[)";
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (index > 0)
            page += ',';
        page += records[index];
    }
    page += "]}";
    return page;
}
