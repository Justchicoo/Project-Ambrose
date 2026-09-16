/*
 * Project Ambrose by Imjustchico
 * Game server entry point: loads the type dump and, when ClientDir names the user's install, the locale text of its Root.wad in Locale.Default, brings the login, characters and world databases current and opens them, then runs the world update tick whose interval follows World.UpdateInterval live.
 */

#include "AppenderDB.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Environment.h"
#include "Log.h"
#include "LocaleStore.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "ServerApp.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
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

            std::string const clientDir = Config().GetOption<std::string>("ClientDir", "", true);
            std::string const locale = Config().GetOption<std::string>("Locale.Default", "en-US", true);
            if (clientDir.empty())
                LOG_WARN("server.gameserver", "ClientDir is not set, so locale keys cannot be resolved to text");
            else
            {
                std::filesystem::path const rootWad = LogConfig::Utf8Path(clientDir) / "Data" / "GameData" / "Root.wad";
                std::string error;
                if (!sLocaleStore.Load(rootWad, locale, error))
                {
                    LOG_ERROR("server.gameserver", "Cannot load the {} locale from {}: {}", locale, ConfigMgr::PathToUtf8(rootWad), error);
                    return false;
                }
                std::shared_ptr<LocaleTable const> const table = sLocaleStore.GetTable(locale, &error);
                if (!table)
                {
                    LOG_ERROR("server.gameserver", "The {} locale was unloaded while starting: {}", locale, error);
                    return false;
                }
                for (std::string const& problem : table->GetProblems())
                    LOG_WARN("server.gameserver", "The {} locale skipped {}", locale, problem);
                LOG_INFO("server.gameserver", "Loaded the {} locale: {} files, {} keys, {} repeated keys whose later text is kept, {} files skipped; {} locales installed, the others load when first used",
                    locale, table->GetFileCount(), table->GetKeyCount(), table->GetDuplicateCount(), table->GetProblems().size(), sLocaleStore.GetLocales().size());
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
    return app.Run(Ambrose::GetArguments(argc, argv));
}
