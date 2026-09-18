/*
 * Project Ambrose by Imjustchico
 * Game server entry point: runs setup in Setup.Mode for the install and type dump, stopping cleanly when a stop arrives meanwhile, loads the type dump and the locale text of the install's Root.wad in Locale.Default, brings the login, characters and world databases current and opens them, loads the character name tables when the world database is open and, when they are empty, extracts them from the install and reloads them, automatically in auto mode, after a yes in ask mode and never in off mode, then runs the world update tick whose interval follows World.UpdateInterval live.
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
        GameServerApp() : ServerApp({ "gameserver", "gameserver.conf", 12343 }, sConfigMgr, sLog, std::cout, std::cerr)
        {
        }

    protected:
        bool OnStart() override
        {
            LocalClientSystem const system;
            ClientSetup::Report const report = [](bool warning, std::string const& text)
            {
                if (warning)
                    LOG_WARN("server.gameserver", "{}", text);
                else
                    LOG_INFO("server.gameserver", "{}", text);
            };
            std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ServerPrompt(std::cout, Config());
            prompt->SetCancellation([this] { return PollStopRequested(); });
            ClientSetupResult const setup = ClientSetup::ForServer(Config(), *prompt, system, ClientSetup::ServerTypeDumps(Config(), system, report, [this] { return PollStopRequested(); }),
                { "gameserver", true, true, false }, report);
            if (PollStopRequested())
                return false;

            std::vector<std::string> limitProblems;
            SerializerLimits::Apply(SerializerLimits::Load(Config(), &limitProblems));
            for (std::string const& problem : limitProblems)
                LOG_WARN("server.gameserver", "{}", problem);

            if (!setup.TypeDump)
                LOG_WARN("server.gameserver", "No type dump is in use, so ObjectProperty data cannot be read or written: {}", setup.TypeDumpError);
            else if (!sTypeRegistry.LoadFromFile(*setup.TypeDump))
            {
                LOG_ERROR("server.gameserver", "Cannot load the type dump {}", ConfigMgr::PathToUtf8(*setup.TypeDump));
                return false;
            }

            std::string const locale = Config().GetOption<std::string>("Locale.Default", "en-US", true);
            if (!setup.Install)
                LOG_WARN("server.gameserver", "No Wizard101 install is in use, so locale keys cannot be resolved to text");
            else
            {
                std::filesystem::path const rootWad = setup.Install->Root / "Data" / "GameData" / "Root.wad";
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
                if (names.Loaded && names.Tables == 0 && ExtractNames(setup, *prompt))
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

        bool ExtractNames(ClientSetupResult const& setup, SetupPrompt& prompt)
        {
            if (!setup.Install || !setup.TypeDump)
                return false;
            std::string const install = setup.Install->Describe();
            if (setup.Mode == SetupMode::Off)
            {
                LOG_WARN("server.gameserver", "The world database has no character name tables, and Setup.Mode is off, so they are not extracted from {}; set Setup.Mode = auto, or run the extractor's names command", install);
                return false;
            }
            if (setup.Mode == SetupMode::Ask)
            {
                if (!prompt.IsInteractive())
                {
                    LOG_WARN("server.gameserver", "The world database has no character name tables, and setup could not ask whether to extract them from {}; start the game server in a terminal, set Setup.Mode = auto, or run the extractor's names command", install);
                    return false;
                }
                if (!prompt.Confirm(fmt::format("The world database has no character name tables. Extract them now from your install {}?", install)))
                    return false;
            }
            else
                LOG_INFO("server.gameserver", "The world database has no character name tables, so they are extracted from {}", install);
            std::string error;
            std::optional<NameExtraction> const extraction = CharacterNameExtractor::ExtractFromInstall(setup.Install->Root, *setup.TypeDump, error);
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
            LOG_INFO("server.gameserver", "Extracted {} character name tables holding {} names from {}", extraction->Tables.size(), extraction->GetPartCount(), install);
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
