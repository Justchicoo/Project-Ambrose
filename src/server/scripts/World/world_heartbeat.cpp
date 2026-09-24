/*
 * Project Ambrose by Imjustchico
 * The first WorldScript, and the one that shows the hook framework is running: it counts the ticks and the time they covered, and says so once a minute at World.Heartbeat seconds, so an operator reading the log can tell a server that is updating from one that is merely still open, and a later script can be added beside it without an edit to anything in the core.
 */

#include "ConfigMgr.h"
#include "Log.h"
#include "ScriptMgr.h"

#include <algorithm>

namespace
{
    class WorldHeartbeat : public WorldScript
    {
    public:
        WorldHeartbeat() : WorldScript("world_heartbeat") {}

        void OnConfigLoad(bool) override
        {
            uint32 const seconds = sConfigMgr.GetOption<uint32>("World.Heartbeat", 60, true);
            _every = std::chrono::seconds(std::clamp<uint32>(seconds, 0, 86400));
        }

        void OnStartup() override
        {
            OnConfigLoad(false);
            _ticks = 0;
            _covered = std::chrono::milliseconds::zero();
            _since = std::chrono::milliseconds::zero();
        }

        void OnUpdate(std::chrono::milliseconds diff) override
        {
            ++_ticks;
            _covered += diff;
            _since += diff;
            if (_every.count() == 0 || _since < _every)
                return;
            LOG_INFO("server.world", "The world has ticked {} time(s) over {} ms", _ticks, _covered.count());
            _ticks = 0;
            _covered = std::chrono::milliseconds::zero();
            _since = std::chrono::milliseconds::zero();
        }

    private:
        std::chrono::seconds _every{ 60 };
        uint64 _ticks = 0;
        std::chrono::milliseconds _covered{ 0 };
        std::chrono::milliseconds _since{ 0 };
    };
}

void AddSC_world_heartbeat()
{
    new WorldHeartbeat();
}
