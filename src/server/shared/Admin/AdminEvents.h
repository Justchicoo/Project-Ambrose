/*
 * Project Ambrose by Imjustchico
 * The admin API's event feed on the stream layer: an event names its kind, such as setting.changed or reload.result, what it is about and its data as JSON, a setting change always carrying its values masked when the setting is a secret, a subscriber picks kinds by dot prefix, the hub stamps each event's sequence number under its own lock so the backlog is always in sequence order, and /api/events serves the feed with the same resume, backlog and dropped markers as the live log.
 */

#ifndef AMBROSE_ADMINEVENTS_H
#define AMBROSE_ADMINEVENTS_H

#include "ReloadMgr.h"
#include "Settings.h"
#include "StreamHub.h"
#include "StreamService.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct AdminEvent
{
    uint64 Sequence = 0;
    std::string Kind;
    std::string Subject;
    std::chrono::system_clock::time_point Time;
    std::string Data = "{}";
};

struct AdminEventFilter
{
    std::vector<std::string> Kinds;

    bool Matches(AdminEvent const& event) const noexcept;
};

struct AdminEventRequest
{
    std::vector<std::string> Kinds;
    std::optional<uint64> After;
    std::size_t QueueCapacity = StreamHub<AdminEvent, AdminEventFilter>::DefaultSubscriberCapacity;
};

struct AdminEventTraits
{
    using Record = AdminEvent;
    using Filter = AdminEventFilter;
    using Request = AdminEventRequest;

    static AdminEventFilter FilterOf(AdminEventRequest const& request);
    static std::string Encode(AdminEvent const& event);
    static std::optional<AdminEventRequest> Parse(std::string const& text, std::string& error);
};

extern template class StreamHub<AdminEvent, AdminEventFilter>;
extern template class StreamSession<AdminEventTraits>;
extern template class StreamService<AdminEventTraits>;

class AdminEventHub : public StreamHub<AdminEvent, AdminEventFilter>
{
public:
    uint64 Announce(std::string kind, std::string subject, std::string data);
    uint64 GetLatestSequence() const;

private:
    mutable std::mutex _stampMutex;
    uint64 _sequence = 0;
};

class AdminEventService : public StreamService<AdminEventTraits>
{
public:
    explicit AdminEventService(AdminEventHub& hub);
};

namespace AdminEventData
{
    std::string SettingChanged(Settings const& settings, SettingChange const& change);
    std::string ReloadResult(ReloadOutcome const& outcome);
}

#endif
