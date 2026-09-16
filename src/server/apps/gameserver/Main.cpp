/*
 * Project Ambrose by Imjustchico
 * Game server entry point: loads the type dump, brings the login, characters and world databases current and opens them, then runs the world update tick whose interval follows World.UpdateInterval live.
 */

#include "AppenderDB.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Log.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "ServerApp.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
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
        bool OnStart() override
        {
            std::vector<std::string> limitProblems;
            SerializerLimits::Apply(SerializerLimits::Load(Config(), &limitProblems));
            for (std::string const& problem : limitProblems)
                LOG_WARN("server.gameserver", "{}", problem);

            std::string const typeDump = Config().GetOption<std::string>("TypeDumpPath", "", true);
            if (typeDump.empty())
                LOG_WARN("server.gameserver", "TypeDumpPath is not set, so ObjectProperty data cannot be read or written");
            else if (!sTypeRegistry.LoadFromFile(LogConfig::Utf8Path(typeDump)))
            {
                LOG_ERROR("server.gameserver", "Cannot load the type dump {}", typeDump);
                return false;
            }

            _databases = std::make_unique<DatabaseLoader>(Config());
            _databases->AddDatabase(LoginDatabase, "Login", DatabaseLoader::DATABASE_LOGIN)
                .AddDatabase(CharacterDatabase, "Character", DatabaseLoader::DATABASE_CHARACTER)
                .AddDatabase(WorldDatabase, "World", DatabaseLoader::DATABASE_WORLD);
            if (!_databases->Load())
            {
                LOG_ERROR("server.gameserver", "Cannot open the realm's databases");
                _databases.reset();
                return false;
            }
            AppenderDB::Enable(Logger(), Config().GetOption<uint32>("RealmID", 1, true));
            return true;
        }

        void OnStop() override
        {
            AppenderDB::Disable(Logger());
            if (_databases)
                _databases->Close();
            _databases.reset();
        }

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
        std::unique_ptr<DatabaseLoader> _databases;
    };
}

int main(int argc, char** argv)
{
    GameServerApp app;
    return app.Run(std::vector<std::string>(argv, argv + argc));
}
