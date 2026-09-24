/*
 * Project Ambrose by Imjustchico
 * The one stream layer every live feed is built on: a session per subscriber over the log hub with a level and category filter, the backlog or a resume after a sequence number on open, a dropped marker naming any range that left the backlog or a full queue, records pumped to the sink by one thread so logging never waits on a socket and pays one atomic flag per record to wake it, and the JSON each message takes.
 */

#ifndef AMBROSE_LOGSTREAM_H
#define AMBROSE_LOGSTREAM_H

#include "AdminServer.h"
#include "LogMessage.h"
#include "LogStreamHub.h"
#include "Types.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct LogStreamRequest
{
    LogLevel MinLevel = LogLevel::Trace;
    std::vector<std::string> Categories;
    std::optional<uint64> After;
    std::size_t QueueCapacity = LogStreamHub::DefaultSubscriberCapacity;
};

class LogStreamSink
{
public:
    virtual ~LogStreamSink() = default;

    virtual void Send(std::string text) = 0;
    virtual void Close(std::string reason) = 0;
};

class LogStreamSession
{
public:
    LogStreamSession(std::shared_ptr<LogStreamSink> sink, std::shared_ptr<LogSubscription> subscription, std::optional<uint64> after);

    bool IsClosed() const;
    void Close();
    std::size_t Drain(std::size_t max);
    void SendPreamble(std::vector<std::string> const& messages);

    uint64 GetLastSent() const;
    uint64 GetSentCount() const;
    uint64 GetDroppedCount() const;

private:
    std::shared_ptr<LogStreamSink> _sink;
    std::shared_ptr<LogSubscription> _subscription;
    std::optional<uint64> _after;
    mutable std::mutex _mutex;
    uint64 _lastSent = 0;
    uint64 _sent = 0;
    uint64 _dropped = 0;
    bool _closed = false;
};

class LogStreamService
{
public:
    static constexpr std::size_t DrainBatch = 256;

    explicit LogStreamService(LogStreamHub& hub);
    ~LogStreamService();

    LogStreamService(LogStreamService const&) = delete;
    LogStreamService& operator=(LogStreamService const&) = delete;

    void Start();
    void Stop();
    bool IsRunning() const;

    std::shared_ptr<LogStreamSession> Open(std::shared_ptr<LogStreamSink> sink, LogStreamRequest const& request);
    void Close(std::shared_ptr<LogStreamSession> const& session);
    std::size_t GetSessionCount() const;
    std::size_t Pump(std::size_t max = DrainBatch);
    AdminSocketRoute MakeSocketRoute(std::string path);

    static std::optional<LogStreamRequest> ParseRequest(std::string const& text, std::string& error);
    static std::string EncodeRecord(LogMessage const& record);
    static std::string BacklogJson(LogStreamHub const& hub, uint64 after, std::size_t max);
    static std::string EncodeDropped(uint64 from, uint64 to, uint64 count);
    static std::string EncodeHello(uint64 latest, uint64 oldest, std::size_t backlog);
    static std::string EncodeProblem(std::string const& code, std::string const& message);

private:
    void Run();
    void Wake();

    LogStreamHub& _hub;
    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::vector<std::shared_ptr<LogStreamSession>> _sessions;
    std::map<AdminSocket*, std::shared_ptr<LogStreamSession>> _bySocket;
    std::thread _thread;
    bool _running = false;
    std::atomic<bool> _signalled{ false };
    std::atomic<bool> _sleeping{ false };
};

#endif
