/*
 * Project Ambrose by Imjustchico
 * The first call to Update decides which thread the world runs on and every later call is expected on it, so a test and a running server agree on what "the world thread" means; sessions are drained under a lock held only long enough to take a copy of the list, because a handler may add or remove a session while it runs, each session is then given the tick's time, which is how one that never attaches is closed, and a session that has closed leaves its zone instance and is dropped after its last queued work has run, on this thread, because the instance is the world thread's alone; then the wizards that left an instance this tick are taken away from the wizards still in it and those that arrived are shown to the wizards already there, and those wizards to them, and a wizard that jumped is shown entering its jumping state to the others in its instance, and to its own client when it did not ask to be left out; after the instances are looked after, each is brought in line with its zone's objects, and the wizards in an instance whose objects changed are told which left and which came. When a movement flush is due, every wizard in an instance is asked for what changed of its movement since the last one, and what it gives is sent to every other wizard in the same instance. A wizard is in the world from the moment its session is sent its object until it is kicked or its socket closes, and work for one from another thread is queued on its session and waited for, run at once when the caller is the world thread itself, which could never wait on its own tick.
 */

#include "World.h"
#include "GameSession.h"
#include "Log.h"
#include "MapMgr.h"
#include "MetricRegistry.h"
#include "PlayerMeetings.h"
#include "PlayerStates.h"
#include "Settings.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <algorithm>
#include <map>
#include <future>
#include <optional>
#include <string>
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
            presence.MapId = open ? session->GetMapId() : std::nullopt;
            presence.WorldGuid = session->GetWorldGuid();
            presence.Shown = open && session->IsShown();
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
    _moveFlush.Reset();
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

    std::vector<std::shared_ptr<GameSession>> sessions;
    {
        std::lock_guard const lock(_mutex);
        sessions = _sessions;
    }
    std::size_t const sessionCount = sessions.size();
    for (std::shared_ptr<GameSession> const& session : sessions)
    {
        session->DrainQueue();
        session->WorldUpdate(started);
    }
    for (std::shared_ptr<GameSession> const& session : sessions)
    {
        if (session->IsOpen())
            continue;
        session->LeaveWorld();
        RemoveSession(session.get());
    }
    MeetPlayers(sessions);
    RelayJumps(sessions);

    for (uint32 const taken : sMapMgr.Update())
        LOG_DEBUG("server.world", "Took down zone instance {}, empty for longer than its unload delay", taken);
    for (MapObjectChanges const& changes : sMapMgr.RefreshObjects())
        for (std::shared_ptr<GameSession> const& session : sessions)
            if (session->IsOpen() && session->GetMapId() == changes.DynamicZoneId)
                session->SendObjectChanges(changes);
    if (_moveFlush.Advance(diff, std::chrono::milliseconds(sSettings.Get<uint32>("Zone.MoveFlushInterval"))))
        FlushMovement(sessions, sSettings.Get<uint32>("Zone.MoveIdleIntervals"));

    sScriptMgr.OnWorldUpdate(diff);

    static Ambrose::Histogram& tickSeconds = sMetrics.HistogramFor("ambrose_world_tick_seconds", "How long a world tick took");
    static Ambrose::Counter& ticks = sMetrics.CounterFor("ambrose_world_ticks_total", "World ticks run");
    static Ambrose::Gauge& held = sMetrics.GaugeFor("ambrose_world_sessions", "Sessions the world is holding");
    tickSeconds.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    ticks.Add();
    held.Set(static_cast<int64>(sessionCount));
}
