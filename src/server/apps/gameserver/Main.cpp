/*
 * Project Ambrose by Imjustchico
 * Game server entry point: runs setup in Setup.Mode for the install and type dump, stopping cleanly when a stop arrives meanwhile, loads the type dump and the locale text of the install's Root.wad in Locale.Default, brings the login, characters and world databases current and opens them, which the admin API reports, lists the updates of and applies data-only updates to while the server runs, reloading the character name tables after the world database takes one, loads the character name tables when the world database is open and, when they are empty, extracts them from the install and reloads them, automatically in auto mode, after a yes in ask mode and never in off mode, loads the zones, the named places inside them and the objects placed in them and registers each as a reload target, refusing to start when they cannot be read, loads the scripts and tells them the server has started, then runs the world update tick whose interval follows World.UpdateInterval live and carries every script's OnUpdate, and tells them it is shutting down before the databases close.
 */

#include "TypeDumpCache.h"
#include "RealmHeartbeat.h"
#include "RealmList.h"
#include "AdminDatabaseView.h"
#include "AdminServer.h"
#include "AppenderDB.h"
#include "CharacterNameExtractor.h"
#include "CharacterNameMgr.h"
#include "ZoneMgr.h"
#include "CharacterNameScript.h"
#include "AccountMgr.h"
#include "ClientSetup.h"
#include "CommandMgr.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Environment.h"
#include "Log.h"
#include "LocaleStore.h"
#include "ObjectSerializer.h"
#include "ScriptLoader.h"
#include "ScriptMgr.h"
#include "GameMessageTable.h"
#include "GameSession.h"
#include "MessageRegistry.h"
#include "SessionContext.h"
#include "SocketMgr.h"
#include "World.h"
#include "ServerApp.h"
#include "StatsRegistry.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class GameServerApp : public ServerApp
    {
    public:
        static constexpr uint16 DefaultWorldPort = 12333;

        GameServerApp() : ServerApp({ "gameserver", "gameserver.conf", 12343 }, sConfigMgr, sLog, std::cout, std::cerr), _databases(Config()), _databaseView(_databases)
        {
            _databases.AddDatabase(LoginDatabase, "Login", DatabaseLoader::DATABASE_LOGIN)
                .AddDatabase(CharacterDatabase, "Character", DatabaseLoader::DATABASE_CHARACTER)
                .AddDatabase(WorldDatabase, "World", DatabaseLoader::DATABASE_WORLD);
            _databaseView.AddStore("character names", WorldDatabase.GetName(), []
            {
                CharacterNameLoadResult const names = sCharacterNameMgr.Load();
                if (names.Loaded)
                    LOG_INFO("server.gameserver", "Reloaded {} character name tables holding {} names in {} locales, and {} disallowed names", names.Tables, names.Parts, names.HumanLocales, names.Disallowed);
                else
                    for (std::string const& problem : names.Errors)
                        LOG_ERROR("server.gameserver", "Character name tables were not reloaded, and the loaded ones stay in use: {}", problem);
                for (std::string const& warning : names.Warnings)
                    LOG_WARN("server.gameserver", "Character name tables: {}", warning);
                return AdminStoreReload{ names.Loaded, names.Errors, names.Warnings };
            });
        }

    protected:
        void OnAdminApiReady(AdminServer& admin) override
        {
            _databaseView.Register(admin.Routes());
        }

        std::vector<RestartRequiredOption> GetRestartRequiredOptions() const override
        {
            std::vector<RestartRequiredOption> options(ClientSetup::RestartRequiredOptions.begin(), ClientSetup::RestartRequiredOptions.end());
            options.insert(options.end(), DatabaseLoader::RestartRequiredOptions.begin(), DatabaseLoader::RestartRequiredOptions.end());
            options.push_back({ "RealmID", "A running game server keeps the realm it started as, because its players and log rows belong to that realm, so a change takes effect at the next start" });
            return options;
        }

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
            SetClientSetup(setup);
            if (PollStopRequested())
                return false;
            SetClientSetup(setup.Install.has_value(), setup.TypeDump.has_value(), false, setup.TypeDumpError);

            std::vector<std::string> limitProblems;
            SerializerLimits::Apply(SerializerLimits::Load(Config(), &limitProblems));
            for (std::string const& problem : limitProblems)
                LOG_WARN("server.gameserver", "{}", problem);

            if (!setup.TypeDump)
                LOG_WARN("server.gameserver", "No type dump is in use, so ObjectProperty data cannot be read or written: {}", setup.TypeDumpError);
            else
            {
                std::filesystem::path const binary = TypeDumpCache::FastCopyOf(*setup.TypeDump);
                if (std::string fastCopyError; !TypeDumpCache::EnsureFastCopy(*setup.TypeDump, fastCopyError))
                    LOG_WARN("server.worldserver", "The type dump's fast copy could not be built, so it is read from JSON this time: {}", fastCopyError);
                bool loaded = false;
                if (std::filesystem::exists(binary))
                    loaded = sTypeRegistry.LoadBinary(binary, *setup.TypeDump, setup.Install ? setup.Install->Revision : std::string_view{});
                else
                    loaded = sTypeRegistry.LoadFromFile(*setup.TypeDump);
                if (!loaded)
                {
                    LOG_ERROR("server.gameserver", "Cannot load the type dump {}", ConfigMgr::PathToUtf8(*setup.TypeDump));
                    return false;
                }
            }

            MessageHandlerTable<GameSession> const& messages = GameMessageTable::Get();
            std::vector<std::string> messageErrors;
            if (!messages.Declare(sMessageRegistry, messageErrors))
            {
                for (std::string const& error : messageErrors)
                    LOG_ERROR("server.gameserver", "{}", error);
                LOG_ERROR("server.gameserver", "The game message table does not match the loaded message definitions");
                return false;
            }
            if (!setup.Install)
                LOG_WARN("server.gameserver", "No Wizard101 install is in use, so client messages are logged by service and order only");
            else if (!sMessageRegistry.LoadFromClient(setup.Install->Root))
            {
                LOG_ERROR("server.gameserver", "Cannot load the message definitions from the client in {}", ClientLocator::PathText(setup.Install->Root));
                return false;
            }
            else if (!messages.Validate(*sMessageRegistry.GetCatalog(), messageErrors))
            {
                for (std::string const& error : messageErrors)
                    LOG_ERROR("server.gameserver", "{}", error);
                LOG_ERROR("server.gameserver", "The game message table does not match the client's message definitions in {}", ClientLocator::PathText(setup.Install->Root));
                return false;
            }
            else
                SetMessageSource(setup.Install->Root);

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

            if (!_databases.Load())
            {
                LOG_ERROR("server.gameserver", "Cannot open the realm's databases");
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
                    _databases.Close();
                    return false;
                }
                if (names.Tables == 0)
                    LOG_WARN("server.gameserver", "The world database holds no character name tables, so wizard names cannot be checked or shown; run the extractor's names command against your install");
                else
                    LOG_INFO("server.gameserver", "Loaded {} character name tables holding {} names in {} locales, and {} disallowed names", names.Tables, names.Parts, names.HumanLocales, names.Disallowed);
                for (std::string const& warning : names.Warnings)
                    LOG_WARN("server.gameserver", "Character name tables: {}", warning);
            }
            sZoneMgr.RegisterReloadTargets();
            if (WorldDatabase.IsOpen())
            {
                ZoneLoadResult const zones = sZoneMgr.LoadAll();
                if (!zones.Loaded)
                {
                    LOG_ERROR("server.gameserver", "Cannot load the zones from the world database");
                    _databases.Close();
                    return false;
                }
                if (zones.Zones == 0)
                    LOG_WARN("server.gameserver", "The world database holds no zone, so there is nowhere to stand; run the zone extractor against your install");
            }

            uint32 const realmId = Config().GetOption<uint32>("RealmID", 1, true);
            AppenderDB::Enable(Logger(), realmId);
            GameSession::SetRealmId(realmId);
            sScriptMgr.LoadScripts(&AddScripts);
            sScriptMgr.OnConfigLoad(false);
            LoadCommands();
            sScriptMgr.OnStartup();
            std::vector<std::string> networkProblems;
            _context = std::make_shared<SessionContext>(SessionSettings::Load(Config(), &networkProblems));
            NetworkSettings const network = NetworkSettings::Load(Config(), "WorldServerPort", DefaultWorldPort, &networkProblems);
            for (std::string const& problem : networkProblems)
                LOG_WARN("server.gameserver", "{}", problem);

            _sockets = std::make_unique<SocketMgr<GameSession>>([context = _context](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
            {
                std::shared_ptr<GameSession> session = std::make_shared<GameSession>(std::move(socket), limits, context);
                sWorld.AddSession(session);
                return session;
            });
            std::string listenError;
            if (!_sockets->StartNetwork(network, listenError))
            {
                LOG_ERROR("server.gameserver", "Cannot listen for clients: {}", listenError);
                _sockets.reset();
                AppenderDB::Disable(Logger());
                _databases.Close();
                return false;
            }
            SetListener(network.BindIp, _sockets->GetPort());
            sStats.Publish("sessions", [this] { return Ambrose::StatValue(static_cast<int64>(_sockets ? _sockets->GetConnectionCount() : 0)); });
            sStats.Publish("realm_beating", [this] { return Ambrose::StatValue(_heartbeat.Beating()); });

            _heartbeat.Configure(RealmHeartbeatSettings::Load(Config()),
                [](std::string const& realm, uint32 population, int64 heartbeat, bool online)
                {
                    if (!LoginDatabase.IsOpen())
                        return;
                    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> beat = LoginDatabase.GetPreparedStatement(LOGIN_UPD_REALM_HEARTBEAT);
                    if (!beat)
                        return;
                    beat->SetData(0, population);
                    beat->SetData(1, static_cast<uint64>(heartbeat));
                    beat->SetData(2, online ? uint32{ 0 } : uint32{ REALM_FLAG_OFFLINE });
                    beat->SetData(3, realm);
                    LoginDatabase.DirectExecute(*beat);
                },
                [] { return static_cast<uint32>(sWorld.GetSessionCount()); },
                [](RealmHeartbeatSettings const& realm)
                {
                    if (!LoginDatabase.IsOpen())
                        return;
                    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> add = LoginDatabase.GetPreparedStatement(LOGIN_INS_REALM);
                    if (!add)
                        return;
                    add->SetData(0, realm.RealmName);
                    add->SetData(1, realm.Address);
                    add->SetData(2, realm.Address);
                    add->SetData(3, realm.Port);
                    if (std::optional<uint64> const added = LoginDatabase.DirectExecuteCounted(*add); added && *added > 0)
                        LOG_INFO("server.worldserver", "Realm {} was not in the realm list, so it was added at {}:{}; edit that row to change where players reach it",
                            realm.RealmName, realm.Address, realm.Port);
                },
                [] { return LoginDatabase.IsOpen(); });
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

        void LoadCommands()
        {
            sCommandMgr.SetPrefix(Config().GetOption<std::string>("GM.CommandPrefix", std::string(CommandMgr::DefaultPrefix), true));
            sCommandMgr.SetLogging(Config().GetOption<bool>("GM.LogCommands", true, true));
            sCommandMgr.Load(sScriptMgr.GetCommands());
            std::map<std::string, uint8, std::less<>> overrides;
            if (WorldDatabase.IsOpen())
            {
                if (QueryResult rows = WorldDatabase.Query("SELECT command, security_level FROM command_security"))
                {
                    do
                    {
                        Field const* row = rows->Fetch();
                        overrides.emplace(row[0].Get<std::string>(), row[1].Get<uint8>());
                    } while (rows->NextRow());
                }
            }
            if (!overrides.empty())
                LOG_INFO("server.commands", "{} command(s) have a level from command_security", overrides.size());
            sCommandMgr.SetOverrides(std::move(overrides));
            RegisterCommandConsole();
            LOG_INFO("server.commands", "{} command(s) are ready, typed after {}", sCommandMgr.GetCommandCount(), sCommandMgr.GetPrefix());
        }

        void RegisterCommandConsole()
        {
            for (std::string const& line : sCommandMgr.Describe(SEC_CONSOLE, true))
            {
                std::string const name = line.substr(0, line.find(" - "));
                Commands().Register({ name, "", line.find(" - ") == std::string::npos ? std::string() : line.substr(line.find(" - ") + 3), false,
                    [name](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                    {
                        std::string line = name;
                        for (std::string const& argument : arguments)
                            line += " " + argument;
                        ConsoleCaller caller([&reply](std::string_view text) { reply(text); });
                        return sCommandMgr.Execute(caller, line) != CommandResult::Empty;
                    } });
            }
        }

        void OnStop() override
        {
            sStats.Unpublish("sessions");
            sStats.Unpublish("realm_beating");
            _heartbeat.Stop();
            if (_sockets)
                _sockets->StopNetwork();
            _sockets.reset();
            sWorld.Clear();
            sCommandMgr.Clear();
            sScriptMgr.OnShutdown();
            sScriptMgr.Unload();
            AppenderDB::Disable(Logger());
            _databases.Close();
        }

        std::chrono::milliseconds GetUpdateInterval() const override
        {
            uint32 const configured = sConfigMgr.GetOption<uint32>("World.UpdateInterval", 50, true);
            uint32 const interval = std::clamp<uint32>(configured, 1, 10000);
            if (configured != interval && configured != _reportedInterval.exchange(configured))
                AMBROSE_LOG(sLog, LogLevel::Warn, "server.gameserver", "World.UpdateInterval {} is outside 1..10000, using {} ms", configured, interval);
            return std::chrono::milliseconds(interval);
        }

        void OnUpdate(std::chrono::milliseconds diff) override
        {
            sWorld.Update(diff);
            _heartbeat.Update(diff);
        }

    private:
        mutable std::atomic<uint32> _reportedInterval{ 0 };
        RealmHeartbeat _heartbeat;
        DatabaseLoader _databases;
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<GameSession>> _sockets;
        AdminDatabaseView _databaseView;
    };
}

int main(int argc, char** argv)
{
    GameServerApp app;
    return app.Run(Ambrose::GetArguments(argc, argv));
}
