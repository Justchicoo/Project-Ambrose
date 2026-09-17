/*
 * Project Ambrose by Imjustchico
 * Game server entry point: runs guided setup for the install and type dump, loads the type dump and, when ClientDir names the user's install, the locale text of its Root.wad in Locale.Default, brings the login, characters and world databases current and opens them, loads the character name tables when the world database is open, offering on a terminal to extract them from the install when they are empty, then runs the world update tick whose interval follows World.UpdateInterval live.
 */

#include "AppenderDB.h"
#include "CharacterNameExtractor.h"
#include "CharacterNameMgr.h"
#include "CharacterNameScript.h"
#include "ClientSetup.h"
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

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
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
            std::unique_ptr<SetupPrompt> const prompt = SetupPrompt::ForProcess(std::cout, Config().GetOption<bool>("Setup.Prompt", true, true),
                std::chrono::seconds(Config().GetOption<uint32>("Setup.PromptTimeout", ClientSetup::DefaultTimeoutSeconds, true)));
            LocalClientSystem const system;
            ClientSetup::ForServer(Config(), *prompt, system, { "gameserver", true, true }, [](bool warning, std::string const& text)
            {
                if (warning)
                    LOG_WARN("server.gameserver", "{}", text);
                else
                    LOG_INFO("server.gameserver", "{}", text);
            });

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
            sCharacterNameMgr.SetDefaultLocale(locale);
            if (!WorldDatabase.IsOpen())
                LOG_WARN("server.gameserver", "WorldDatabaseInfo is empty, so the character name tables are not loaded");
            else
            {
                CharacterNameLoadResult names = sCharacterNameMgr.Load();
                if (names.Loaded && names.Tables == 0 && OfferNameExtraction(*prompt))
                    names = sCharacterNameMgr.Load();
                if (!names.Loaded)
                {
                    for (std::string const& problem : names.Errors)
                        LOG_ERROR("server.gameserver", "Character name tables: {}", problem);
                    LOG_ERROR("server.gameserver", "Cannot load the character name tables from the world database");
                    _databases->Close();
                    _databases.reset();
                    return false;
                }
                if (names.Tables == 0)
                    LOG_WARN("server.gameserver", "The world database holds no character name tables, so wizard names cannot be checked or shown; run the extractor's names command against your install");
                else
                    LOG_INFO("server.gameserver", "Loaded {} character name tables holding {} names in {} locales, and {} disallowed names", names.Tables, names.Parts, names.HumanLocales, names.Disallowed);
                for (std::string const& warning : names.Warnings)
                    LOG_WARN("server.gameserver", "Character name tables: {}", warning);
            }
            AppenderDB::Enable(Logger(), Config().GetOption<uint32>("RealmID", 1, true));
            return true;
        }

        bool OfferNameExtraction(SetupPrompt& prompt)
        {
            std::string const clientDir = Config().GetOption<std::string>("ClientDir", "", true);
            std::string const typeDump = Config().GetOption<std::string>("TypeDumpPath", "", true);
            if (!prompt.IsInteractive() || clientDir.empty() || typeDump.empty())
                return false;
            if (!prompt.Confirm(fmt::format("The world database has no character name tables. Extract them now from your install in {}?", clientDir)))
                return false;
            std::string error;
            std::optional<NameExtraction> const extraction = CharacterNameExtractor::ExtractFromInstall(LogConfig::Utf8Path(clientDir), LogConfig::Utf8Path(typeDump), error);
            if (!extraction)
            {
                LOG_ERROR("server.gameserver", "Cannot extract the character name tables: {}", error);
                return false;
            }
            if (!extraction->Ok())
            {
                for (std::string const& problem : extraction->Errors)
                    LOG_ERROR("server.gameserver", "Character name extraction: {}", problem);
                return false;
            }
            std::optional<MySQLConnectionInfo> const world = MySQLConnectionInfo::Parse(Config().GetOption<std::string>("WorldDatabaseInfo", "", true), &error);
            if (!world || !CharacterNameScript::Build(*extraction).Apply(*world, error))
            {
                LOG_ERROR("server.gameserver", "Cannot write the character name tables to the world database: {}", error);
                return false;
            }
            LOG_INFO("server.gameserver", "Extracted {} character name tables holding {} names from {}", extraction->Tables.size(), extraction->GetPartCount(), clientDir);
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
