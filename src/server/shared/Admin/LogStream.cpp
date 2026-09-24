/*
 * Project Ambrose by Imjustchico
 * Opens a session by subscribing to the hub, marks the range a resume can no longer reach or a full queue threw away, drains every session on one pump thread into its sink, and encodes records, markers and the subscribe request as JSON with every secret redacted before it leaves. A record carries the place in the code it was written at and the template it was written from, added to the shape rather than changing it, so a reader that knows nothing of them reads it as before.
 */

#include "LogStream.h"
#include "LogRedaction.h"
#include "LogSource.h"
#include "LogTimestamp.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <exception>

LogStreamSession::LogStreamSession(std::shared_ptr<LogStreamSink> sink, std::shared_ptr<LogSubscription> subscription, std::optional<uint64> after)
    : _sink(std::move(sink)), _subscription(std::move(subscription)), _after(after), _lastSent(after.value_or(0))
{
}

bool LogStreamSession::IsClosed() const
{
    std::lock_guard const lock(_mutex);
    return _closed;
}

void LogStreamSession::Close()
{
    std::shared_ptr<LogStreamSink> sink;
    {
        std::lock_guard const lock(_mutex);
        if (_closed)
            return;
        _closed = true;
        sink = _sink;
    }
    _subscription->Close();
    try
    {
        sink->Close("closed");
    }
    catch (std::exception const&)
    {
    }
}

void LogStreamSession::SendPreamble(std::vector<std::string> const& messages)
{
    std::shared_ptr<LogStreamSink> sink;
    {
        std::lock_guard const lock(_mutex);
        if (_closed)
            return;
        sink = _sink;
    }
    try
    {
        for (std::string const& message : messages)
            sink->Send(message);
    }
    catch (std::exception const&)
    {
        Close();
    }
}

std::size_t LogStreamSession::Drain(std::size_t max)
{
    if (IsClosed())
        return 0;
    std::vector<std::shared_ptr<LogMessage const>> records;
    LogPopResult const popped = _subscription->Pop(records, max);
    if (records.empty() && popped.Dropped == 0)
        return 0;
    std::vector<std::string> out;
    out.reserve(records.size() + 1);
    uint64 sent = 0;
    uint64 lastSent = 0;
    {
        std::lock_guard const lock(_mutex);
        lastSent = _lastSent;
    }
    if (popped.Dropped > 0)
    {
        uint64 const from = lastSent + 1;
        uint64 to = from + popped.Dropped - 1;
        if (!records.empty() && records.front()->Sequence > from)
            to = records.front()->Sequence - 1;
        out.push_back(LogStreamService::EncodeDropped(from, to, popped.Dropped));
    }
    for (std::shared_ptr<LogMessage const> const& record : records)
    {
        if (_after && record->Sequence <= *_after)
            continue;
        out.push_back(LogStreamService::EncodeRecord(*record));
        lastSent = std::max(lastSent, record->Sequence);
        ++sent;
    }
    std::shared_ptr<LogStreamSink> sink;
    {
        std::lock_guard const lock(_mutex);
        if (_closed)
            return 0;
        _lastSent = lastSent;
        _sent += sent;
        _dropped += popped.Dropped;
        sink = _sink;
    }
    try
    {
        for (std::string const& message : out)
            sink->Send(message);
    }
    catch (std::exception const&)
    {
        Close();
    }
    return records.size();
}

uint64 LogStreamSession::GetLastSent() const
{
    std::lock_guard const lock(_mutex);
    return _lastSent;
}

uint64 LogStreamSession::GetSentCount() const
{
    std::lock_guard const lock(_mutex);
    return _sent;
}

uint64 LogStreamSession::GetDroppedCount() const
{
    std::lock_guard const lock(_mutex);
    return _dropped;
}

LogStreamService::LogStreamService(LogStreamHub& hub) : _hub(hub)
{
}

LogStreamService::~LogStreamService()
{
    Stop();
}

void LogStreamService::Start()
{
    std::lock_guard const lock(_mutex);
    if (_running)
        return;
    _running = true;
    _thread = std::thread([this] { Run(); });
}

void LogStreamService::Stop()
{
    std::vector<std::shared_ptr<LogStreamSession>> sessions;
    {
        std::lock_guard const lock(_mutex);
        if (!_running && !_thread.joinable())
        {
            sessions.swap(_sessions);
        }
        else
        {
            _running = false;
            _signalled.store(true);
            sessions.swap(_sessions);
        }
    }
    _wake.notify_all();
    if (_thread.joinable())
        _thread.join();
    for (std::shared_ptr<LogStreamSession> const& session : sessions)
        session->Close();
}

bool LogStreamService::IsRunning() const
{
    std::lock_guard const lock(_mutex);
    return _running;
}

std::shared_ptr<LogStreamSession> LogStreamService::Open(std::shared_ptr<LogStreamSink> sink, LogStreamRequest const& request)
{
    LogStreamFilter filter;
    filter.MinLevel = request.MinLevel;
    filter.Categories = request.Categories;

    std::vector<std::shared_ptr<LogMessage const>> const backlog = _hub.GetBacklog();
    uint64 const oldest = backlog.empty() ? 0 : backlog.front()->Sequence;
    uint64 const latest = backlog.empty() ? 0 : backlog.back()->Sequence;

    std::shared_ptr<LogStreamSession> session;
    std::shared_ptr<LogSubscription> const subscription = _hub.Subscribe(std::move(filter), request.QueueCapacity, [this] { Wake(); });
    session = std::make_shared<LogStreamSession>(std::move(sink), subscription, request.After);

    std::vector<std::string> preamble;
    preamble.push_back(EncodeHello(latest, oldest, backlog.size()));
    if (request.After && oldest > 0 && *request.After + 1 < oldest)
        preamble.push_back(EncodeDropped(*request.After + 1, oldest - 1, oldest - *request.After - 1));
    session->SendPreamble(preamble);

    {
        std::lock_guard const lock(_mutex);
        _sessions.push_back(session);
    }
    Wake();
    return session;
}

void LogStreamService::Close(std::shared_ptr<LogStreamSession> const& session)
{
    {
        std::lock_guard const lock(_mutex);
        std::erase(_sessions, session);
    }
    session->Close();
}

std::size_t LogStreamService::GetSessionCount() const
{
    std::lock_guard const lock(_mutex);
    return _sessions.size();
}

std::size_t LogStreamService::Pump(std::size_t max)
{
    std::vector<std::shared_ptr<LogStreamSession>> sessions;
    {
        std::lock_guard const lock(_mutex);
        sessions = _sessions;
    }
    std::size_t drained = 0;
    bool prune = false;
    for (std::shared_ptr<LogStreamSession> const& session : sessions)
    {
        if (session->IsClosed())
        {
            prune = true;
            continue;
        }
        drained += session->Drain(max);
    }
    if (prune)
    {
        std::lock_guard const lock(_mutex);
        std::erase_if(_sessions, [](std::shared_ptr<LogStreamSession> const& session) { return session->IsClosed(); });
    }
    return drained;
}

void LogStreamService::Run()
{
    while (true)
    {
        {
            std::unique_lock lock(_mutex);
            _sleeping.store(true);
            _wake.wait(lock, [this] { return _signalled.load() || !_running; });
            _sleeping.store(false);
            if (!_running)
                return;
            _signalled.store(false);
        }
        while (Pump() > 0)
        {
        }
    }
}

void LogStreamService::Wake()
{
    _signalled.store(true);
    if (!_sleeping.exchange(false))
        return;
    std::lock_guard const lock(_mutex);
    _wake.notify_one();
}

AdminSocketRoute LogStreamService::MakeSocketRoute(std::string path)
{
    class SocketSink final : public LogStreamSink
    {
    public:
        explicit SocketSink(std::shared_ptr<AdminSocket> socket) : _socket(std::move(socket))
        {
        }

        void Send(std::string text) override
        {
            _socket->SendText(std::move(text));
        }

        void Close(std::string reason) override
        {
            _socket->Close(std::move(reason));
        }

    private:
        std::shared_ptr<AdminSocket> _socket;
    };

    AdminSocketRoute route;
    route.Path = std::move(path);
    route.Received = [this](AdminSocket& socket, std::string const& message, bool binary)
    {
        {
            std::lock_guard const lock(_mutex);
            if (_bySocket.contains(&socket))
                return;
        }
        std::string error;
        std::optional<LogStreamRequest> const request = binary ? std::nullopt : ParseRequest(message, error);
        std::shared_ptr<AdminSocket> const kept = request ? socket.Keep() : nullptr;
        if (!request || !kept)
        {
            if (binary)
                error = "the subscribe message must be text";
            else if (!request)
                error = error.empty() ? "the subscribe message could not be read" : error;
            else
                error = "this socket cannot be kept for streaming";
            socket.SendText(EncodeProblem("bad_request", error));
            socket.Close("bad subscribe request");
            return;
        }
        std::shared_ptr<LogStreamSession> const session = Open(std::make_shared<SocketSink>(kept), *request);
        std::lock_guard const lock(_mutex);
        _bySocket[&socket] = session;
    };
    route.Closed = [this](AdminSocket& socket, std::string const&, uint16)
    {
        std::shared_ptr<LogStreamSession> session;
        {
            std::lock_guard const lock(_mutex);
            auto const found = _bySocket.find(&socket);
            if (found == _bySocket.end())
                return;
            session = found->second;
            _bySocket.erase(found);
        }
        Close(session);
    };
    return route;
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
    std::vector<std::shared_ptr<LogMessage const>> const backlog = hub.GetBacklog();
    uint64 const oldest = backlog.empty() ? 0 : backlog.front()->Sequence;
    uint64 const latest = backlog.empty() ? 0 : backlog.back()->Sequence;

    nlohmann::json records = nlohmann::json::array();
    for (std::shared_ptr<LogMessage const> const& record : backlog)
    {
        if (record->Sequence <= after)
            continue;
        if (records.size() >= max)
            break;
        records.push_back(RecordObject(*record));
    }

    nlohmann::json body;
    body["schema"] = 1;
    body["oldest"] = oldest;
    body["latest"] = latest;
    body["records"] = std::move(records);
    if (after != 0 && oldest > 0 && after + 1 < oldest)
        body["dropped"] = { { "from", after + 1 }, { "to", oldest - 1 }, { "count", oldest - after - 1 } };
    else
        body["dropped"] = nullptr;
    return body.dump();
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
    nlohmann::json body;
    body["type"] = "dropped";
    body["from"] = from;
    body["to"] = to;
    body["count"] = count;
    return body.dump();
}

std::string LogStreamService::EncodeHello(uint64 latest, uint64 oldest, std::size_t backlog)
{
    nlohmann::json body;
    body["type"] = "hello";
    body["latest"] = latest;
    body["oldest"] = oldest;
    body["backlog"] = backlog;
    return body.dump();
}

std::string LogStreamService::EncodeProblem(std::string const& code, std::string const& message)
{
    nlohmann::json body;
    body["type"] = "problem";
    body["code"] = code;
    body["message"] = message;
    return body.dump();
}
