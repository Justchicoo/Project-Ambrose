/*
 * Project Ambrose by Imjustchico
 * The panel's event socket as feeds of the one stream layer rather than a second copy of it: a record carries its type, the app it is about or none for the whole panel, its time, its data already written as JSON and the key a stream that keeps only the newest record per key replaces it by; the hub stamps each record's sequence under its own lock so the backlog is always in order, a subscription follows one app or the whole panel within the apps its caller may see, and each record is written as the socket's own envelope. One hub and one pump serve each stream the catalog says this build serves, with the stream's overflow policy on every session it opens, a message the catalog does not mark as sent on a stream is refused rather than published, so nothing reaches a page that has no handler for it, and a random instance id names this run so a page drops the sequence numbers of an earlier one.
 */

#ifndef AMBROSE_PANELEVENTSTREAMS_H
#define AMBROSE_PANELEVENTSTREAMS_H

#include "StreamHub.h"
#include "StreamService.h"
#include "Types.h"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct PanelEvent
{
    uint64 Sequence = 0;
    std::string Type = {};
    std::string App = {};
    int64 TimeMs = 0;
    std::string Data = "{}";
    std::string Key = {};
};

struct PanelEventFilter
{
    std::optional<std::string> App = {};
    std::optional<std::set<std::string, std::less<>>> Visible = {};

    bool Matches(PanelEvent const& event) const noexcept;
};

struct PanelEventRequest
{
    std::string Stream = {};
    std::optional<std::string> App = {};
    std::optional<std::set<std::string, std::less<>>> Visible = {};
    std::optional<uint64> After = {};
    std::size_t QueueCapacity = StreamHub<PanelEvent, PanelEventFilter>::DefaultSubscriberCapacity;
    StreamOverflow Overflow = StreamOverflow::DropOldest;
};

struct PanelEventTraits
{
    using Record = PanelEvent;
    using Filter = PanelEventFilter;
    using Request = PanelEventRequest;

    static PanelEventFilter FilterOf(PanelEventRequest const& request);
    static StreamOverflow OverflowOf(PanelEventRequest const& request);
    static std::string Encode(PanelEvent const& event);
    static std::optional<PanelEventRequest> Parse(std::string const& text, std::string& error);
};

extern template class StreamHub<PanelEvent, PanelEventFilter>;
extern template class StreamSession<PanelEventTraits>;
extern template class StreamService<PanelEventTraits>;

using PanelEventSession = StreamSession<PanelEventTraits>;

class PanelEventHub : public StreamHub<PanelEvent, PanelEventFilter>
{
public:
    uint64 Announce(std::string type, std::string app, std::string data, std::string key = {});
    uint64 GetLatestSequence() const;

private:
    mutable std::mutex _stampMutex;
    uint64 _sequence = 0;
};

class PanelEventService : public StreamService<PanelEventTraits>
{
public:
    explicit PanelEventService(PanelEventHub& hub);
};

class PanelEventStreams
{
public:
    PanelEventStreams();

    PanelEventStreams(PanelEventStreams const&) = delete;
    PanelEventStreams& operator=(PanelEventStreams const&) = delete;

    void Start();
    void Stop();

    bool Serves(std::string_view stream) const;
    std::vector<std::string> Served() const;
    std::string const& Instance() const noexcept { return _instance; }
    PanelEventHub* Hub(std::string_view stream);
    PanelEventService* Service(std::string_view stream);

    uint64 Publish(std::string_view stream, std::string type, std::string app, std::string data, std::string key = {});
    std::shared_ptr<PanelEventSession> Open(std::shared_ptr<StreamSink> sink, PanelEventRequest request);
    void Close(std::string_view stream, std::shared_ptr<PanelEventSession> const& session);

private:
    struct Feed
    {
        std::unique_ptr<PanelEventHub> Records = {};
        std::unique_ptr<PanelEventService> Pump = {};
        StreamOverflow Overflow = StreamOverflow::DropOldest;
    };

    std::map<std::string, Feed, std::less<>> _feeds;
    std::string _instance;
};

#endif
