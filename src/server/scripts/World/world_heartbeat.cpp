/*
 * Project Ambrose by Imjustchico
 * The first WorldScript, and the one that shows the hook framework is running: it counts the ticks and the time they covered, and says so every World.Heartbeat seconds, read each tick so a live change takes hold at once, so an operator reading the log can tell a server that is updating from one that is merely still open, and a later script can be added beside it without an edit to anything in the core.
 */

#include "Log.h"
#include "ScriptMgr.h"
#include "Settings.h"

namespace
{
    class WorldHeartbeat : public WorldScript
    {
    public:
        WorldHeartbeat() : WorldScript("world_heartbeat") {}

        void OnStartup() override
        {
            _ticks = 0;
            _covered = std::chrono::milliseconds::zero();
            _since = std::chrono::milliseconds::zero();
        }

        void OnUpdate(std::chrono::milliseconds diff) override
        {
            ++_ticks;
            _covered += diff;
            _since += diff;
            std::chrono::seconds const every(sSettings.Get<uint32>("World.Heartbeat"));
            if (every.count() == 0 || _since < every)
                return;
            LOG_INFO("server.world", "The world has ticked {} time(s) over {} ms", _ticks, _covered.count());
            _ticks = 0;
            _covered = std::chrono::milliseconds::zero();
            _since = std::chrono::milliseconds::zero();
        }

    private:
        uint64 _ticks = 0;
        std::chrono::milliseconds _covered{ 0 };
        std::chrono::milliseconds _since{ 0 };
    };
}

void AddSC_world_heartbeat()
{
    new WorldHeartbeat();
}
