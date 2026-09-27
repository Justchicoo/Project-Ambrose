/*
 * Project Ambrose by Imjustchico
 * The one stream layer every live feed is built on, for any record a hub carries: a session per subscriber with the feed's own filter and, when the feed's traits name one, the overflow policy of the stream it opens, the backlog or a resume after a sequence number on open, a dropped marker naming any range that left the backlog or a full queue, records pumped to the sink by one thread so a publisher never waits on a socket and pays one atomic flag per record to wake it, a session that closes once a stream that never drops has filled its queue and delivered what it held, the socket route that opens a session from a subscribe message, and the page of backlog a plain HTTP read gets. The sink hears the opening, each dropped range and an overflow through hooks whose defaults write the layer's own messages, so a feed with a wire of its own overrides only those. A feed supplies only its record, filter, request and how each is written and read.
 */

#ifndef AMBROSE_STREAMSERVICE_H
#define AMBROSE_STREAMSERVICE_H

#include "AdminServer.h"
#include "StreamHub.h"
#include "Types.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class StreamSink
{
public:
    virtual ~StreamSink() = default;

    virtual void Send(std::string text) = 0;
    virtual void Close(std::string reason) = 0;
    virtual void Opened(uint64 latest, uint64 oldest, std::size_t backlog);
    virtual void Dropped(uint64 from, uint64 to, uint64 count);
    virtual void Overflowed();
};

struct StreamGap
{
    uint64 From = 0;
    uint64 To = 0;
    uint64 Count = 0;
};

namespace StreamWire
{
    std::string EncodeDropped(uint64 from, uint64 to, uint64 count);
    std::string EncodeHello(uint64 latest, uint64 oldest, std::size_t backlog);
    std::string EncodeProblem(std::string const& code, std::string const& message);
    std::string BacklogPage(uint64 oldest, uint64 latest, uint64 after, std::vector<std::string> const& records);
}

template<class Traits>
class StreamSession
{
public:
    using Record = typename Traits::Record;
    using Subscription = StreamSubscription<Record, typename Traits::Filter>;

    StreamSession(std::shared_ptr<StreamSink> sink, std::shared_ptr<Subscription> subscription, std::optional<uint64> after)
        : _sink(std::move(sink)), _subscription(std::move(subscription)), _after(after), _lastSent(after.value_or(0))
    {
    }

    bool IsClosed() const
    {
        std::lock_guard const lock(_mutex);
        return _closed;
    }

    void Close()
    {
        std::shared_ptr<StreamSink> sink;
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

    std::size_t Drain(std::size_t max)
    {
        if (IsClosed())
            return 0;
        std::vector<std::shared_ptr<Record const>> records;
        StreamPopResult const popped = _subscription->Pop(records, max);
        if (records.empty() && popped.Dropped == 0 && !popped.Overflowed)
            return 0;
        std::vector<std::string> out;
        out.reserve(records.size());
        uint64 sent = 0;
        uint64 lastSent = 0;
        {
            std::lock_guard const lock(_mutex);
            lastSent = _lastSent;
        }
        std::optional<StreamGap> gap;
        if (popped.Dropped > 0)
        {
            uint64 const from = lastSent + 1;
            uint64 to = from + popped.Dropped - 1;
            if (!records.empty() && records.front()->Sequence > from)
                to = records.front()->Sequence - 1;
            gap = StreamGap{ from, to, popped.Dropped };
        }
        for (std::shared_ptr<Record const> const& record : records)
        {
            if (_after && record->Sequence <= *_after)
                continue;
            out.push_back(Traits::Encode(*record));
            lastSent = std::max(lastSent, record->Sequence);
            ++sent;
        }
        std::shared_ptr<StreamSink> sink;
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
            if (gap)
                sink->Dropped(gap->From, gap->To, gap->Count);
            for (std::string& message : out)
                sink->Send(std::move(message));
        }
        catch (std::exception const&)
        {
            Close();
            return records.size();
        }
        if (popped.Overflowed)
            Overflow(sink);
        return records.size();
    }

    void Begin(uint64 latest, uint64 oldest, std::size_t backlog, std::optional<StreamGap> const& gap)
    {
        std::shared_ptr<StreamSink> sink;
        {
            std::lock_guard const lock(_mutex);
            if (_closed)
                return;
            sink = _sink;
        }
        try
        {
            sink->Opened(latest, oldest, backlog);
            if (gap)
                sink->Dropped(gap->From, gap->To, gap->Count);
        }
        catch (std::exception const&)
        {
            Close();
        }
    }

    uint64 GetLastSent() const
    {
        std::lock_guard const lock(_mutex);
        return _lastSent;
    }

    uint64 GetSentCount() const
    {
        std::lock_guard const lock(_mutex);
        return _sent;
    }

    uint64 GetDroppedCount() const
    {
        std::lock_guard const lock(_mutex);
        return _dropped;
    }

private:
    void Overflow(std::shared_ptr<StreamSink> const& sink)
    {
        {
            std::lock_guard const lock(_mutex);
            if (_closed)
                return;
            _closed = true;
        }
        _subscription->Close();
        try
        {
            sink->Overflowed();
        }
        catch (std::exception const&)
        {
        }
    }

    std::shared_ptr<StreamSink> _sink;
    std::shared_ptr<Subscription> _subscription;
    std::optional<uint64> _after;
    mutable std::mutex _mutex;
    uint64 _lastSent = 0;
    uint64 _sent = 0;
    uint64 _dropped = 0;
    bool _closed = false;
};

template<class Traits>
class StreamService
{
public:
    using Record = typename Traits::Record;
    using Filter = typename Traits::Filter;
    using Request = typename Traits::Request;
    using Hub = StreamHub<Record, Filter>;
    using Session = StreamSession<Traits>;

    static constexpr std::size_t DrainBatch = 256;

    explicit StreamService(Hub& hub) : _hub(hub)
    {
    }

    ~StreamService()
    {
        Stop();
    }

    StreamService(StreamService const&) = delete;
    StreamService& operator=(StreamService const&) = delete;

    void Start()
    {
        std::lock_guard const lock(_mutex);
        if (_running)
            return;
        _running = true;
        _thread = std::thread([this] { Run(); });
    }

    void Stop()
    {
        std::vector<std::shared_ptr<Session>> sessions;
        {
            std::lock_guard const lock(_mutex);
            if (_running || _thread.joinable())
            {
                _running = false;
                _signalled.store(true);
            }
            sessions.swap(_sessions);
        }
        _wake.notify_all();
        if (_thread.joinable())
            _thread.join();
        for (std::shared_ptr<Session> const& session : sessions)
            session->Close();
    }

    bool IsRunning() const
    {
        std::lock_guard const lock(_mutex);
        return _running;
    }

    std::shared_ptr<Session> Open(std::shared_ptr<StreamSink> sink, Request const& request)
    {
        std::vector<std::shared_ptr<Record const>> const backlog = _hub.GetBacklog();
        uint64 const oldest = backlog.empty() ? 0 : backlog.front()->Sequence;
        uint64 const latest = backlog.empty() ? 0 : backlog.back()->Sequence;

        StreamOverflow overflow = StreamOverflow::DropOldest;
        if constexpr (requires { Traits::OverflowOf(request); })
            overflow = Traits::OverflowOf(request);
        std::shared_ptr<typename Session::Subscription> const subscription = _hub.Subscribe(Traits::FilterOf(request), request.QueueCapacity, [this] { Wake(); }, overflow);
        auto const session = std::make_shared<Session>(std::move(sink), subscription, request.After);

        std::optional<StreamGap> gap;
        if (request.After && oldest > 0 && *request.After + 1 < oldest)
            gap = StreamGap{ *request.After + 1, oldest - 1, oldest - *request.After - 1 };
        session->Begin(latest, oldest, backlog.size(), gap);

        {
            std::lock_guard const lock(_mutex);
            _sessions.push_back(session);
        }
        Wake();
        return session;
    }

    void Close(std::shared_ptr<Session> const& session)
    {
        {
            std::lock_guard const lock(_mutex);
            std::erase(_sessions, session);
        }
        session->Close();
    }

    std::size_t GetSessionCount() const
    {
        std::lock_guard const lock(_mutex);
        return _sessions.size();
    }

    std::size_t Pump(std::size_t max = DrainBatch)
    {
        std::vector<std::shared_ptr<Session>> sessions;
        {
            std::lock_guard const lock(_mutex);
            sessions = _sessions;
        }
        std::size_t drained = 0;
        bool prune = false;
        for (std::shared_ptr<Session> const& session : sessions)
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
            std::erase_if(_sessions, [](std::shared_ptr<Session> const& session) { return session->IsClosed(); });
        }
        return drained;
    }

    AdminSocketRoute MakeSocketRoute(std::string path, std::string permission = {})
    {
        class SocketSink final : public StreamSink
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
        route.Permission = std::move(permission);
        route.Received = [this](AdminSocket& socket, std::string const& message, bool binary)
        {
            {
                std::lock_guard const lock(_mutex);
                if (_bySocket.contains(&socket))
                    return;
            }
            std::string error;
            std::optional<Request> const request = binary ? std::nullopt : Traits::Parse(message, error);
            std::shared_ptr<AdminSocket> const kept = request ? socket.Keep() : nullptr;
            if (!request || !kept)
            {
                if (binary)
                    error = "the subscribe message must be text";
                else if (!request)
                    error = error.empty() ? "the subscribe message could not be read" : error;
                else
                    error = "this socket cannot be kept for streaming";
                socket.SendText(StreamWire::EncodeProblem("bad_request", error));
                socket.Close("bad subscribe request");
                return;
            }
            std::shared_ptr<Session> const session = Open(std::make_shared<SocketSink>(kept), *request);
            std::lock_guard const lock(_mutex);
            _bySocket[&socket] = session;
        };
        route.Closed = [this](AdminSocket& socket, std::string const&, uint16)
        {
            std::shared_ptr<Session> session;
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

    static std::string BacklogJson(Hub const& hub, uint64 after, std::size_t max)
    {
        std::vector<std::shared_ptr<Record const>> const backlog = hub.GetBacklog();
        uint64 const oldest = backlog.empty() ? 0 : backlog.front()->Sequence;
        uint64 const latest = backlog.empty() ? 0 : backlog.back()->Sequence;
        std::vector<std::string> records;
        for (std::shared_ptr<Record const> const& record : backlog)
        {
            if (record->Sequence <= after)
                continue;
            if (records.size() >= max)
                break;
            records.push_back(Traits::Encode(*record));
        }
        return StreamWire::BacklogPage(oldest, latest, after, records);
    }

private:
    void Run()
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

    void Wake()
    {
        _signalled.store(true);
        if (!_sleeping.exchange(false))
            return;
        std::lock_guard const lock(_mutex);
        _wake.notify_one();
    }

    Hub& _hub;
    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::vector<std::shared_ptr<Session>> _sessions;
    std::map<AdminSocket*, std::shared_ptr<Session>> _bySocket;
    std::thread _thread;
    bool _running = false;
    std::atomic<bool> _signalled{ false };
    std::atomic<bool> _sleeping{ false };
};

#endif
