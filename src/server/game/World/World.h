/*
 * Project Ambrose by Imjustchico
 * The game's update loop and the sessions it owns, of which a command may take a copy to act on, finding the wizards in the world by character id or by name and handing work for one to the world thread: one thread calls Update, which is the world thread from then on, and everything the world touches happens there, so a session's queued work is drained on it rather than on the network thread that read the message; each tick also moves the movement flush on, and when one is due every wizard's new move and movement state go to the others in its instance, and shows what each wizard said or played to the wizards that hear it; the tick carries every script's OnUpdate after the sessions have been drained, so a script sees the state the messages of that tick left behind; it also times each subsystem of the tick, and only while an operator has asked for a profile keeps a bounded Chrome trace of it; it also keeps a link-dead wizard's session until its grace period ends, and finds a session by character id, so a client that comes back can take its wizard's place again.
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
