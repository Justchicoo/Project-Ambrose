/*
 * Project Ambrose by Imjustchico
 * The first call to Update decides which thread the world runs on and every later call is expected on it, so a test and a running server agree on what "the world thread" means, until Clear forgets it along with the sessions, since a later thread may be given the same id; sessions are drained under a lock held only long enough to take a copy of the list, because a handler may add or remove a session while it runs, each session is then given the tick's time, which is how one that never attaches is closed, and a session that has closed leaves its zone instance and is dropped after its last queued work has run, on this thread, because the instance is the world thread's alone; then the wizards that left an instance this tick are taken away from the wizards still in it and those that arrived are shown to the wizards already there, and those wizards to them, and a wizard that jumped is shown entering its jumping state to the others in its instance, and to its own client when it did not ask to be left out; then what each wizard said or played since the last tick is shown to every other wizard in its instance within Chat.SayRange of it, a line never to the speaker, whose client shows its own, and an emote to the speaker too when it did not ask to be left out, timed as the chat part of the tick; after the instances are looked after, each is brought in line with its zone's objects, and the wizards in an instance whose objects changed are told which left and which came. When a movement flush is due, every wizard in an instance is asked for what changed of its movement since the last one, and what it gives is sent to every other wizard in the same instance. A wizard is in the world from the moment its session is sent its object until it is kicked or its socket closes, and work for one from another thread is queued on its session and waited for, run at once when the caller is the world thread itself, which could never wait on its own tick; each tick times its subsystems, showing wizards to each other, their jumps and the movement flush counted together as movement, and only while an operator has asked for a profile records a bounded Chrome trace of them; a wizard whose client drops without logging out stays in its instance as link-dead, shown to the others standing still with MSG_ZOMBIE_PLAYER, until its session's link-dead time passes or the same character attaches again and takes its place.
 */

#include "World.h"
#include "ChatMgr.h"
#include "GameSession.h"
#include "Log.h"
#include "MapMgr.h"
#include "MetricRegistry.h"
#include "PlayerMeetings.h"
#include "PlayerStates.h"
#include "Settings.h"
#include "SpeechMessages.h"
#include "SpeechRelay.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    void MeetPlayers(std::vector<std::shared_ptr<GameSession>> const& sessions)
    {
        std::vector<PlayerPresence> players;
        players.reserve(sessions.size());
        bool changed = false;
        for (std::shared_ptr<GameSession> const& session : sessions)
        {
            bool const open = session->IsOpen();
            PlayerPresence presence;
            bool const present = open || session->IsLinkDead();
            presence.MapId = present ? session->GetMapId() : std::nullopt;
            presence.WorldGuid = session->GetWorldGuid();
            presence.Shown = present && session->IsShown();
            presence.Arrived = session->TakeArrival() && open;
            if (std::optional<WorldDeparture> const left = session->TakeDeparture())
            {
                presence.LeftMapId = left->MapId;
                presence.LeftGuid = left->WorldGuid;
            }
            changed = changed || presence.Arrived || presence.LeftMapId.has_value();
            players.push_back(presence);
        }
        if (!changed)
            return;
        PlayerMeetingPlan const plan = PlanPlayerMeetings(players);
        std::map<std::size_t, std::size_t> told;
        for (PlayerHiding const& hiding : plan.Hidings)
        {
            sessions[hiding.Viewer]->HidePlayer(players[hiding.Left].LeftGuid);
            ++told[hiding.Left];
        }
        for (auto const& [left, count] : told)
            LOG_DEBUG("server.world", "Session {}'s wizard {} was taken away from the {} wizard(s) still in instance {}", sessions[left]->GetSessionId(), players[left].LeftGuid,
                count, *players[left].LeftMapId);
        std::map<std::size_t, std::size_t> met;
        for (PlayerMeeting const& meeting : plan.Meetings)
        {
            GameSession& arrived = *sessions[meeting.Arrived];
            GameSession& other = *sessions[meeting.Other];
            if (meeting.ArrivedSees)
                arrived.ShowPlayer(other);
            if (meeting.OtherSees)
                other.ShowPlayer(arrived);
            ++met[meeting.Arrived];
        }
        for (auto const& [arrived, count] : met)
            LOG_DEBUG("server.world", "Session {}'s wizard {} and the {} wizard(s) already in instance {} were shown to each other", sessions[arrived]->GetSessionId(),
                sessions[arrived]->GetWorldGuid(), count, *players[arrived].MapId);
    }

    void RelayJumps(std::vector<std::shared_ptr<GameSession>> const& sessions)
    {
        for (std::shared_ptr<GameSession> const& jumper : sessions)
        {
            std::optional<uint8> const excludeOriginator = jumper->TakeJump();
            if (!excludeOriginator || !jumper->IsOpen() || !jumper->IsShown())
                continue;
            std::size_t told = 0;
            for (std::shared_ptr<GameSession> const& viewer : sessions)
            {
                if (!viewer->IsOpen() || viewer->GetMapId() != jumper->GetMapId() || (viewer == jumper && *excludeOriginator != 0))
                    continue;
                viewer->ShowStateOf(jumper->GetWorldGuid(), PlayerStates::Jumping);
                if (viewer != jumper)
                    ++told;
            }
            LOG_DEBUG("server.world", "Session {}'s wizard {} jumped{}, shown to {} other wizard(s)", jumper->GetSessionId(), jumper->GetWorldGuid(),
                *excludeOriginator != 0 ? "" : " and to itself", told);
        }
    }

    void RelayWizBangs(std::vector<std::shared_ptr<GameSession>> const& sessions)
    {
        for (std::shared_ptr<GameSession> const& sender : sessions)
        {
            std::optional<uint32> const wizBangId = sender->TakeWizBangChange();
            if (!wizBangId || !sender->IsOpen() || !sender->IsShown())
                continue;

            std::optional<uint32> const mapId = sender->GetMapId();
            std::size_t recipients = 0;
            for (std::shared_ptr<GameSession> const& viewer : sessions)
            {
                if (!viewer->IsOpen() || !viewer->IsShown() || viewer->GetMapId() != mapId)
                    continue;
                viewer->ShowWizBangOf(sender->GetWorldGuid(), *wizBangId);
                ++recipients;
            }

            LOG_DEBUG("server.world", "Session {} changed wizard {}'s wizbang to {} for {} wizard(s) in zone instance {}", sender->GetSessionId(),
                sender->GetWorldGuid(), *wizBangId, recipients, *mapId);
        }
    }

    std::string SpeechName(Speech const& speech)
    {
        switch (speech.Kind)
        {
            case SpeechKind::Say: return "a chat line";
            case SpeechKind::QuickChat: return fmt::format("quick chat phrase {}", speech.PhraseId);
            case SpeechKind::QuickChatExt: return "an extended quick chat phrase";
            case SpeechKind::Emote: return fmt::format("the emote {}", speech.Animation);
            case SpeechKind::CustomEmote: return fmt::format("the custom emote {}", speech.Animation);
        }
        return "something";
    }

    void RelaySpeech(std::vector<std::shared_ptr<GameSession>> const& sessions, float range)
    {
        std::vector<std::pair<std::size_t, std::vector<Speech>>> spoken;
        for (std::size_t index = 0; index < sessions.size(); ++index)
            if (std::vector<Speech> said = sessions[index]->TakeSpeech(); !said.empty())
                spoken.emplace_back(index, std::move(said));
        if (spoken.empty())
            return;
        std::vector<SpeechListener> listeners;
        listeners.reserve(sessions.size());
        for (std::shared_ptr<GameSession> const& session : sessions)
        {
            PlayerPosition const& at = session->GetMovement().GetPosition();
            listeners.push_back({ session->IsOpen(), session->GetMapId(), at.X, at.Y, at.Z });
        }
        for (auto const& [index, said] : spoken)
        {
            GameSession const& speaker = *sessions[index];
            if (!speaker.IsOpen() || !speaker.IsShown())
                continue;
            ChatSpeaker const who{ speaker.GetChatName(), speaker.GetWorldGuid(), speaker.GetChatFilter() };
            for (Speech const& speech : said)
            {
                std::vector<std::size_t> const hearers = PlanHearers(listeners, index, speech.SpeakerSees, range);
                for (std::size_t const hearer : hearers)
                    if (speech.Kind == SpeechKind::CustomEmote)
                        sessions[hearer]->HearCustomEmote(who, speech);
                    else
                        sessions[hearer]->HearSpeech(who, speech);
                std::size_t const others = hearers.size() - (speech.SpeakerSees && std::find(hearers.begin(), hearers.end(), index) != hearers.end() ? 1 : 0);
                LOG_DEBUG("server.world", "Session {}'s wizard {} sent {}, shown to {} other wizard(s){}", speaker.GetSessionId(), speaker.GetWorldGuid(), SpeechName(speech),
                    others, speech.SpeakerSees ? " and to itself" : "");
            }
        }
    }

    void NotifyLinkDead(std::vector<std::shared_ptr<GameSession>> const& sessions, GameSession& lost)
    {
        if (!lost.GetMapId())
            return;
        for (std::shared_ptr<GameSession> const& viewer : sessions)
        {
            if (!viewer->IsOpen() || viewer.get() == &lost || viewer->GetMapId() != lost.GetMapId())
                continue;
            MovementUpdate stopped;
            stopped.State = MovementRelay::Standing;
            viewer->ShowMovementOf(lost, stopped);
            viewer->ShowZombiePlayer(lost);
        }
    }

    void FlushMovement(std::vector<std::shared_ptr<GameSession>> const& sessions, uint32 idleFlushes)
    {
        std::map<uint32, std::vector<GameSession*>> instances;
        for (std::shared_ptr<GameSession> const& session : sessions)
            if (session->IsOpen())
                if (std::optional<uint32> const map = session->GetMapId())
                    instances[*map].push_back(session.get());
        for (auto const& [map, wizards] : instances)
            for (GameSession* mover : wizards)
            {
                MovementUpdate const update = mover->TakeMovementUpdate(idleFlushes);
                if (update.Empty())
                    continue;
                for (GameSession* viewer : wizards)
                    if (viewer != mover)
                        viewer->ShowMovementOf(*mover, update);
            }
    }
}

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

    std::array<ComponentMetrics, 10>& TickComponents()
    {
        static std::array<ComponentMetrics, 10> components = []
        {
            struct Definition
            {
                std::string_view Name;
                int64 BudgetNanoseconds;
                bool Available;
                std::string_view UnavailableReason;
            };
            constexpr std::array<Definition, 10> definitions{{
                { "network_drain", 5000000, true, "" },
                { "session_update", 10000000, true, "" },
                { "session_cleanup", 2000000, true, "" },
                { "zone_instances", 5000000, true, "" },
                { "movement", 5000000, true, "" },
                { "chat", 2000000, true, "" },
                { "scripting", 5000000, true, "" },
                { "world_overhead", 3000000, true, "" },
                { "database_waits", 0, false, "Database work runs on asynchronous workers outside the world tick" },
                { "combat", 0, false, "Combat is not integrated into the world tick yet" }
            }};
            std::array<ComponentMetrics, 10> registered{};
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
    session->_world = this;
    _sessions.push_back(std::move(session));
}

void World::RemoveSession(GameSession const* session)
{
    std::lock_guard const lock(_mutex);
    for (std::shared_ptr<GameSession> const& held : _sessions)
        if (held.get() == session)
            held->_world = nullptr;
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

std::shared_ptr<GameSession> World::FindSessionByCharacterId(uint64 characterId, GameSession const* except) const
{
    if (characterId == 0)
        return nullptr;
    std::lock_guard const lock(_mutex);
    auto const found = std::find_if(_sessions.begin(), _sessions.end(), [characterId, except](std::shared_ptr<GameSession> const& session)
    {
        return session.get() != except && !session->IsKicked() && session->GetCharacterId() == characterId && (session->IsOpen() || session->IsLinkDead());
    });
    return found == _sessions.end() ? nullptr : *found;
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
    for (std::shared_ptr<GameSession> const& session : _sessions)
        session->_world = nullptr;
    _sessions.clear();
    _moveFlush.Reset();
    std::lock_guard const threadLock(_threadMutex);
    _worldThread = std::thread::id();
    _worldThreadKnown = false;
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
    std::array<std::chrono::nanoseconds, 8> measured{};

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
        if (session->TakeLinkDeadStart())
            NotifyLinkDead(sessions, *session);
        auto const updateEnded = std::chrono::steady_clock::now();
        measured[1] += std::chrono::duration_cast<std::chrono::nanoseconds>(updateEnded - updateStarted);
        RecordProfileEvent("session_update", updateStarted, updateEnded);
    }

    auto const cleanupStarted = std::chrono::steady_clock::now();
    for (std::shared_ptr<GameSession> const& session : sessions)
    {
        if (session->IsOpen() || session->IsLinkDead())
            continue;
        session->LeaveWorld();
        RemoveSession(session.get());
    }
    auto const cleanupEnded = std::chrono::steady_clock::now();
    measured[2] = std::chrono::duration_cast<std::chrono::nanoseconds>(cleanupEnded - cleanupStarted);
    RecordProfileEvent("session_cleanup", cleanupStarted, cleanupEnded);

    auto const meetingStarted = std::chrono::steady_clock::now();
    MeetPlayers(sessions);
    RelayJumps(sessions);
    RelayWizBangs(sessions);
    auto const meetingEnded = std::chrono::steady_clock::now();
    measured[4] = std::chrono::duration_cast<std::chrono::nanoseconds>(meetingEnded - meetingStarted);
    RecordProfileEvent("movement", meetingStarted, meetingEnded);

    auto const chatStarted = std::chrono::steady_clock::now();
    RelaySpeech(sessions, sSettings.Get<float>("Chat.SayRange"));
    auto const chatEnded = std::chrono::steady_clock::now();
    measured[6] = std::chrono::duration_cast<std::chrono::nanoseconds>(chatEnded - chatStarted);
    RecordProfileEvent("chat", chatStarted, chatEnded);

    auto const zonesStarted = std::chrono::steady_clock::now();
    for (uint32 const taken : sMapMgr.Update())
        LOG_DEBUG("server.world", "Took down zone instance {}, empty for longer than its unload delay", taken);
    for (MapObjectChanges const& changes : sMapMgr.RefreshObjects())
        for (std::shared_ptr<GameSession> const& session : sessions)
            if (session->IsOpen() && session->GetMapId() == changes.DynamicZoneId)
                session->SendObjectChanges(changes);
    auto const zonesEnded = std::chrono::steady_clock::now();
    measured[3] = std::chrono::duration_cast<std::chrono::nanoseconds>(zonesEnded - zonesStarted);
    RecordProfileEvent("zone_instances", zonesStarted, zonesEnded);

    auto const flushStarted = std::chrono::steady_clock::now();
    if (_moveFlush.Advance(diff, std::chrono::milliseconds(sSettings.Get<uint32>("Zone.MoveFlushInterval"))))
        FlushMovement(sessions, sSettings.Get<uint32>("Zone.MoveIdleIntervals"));
    auto const flushEnded = std::chrono::steady_clock::now();
    measured[4] += std::chrono::duration_cast<std::chrono::nanoseconds>(flushEnded - flushStarted);
    RecordProfileEvent("movement", flushStarted, flushEnded);

    auto const scriptsStarted = std::chrono::steady_clock::now();
    sScriptMgr.OnWorldUpdate(diff);
    auto const scriptsEnded = std::chrono::steady_clock::now();
    measured[5] = std::chrono::duration_cast<std::chrono::nanoseconds>(scriptsEnded - scriptsStarted);
    RecordProfileEvent("scripting", scriptsStarted, scriptsEnded);

    static Ambrose::Histogram& tickSeconds = sMetrics.HistogramFor("ambrose_world_tick_seconds", "How long a world tick took");
    static Ambrose::Counter& ticks = sMetrics.CounterFor("ambrose_world_ticks_total", "World ticks run");
    static Ambrose::Gauge& held = sMetrics.GaugeFor("ambrose_world_sessions", "Sessions the world is holding");
    auto const tickEnded = std::chrono::steady_clock::now();
    std::chrono::nanoseconds const tickElapsed = tickEnded - started;
    std::chrono::nanoseconds measuredTotal{};
    for (std::size_t index = 0; index < measured.size() - 1; ++index)
        measuredTotal += measured[index];
    measured[7] = std::max(std::chrono::nanoseconds::zero(), tickElapsed - measuredTotal);
    RecordProfileEvent("world_overhead", scriptsEnded, tickEnded);
    RecordProfileEvent("world_tick", started, tickEnded);

    constexpr std::array<std::string_view, 8> names{
        "network_drain", "session_update", "session_cleanup", "zone_instances", "movement", "scripting", "chat", "world_overhead"
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
