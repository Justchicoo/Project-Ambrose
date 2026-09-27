/*
 * Project Ambrose by Imjustchico
 * The first call to Update decides the world thread; each tick measures the session queue, session update, cleanup, zone instance and script call sites, publishes their latest values and budgets, and records bounded Chrome trace events only while an operator's on-demand capture is active.
 */

#include "World.h"
#include "GameSession.h"
#include "Log.h"
#include "MapMgr.h"
#include "MetricRegistry.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <future>
#include <optional>
#include <string>
#include <utility>

namespace
{
    constexpr std::size_t MaxProfileEvents = 50000;

    struct ComponentMetrics
    {
        std::string_view Name;
        int64 BudgetNanoseconds;
        bool Available;
        std::string_view UnavailableReason;
        Ambrose::Gauge* ElapsedNanoseconds;
        Ambrose::Gauge* Budget;
        Ambrose::Gauge* AvailableMetric;
        Ambrose::Gauge* OverBudget;
        Ambrose::Histogram* Samples;
    };

    std::array<ComponentMetrics, 9>& TickComponents()
    {
        static std::array<ComponentMetrics, 9> components = []
        {
            struct Definition
            {
                std::string_view Name;
                int64 BudgetNanoseconds;
                bool Available;
                std::string_view UnavailableReason;
            };
            constexpr std::array<Definition, 9> definitions{{
                { "network_drain", 5000000, true, "" },
                { "session_update", 10000000, true, "" },
                { "session_cleanup", 2000000, true, "" },
                { "zone_instances", 5000000, true, "" },
                { "scripting", 5000000, true, "" },
                { "world_overhead", 3000000, true, "" },
                { "movement", 0, false, "Queued movement is not separately timed outside handler drain" },
                { "database_waits", 0, false, "Database work runs on asynchronous workers outside the world tick" },
                { "combat", 0, false, "Combat is not integrated into the world tick yet" }
            }};
            std::array<ComponentMetrics, 9> registered{};
            for (std::size_t index = 0; index < definitions.size(); ++index)
            {
                Definition const& definition = definitions[index];
                Ambrose::MetricLabels const labels{ { "component", std::string(definition.Name) } };
                Ambrose::MetricLabels const availabilityLabels{
                    { "component", std::string(definition.Name) },
                    { "reason", std::string(definition.UnavailableReason) }
                };
                registered[index] = {
                    definition.Name,
                    definition.BudgetNanoseconds,
                    definition.Available,
                    definition.UnavailableReason,
                    &sMetrics.GaugeFor("ambrose_world_tick_subsystem_nanoseconds", "Latest world tick time by subsystem", labels),
                    &sMetrics.GaugeFor("ambrose_world_tick_subsystem_budget_nanoseconds", "World tick time budget by subsystem", labels),
                    &sMetrics.GaugeFor("ambrose_world_tick_subsystem_available", "Whether world tick timing is available for this subsystem", availabilityLabels),
                    &sMetrics.GaugeFor("ambrose_world_tick_subsystem_over_budget", "Whether the latest world tick exceeded this subsystem budget", labels),
                    &sMetrics.HistogramFor("ambrose_world_tick_subsystem_seconds", "World tick time by subsystem in seconds", labels)
                };
                registered[index].Budget->Set(definition.BudgetNanoseconds);
                registered[index].AvailableMetric->Set(definition.Available ? 1 : 0);
            }
            return registered;
        }();
        return components;
    }

    void PublishComponent(std::string_view name, std::chrono::nanoseconds elapsed)
    {
        auto const found = std::find_if(TickComponents().begin(), TickComponents().end(),
            [name](ComponentMetrics const& component) { return component.Name == name; });
        if (found == TickComponents().end())
            return;
        int64 const nanoseconds = std::max<int64>(0, elapsed.count());
        found->ElapsedNanoseconds->Set(nanoseconds);
        found->OverBudget->Set(found->Available && nanoseconds > found->BudgetNanoseconds ? 1 : 0);
        if (found->Available)
            found->Samples->Observe(std::chrono::duration<double>(elapsed).count());
    }
}

World& World::Instance()
{
    static World instance;
    return instance;
}

void World::AddSession(std::shared_ptr<GameSession> session)
{
    if (!session)
        return;
    std::lock_guard const lock(_mutex);
    _sessions.push_back(std::move(session));
}

void World::RemoveSession(GameSession const* session)
{
    std::lock_guard const lock(_mutex);
    std::erase_if(_sessions, [session](std::shared_ptr<GameSession> const& held) { return held.get() == session; });
}

std::size_t World::GetSessionCount() const
{
    std::lock_guard const lock(_mutex);
    return _sessions.size();
}

std::vector<std::shared_ptr<GameSession>> World::GetSessions() const
{
    std::lock_guard const lock(_mutex);
    return _sessions;
}

std::vector<std::shared_ptr<GameSession>> World::FindInWorld(std::string_view characterIdOrName) const
{
    std::optional<uint64> const id = Ambrose::StringTo<uint64>(characterIdOrName, 10);
    std::string const name = Ambrose::ToLower(characterIdOrName);
    std::vector<std::shared_ptr<GameSession>> found;
    for (std::shared_ptr<GameSession> const& session : GetSessions())
    {
        SessionStatus const status = session->GetStatus();
        if (!session->IsOpen() || session->IsKicked() || (status != SessionStatus::LoggedIn && status != SessionStatus::InWorld))
            continue;
        std::string const shown = session->GetCharacterName();
        if ((id && session->GetCharacterId() == *id) || (!shown.empty() && Ambrose::ToLower(shown) == name))
            found.push_back(session);
    }
    return found;
}

bool World::RunFor(std::shared_ptr<GameSession> const& session, std::function<void(GameSession&)> work, std::chrono::milliseconds timeout) const
{
    if (!session || !work)
        return false;
    if (IsWorldThread())
    {
        work(*session);
        return true;
    }
    auto const done = std::make_shared<std::promise<void>>();
    std::future<void> finished = done->get_future();
    GameSession* const target = session.get();
    if (!session->QueueInbound([target, done, work = std::move(work)]
        {
            work(*target);
            done->set_value();
        }))
        return false;
    if (finished.wait_for(timeout) != std::future_status::ready)
        return false;
    try
    {
        finished.get();
    }
    catch (std::future_error const&)
    {
        return false;
    }
    return true;
}

void World::Clear()
{
    std::lock_guard const lock(_mutex);
    _sessions.clear();
}

std::thread::id World::GetWorldThreadId() const
{
    std::lock_guard const lock(_threadMutex);
    return _worldThread;
}

bool World::IsWorldThread() const
{
    std::lock_guard const lock(_threadMutex);
    return _worldThreadKnown && _worldThread == std::this_thread::get_id();
}

uint64 World::GetTickCount() const
{
    return _ticks.load(std::memory_order_relaxed);
}

void World::Update(std::chrono::milliseconds diff)
{
    {
        std::lock_guard const lock(_threadMutex);
        _worldThread = std::this_thread::get_id();
        _worldThreadKnown = true;
    }
    _ticks.fetch_add(1, std::memory_order_relaxed);
    auto const started = std::chrono::steady_clock::now();
    std::array<std::chrono::nanoseconds, 6> measured{};

    std::vector<std::shared_ptr<GameSession>> sessions;
    {
        std::lock_guard const lock(_mutex);
        sessions = _sessions;
    }
    std::size_t const sessionCount = sessions.size();
    for (std::shared_ptr<GameSession> const& session : sessions)
    {
        auto const queueStarted = std::chrono::steady_clock::now();
        session->DrainQueue();
        auto const queueEnded = std::chrono::steady_clock::now();
        measured[0] += std::chrono::duration_cast<std::chrono::nanoseconds>(queueEnded - queueStarted);
        RecordProfileEvent("network_drain", queueStarted, queueEnded);

        auto const updateStarted = std::chrono::steady_clock::now();
        session->WorldUpdate(started);
        auto const updateEnded = std::chrono::steady_clock::now();
        measured[1] += std::chrono::duration_cast<std::chrono::nanoseconds>(updateEnded - updateStarted);
        RecordProfileEvent("session_update", updateStarted, updateEnded);
    }

    auto const cleanupStarted = std::chrono::steady_clock::now();
    for (std::shared_ptr<GameSession> const& session : sessions)
    {
        if (session->IsOpen())
            continue;
        session->LeaveWorld();
        RemoveSession(session.get());
    }
    auto const cleanupEnded = std::chrono::steady_clock::now();
    measured[2] = std::chrono::duration_cast<std::chrono::nanoseconds>(cleanupEnded - cleanupStarted);
    RecordProfileEvent("session_cleanup", cleanupStarted, cleanupEnded);

    auto const zonesStarted = std::chrono::steady_clock::now();
    for (uint32 const taken : sMapMgr.Update())
        LOG_DEBUG("server.world", "Took down zone instance {}, empty for longer than its unload delay", taken);
    auto const zonesEnded = std::chrono::steady_clock::now();
    measured[3] = std::chrono::duration_cast<std::chrono::nanoseconds>(zonesEnded - zonesStarted);
    RecordProfileEvent("zone_instances", zonesStarted, zonesEnded);

    auto const scriptsStarted = std::chrono::steady_clock::now();
    sScriptMgr.OnWorldUpdate(diff);
    auto const scriptsEnded = std::chrono::steady_clock::now();
    measured[4] = std::chrono::duration_cast<std::chrono::nanoseconds>(scriptsEnded - scriptsStarted);
    RecordProfileEvent("scripting", scriptsStarted, scriptsEnded);

    static Ambrose::Histogram& tickSeconds = sMetrics.HistogramFor("ambrose_world_tick_seconds", "How long a world tick took");
    static Ambrose::Counter& ticks = sMetrics.CounterFor("ambrose_world_ticks_total", "World ticks run");
    static Ambrose::Gauge& held = sMetrics.GaugeFor("ambrose_world_sessions", "Sessions the world is holding");
    auto const tickEnded = std::chrono::steady_clock::now();
    std::chrono::nanoseconds const tickElapsed = tickEnded - started;
    std::chrono::nanoseconds measuredTotal{};
    for (std::size_t index = 0; index < measured.size() - 1; ++index)
        measuredTotal += measured[index];
    measured[5] = std::max(std::chrono::nanoseconds::zero(), tickElapsed - measuredTotal);
    RecordProfileEvent("world_overhead", scriptsEnded, tickEnded);
    RecordProfileEvent("world_tick", started, tickEnded);

    constexpr std::array<std::string_view, 6> names{
        "network_drain", "session_update", "session_cleanup", "zone_instances", "scripting", "world_overhead"
    };
    for (std::size_t index = 0; index < names.size(); ++index)
        PublishComponent(names[index], measured[index]);
    tickSeconds.Observe(std::chrono::duration<double>(tickElapsed).count());
    ticks.Add();
    held.Set(static_cast<int64>(sessionCount));
}

bool World::StartTickProfile(uint32 seconds)
{
    if (seconds == 0 || seconds > 30)
        return false;
    std::lock_guard const lock(_profileMutex);
    if (_profileActive.load(std::memory_order_relaxed))
        return false;
    _profileStarted = std::chrono::steady_clock::now();
    _profileEnds = _profileStarted + std::chrono::seconds(seconds);
    _profileSeconds = seconds;
    _profileComplete = false;
    _profileTruncated = false;
    _profileEvents.clear();
    _profileEvents.reserve(MaxProfileEvents);
    _profileActive.store(true, std::memory_order_relaxed);
    return true;
}

WorldTickProfileSnapshot World::GetTickProfile(bool includeEvents)
{
    std::lock_guard const lock(_profileMutex);
    if (_profileActive.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() >= _profileEnds)
    {
        _profileActive.store(false, std::memory_order_relaxed);
        _profileComplete = true;
    }
    WorldTickProfileSnapshot snapshot;
    snapshot.Active = _profileActive.load(std::memory_order_relaxed);
    snapshot.Complete = _profileComplete;
    snapshot.Truncated = _profileTruncated;
    snapshot.RequestedSeconds = _profileSeconds;
    if (includeEvents)
        snapshot.Events = _profileEvents;
    return snapshot;
}

std::string World::TickProfileTraceJson(WorldTickProfileSnapshot const& profile)
{
    std::string json = R"({"traceEvents":[)";
    bool first = true;
    for (WorldTickProfileEvent const& event : profile.Events)
    {
        if (!first)
            json += ',';
        first = false;
        json += fmt::format(R"({{"name":"{}","cat":"world_tick","ph":"X","ts":{},"dur":{},"pid":1,"tid":{}}})",
            event.Component, event.StartMicroseconds, event.DurationMicroseconds, event.Thread);
    }
    json += R"(],"displayTimeUnit":"ms","metadata":{"requested_seconds":)";
    json += fmt::format(R"({},"truncated":)", profile.RequestedSeconds);
    json += profile.Truncated ? "true" : "false";
    json += "}}";
    return json;
}

void World::RecordProfileEvent(std::string_view component, std::chrono::steady_clock::time_point started,
    std::chrono::steady_clock::time_point ended)
{
    if (!_profileActive.load(std::memory_order_relaxed))
        return;
    std::lock_guard const lock(_profileMutex);
    if (!_profileActive.load(std::memory_order_relaxed))
        return;

    if (ended >= _profileEnds)
    {
        _profileActive.store(false, std::memory_order_relaxed);
        _profileComplete = true;
    }
    auto const clippedStart = std::max(started, _profileStarted);
    auto const clippedEnd = std::min(ended, _profileEnds);
    if (clippedEnd <= clippedStart)
        return;
    if (_profileEvents.size() == MaxProfileEvents)
    {
        _profileActive.store(false, std::memory_order_relaxed);
        _profileComplete = true;
        _profileTruncated = true;
        return;
    }

    _profileEvents.push_back({
        std::string(component),
        std::chrono::duration_cast<std::chrono::microseconds>(clippedStart - _profileStarted).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(clippedEnd - clippedStart).count(),
        1
    });
}
