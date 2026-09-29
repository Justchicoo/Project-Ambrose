/*
 * Project Ambrose by Imjustchico
 * The game's update loop and its sessions, which a command may copy to act on, finding players by name or character id, retaining link-dead wizards until their live grace period expires, and draining session work, lifecycle timers, visibility and movement on the single world thread; each tick also runs scripts and records subsystem timings and any requested bounded Chrome trace.
 */

#ifndef AMBROSE_WORLD_H
#define AMBROSE_WORLD_H

#include "MoveFlushClock.h"
#include "GameSessionWorld.h"
#include "Types.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

class GameSession;

struct WorldTickProfileEvent
{
    std::string Component;
    int64 StartMicroseconds = 0;
    int64 DurationMicroseconds = 0;
    uint64 Thread = 0;
};

struct WorldTickProfileSnapshot
{
    bool Active = false;
    bool Complete = false;
    bool Truncated = false;
    uint32 RequestedSeconds = 0;
    std::vector<WorldTickProfileEvent> Events;
};

class World : public GameSessionWorld
{
public:
    static World& Instance();

    World(World const&) = delete;
    World& operator=(World const&) = delete;

    void AddSession(std::shared_ptr<GameSession> session);
    void RemoveSession(GameSession const* session) override;
    std::size_t GetSessionCount() const;
    std::vector<std::shared_ptr<GameSession>> GetSessions() const;
    std::shared_ptr<GameSession> FindSessionByCharacterId(uint64 characterId, GameSession const* except = nullptr) const override;
    std::vector<std::shared_ptr<GameSession>> FindInWorld(std::string_view characterIdOrName) const;
    bool RunFor(std::shared_ptr<GameSession> const& session, std::function<void(GameSession&)> work, std::chrono::milliseconds timeout) const;
    void Clear();

    static constexpr std::chrono::seconds CommandTimeout{ 5 };

    void Update(std::chrono::milliseconds diff);

    bool StartTickProfile(uint32 seconds);
    WorldTickProfileSnapshot GetTickProfile(bool includeEvents = true);
    static std::string TickProfileTraceJson(WorldTickProfileSnapshot const& profile);

    std::thread::id GetWorldThreadId() const;
    bool IsWorldThread() const;
    uint64 GetTickCount() const;

private:
    World() = default;

    mutable std::mutex _mutex;
    std::vector<std::shared_ptr<GameSession>> _sessions;
    std::atomic<uint64> _ticks{ 0 };
    MoveFlushClock _moveFlush;
    mutable std::mutex _threadMutex;
    std::thread::id _worldThread;
    bool _worldThreadKnown = false;
    mutable std::mutex _profileMutex;
    std::atomic<bool> _profileActive{ false };
    std::chrono::steady_clock::time_point _profileStarted;
    std::chrono::steady_clock::time_point _profileEnds;
    uint32 _profileSeconds = 0;
    bool _profileComplete = false;
    bool _profileTruncated = false;
    std::vector<WorldTickProfileEvent> _profileEvents;

    void RecordProfileEvent(std::string_view component, std::chrono::steady_clock::time_point started,
        std::chrono::steady_clock::time_point ended);
};

#define sWorld World::Instance()

#endif
