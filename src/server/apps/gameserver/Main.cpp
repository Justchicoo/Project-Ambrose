/*
 * Project Ambrose by Imjustchico
 * Game server entry point: runs the shared app lifecycle with a world update tick whose interval follows World.UpdateInterval live.
 */

#include "ConfigMgr.h"
#include "Log.h"
#include "ServerApp.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    class GameServerApp : public ServerApp
    {
    public:
        GameServerApp() : ServerApp({ "gameserver", "gameserver.conf" }, sConfigMgr, sLog, std::cout, std::cerr)
        {
        }

    protected:
        std::chrono::milliseconds GetUpdateInterval() const override
        {
            uint32 const configured = sConfigMgr.GetOption<uint32>("World.UpdateInterval", 50, true);
            uint32 const interval = std::clamp<uint32>(configured, 1, 10000);
            if (configured != interval && configured != _reportedInterval.exchange(configured))
                AMBROSE_LOG(sLog, LogLevel::Warn, "server.gameserver", "World.UpdateInterval {} is outside 1..10000, using {} ms", configured, interval);
            return std::chrono::milliseconds(interval);
        }

        void OnUpdate(std::chrono::milliseconds) override
        {
        }

    private:
        mutable std::atomic<uint32> _reportedInterval{ 0 };
    };
}

int main(int argc, char** argv)
{
    GameServerApp app;
    return app.Run(std::vector<std::string>(argv, argv + argc));
}
