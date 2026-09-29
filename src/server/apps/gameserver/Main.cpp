/*
 * Project Ambrose by Imjustchico
 * Game server entry point: runs setup in Setup.Mode for the install and type dump, stopping cleanly when a stop arrives meanwhile, loads the type dump and the locale text of the install's Root.wad in Locale.Default, brings the login, characters and world databases current and opens them, which the admin API reports, lists the updates of and applies data-only updates to while the server runs, reloading the character name tables and the level and stat tables after the world database takes one, writes the classes the install holds that its type dump does not describe to the world database when it holds none marked install, from the class file schemaprobe builds once per revision, the same way it asks before other extractions and starting without them when that fails, loads the character name tables and the level and stat tables when the world database is open and, when either set is empty, extracts it from the install and loads it again, automatically in auto mode, after a yes in ask mode and never in off mode, registering the level and stat sets as reload targets, loads the zones, the named places inside them and the objects placed in them, extracting them from the install first when the world database holds none, the same way it does the level tables, with each extraction, each zone archive and each write to the world database reported as a start step with the time it may take, so a supervisor waits for a first run that is still working and ends only one that stalls, and registers each as a reload target, refusing to start when they cannot be read, has every zone instance filled with the objects its zone places that the server sends, loads the scripts and tells them the server has started, then runs the world update tick whose interval follows World.UpdateInterval live and carries every script's OnUpdate, and tells them it is shutting down before the databases close, after every wizard still in the world has left it and so been saved. Its live settings open over the characters database, and a change to the command prefix, command logging, default locale, session limits, template cache or realm heartbeat is applied on the world thread. It reads the template manifest before the player's template and then every spell and sigil, each a reload target.
 */

#include "TypeDumpCache.h"
#include "RealmHeartbeat.h"
#include "Settings.h"
#include "RealmList.h"
#include "AdminDatabaseView.h"
#include "AdminRouter.h"
#include "AdminServer.h"
#include "AppenderDB.h"
#include "CharacterNameExtractor.h"
#include "CharacterNameMgr.h"
#include "MapMgr.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "SigilMgr.h"
#include "SpellMgr.h"
#include "ZoneMgr.h"
#include "CharacterNameScript.h"
#include "LevelExtractor.h"
#include "LevelScript.h"
#include "MapObjectSpawner.h"
#include "ZoneExtractor.h"
#include "ZoneScript.h"
#include "ServerClassCache.h"
#include "ServerClassScript.h"
#include "StringUtil.h"
#include "StartProgress.h"
#include "PlayerLevelMgr.h"
#include "AccountMgr.h"
#include "AdminCommand.h"
#include "ClientSetup.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "DatabaseSettingStore.h"
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

#include <nlohmann/json.hpp>

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
    nlohmann::json TickProfileState(WorldTickProfileSnapshot const& profile)
    {
        return {
            { "schema", 1 },
            { "active", profile.Active },
            { "complete", profile.Complete },
            { "truncated", profile.Truncated },
            { "requested_seconds", profile.RequestedSeconds },
            { "events", profile.Events.size() }
        };
    }

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
            _databaseView.AddStore("level and stat tables", WorldDatabase.GetName(), []
            {
                PlayerLevelLoadResult const levels = sPlayerLevelMgr.Load();
                if (levels.Loaded)
                    LOG_INFO("server.gameserver", "Reloaded {} magic schools with level tables for {} of them up to level {}, and {} stat settings with {} band values", levels.Schools, levels.LevelTables, levels.MaxLevel, levels.StatSettings, levels.BandValues);
                else
                    for (std::string const& problem : levels.Errors)
                        LOG_ERROR("server.gameserver", "Level and stat tables were not reloaded, and the loaded ones stay in use: {}", problem);
                return AdminStoreReload{ levels.Loaded, levels.Errors, {} };
            });
        }

    protected:
        void RegisterAdminCommand(AdminRouter& routes) override
        {
            AdminCommand::Register(routes,
                [](std::string const& line, uint8 level, bool confirmed)
                {
                    AdminCommandOutcome outcome;
                    outcome.Command = sCommandMgr.DescribeForLog(line);
                    CommandMatch const match = sCommandMgr.Parse(line);
                    if (!match.Found || level < match.SecurityLevel)
                    {
                        outcome.Refused = true;
                        outcome.Reason = "there is no such command";
                        return outcome;
                    }
                    if (AdminCommand::IsDestructive(line) && !confirmed)
                    {
                        outcome.Refused = true;
                        outcome.NeedsConfirm = true;
                        outcome.Reason = "this command changes something that cannot be undone, so it needs confirm";
                        return outcome;
                    }
                    RecordingCaller caller(level, true, "panel operator");
                    CommandResult const result = sCommandMgr.Execute(caller, line);
                    outcome.Lines = caller.GetLines();
                    outcome.Ran = result == CommandResult::Ran;
                    outcome.Refused = !outcome.Ran;
                    if (result == CommandResult::Unknown)
                        outcome.Reason = "there is no such command";
                    else if (result == CommandResult::Usage)
                        outcome.Reason = "the command was not used the way it takes";
                    else if (result == CommandResult::Empty)
                        outcome.Reason = "there was no command to run";
                    else if (result == CommandResult::Refused)
                        outcome.Reason = "the command refused to run";
                    return outcome;
                }, Commands(), GetInfo().Name, CommandAuditFile(),
                [](std::string_view line) { return sCommandMgr.DescribeForLog(line); });
        }

        void OnAdminApiReady(AdminServer& admin) override
        {
            _databaseView.Register(admin.Routes());
            admin.Routes().AddGuarded("POST", "/api/tick-profile", "metrics.profile", [](AdminRequest const& request)
            {
                nlohmann::json const body = nlohmann::json::parse(request.Body, nullptr, false);
                if (!body.is_object())
                    return AdminResponse::Invalid("A tick profile request takes a JSON object", { { "seconds", "Choose a duration from 1 to 30 seconds" } });
                std::vector<std::pair<std::string, std::string>> fields;
                std::optional<uint32> duration;
                for (auto const& [key, value] : body.items())
                {
                    if (key != "seconds")
                        fields.emplace_back(key, "A tick profile request takes only seconds");
                    else if (value.is_number_unsigned())
                    {
                        uint64 const seconds = value.get<uint64>();
                        if (seconds < 1 || seconds > 30)
                            fields.emplace_back("seconds", "Choose a whole number of seconds from 1 to 30");
                        else
                            duration = static_cast<uint32>(seconds);
                    }
                    else if (value.is_number_integer())
                    {
                        int64 const seconds = value.get<int64>();
                        if (seconds < 1 || seconds > 30)
                            fields.emplace_back("seconds", "Choose a whole number of seconds from 1 to 30");
                        else
                            duration = static_cast<uint32>(seconds);
                    }
                    else
                        fields.emplace_back("seconds", "Choose a whole number of seconds from 1 to 30");
                }
                if (!body.contains("seconds"))
                    fields.emplace_back("seconds", "Choose a whole number of seconds from 1 to 30");
                if (!fields.empty())
                    return AdminResponse::Invalid("The tick profile request has problems", std::move(fields));
                if (!sWorld.StartTickProfile(*duration))
                    return AdminResponse::Problem(409, "profile_active", "A world tick profile is already running");
                return AdminResponse::Json(202, TickProfileState(sWorld.GetTickProfile(false)).dump());
            });
            admin.Routes().AddGuarded("GET", "/api/tick-profile", "metrics.profile", [](AdminRequest const&)
            {
                return AdminResponse::Json(200, TickProfileState(sWorld.GetTickProfile(false)).dump());
            });
            admin.Routes().AddGuarded("GET", "/api/tick-profile/trace", "metrics.profile", [](AdminRequest const&)
            {
                WorldTickProfileSnapshot const profile = sWorld.GetTickProfile();
                if (profile.Active)
                    return AdminResponse::Problem(409, "profile_running", "The tick profile has not finished");
                if (!profile.Complete)
                    return AdminResponse::Problem(404, "profile_missing", "No completed tick profile is available");
                return AdminResponse::Json(200, nlohmann::json{
                    { "schema", 1 },
                    { "requested_seconds", profile.RequestedSeconds },
                    { "truncated", profile.Truncated },
                    { "events", profile.Events.size() },
                    { "trace", World::TickProfileTraceJson(profile) }
                }.dump());
            });
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

            std::string const locale = sSettings.Get<std::string>("Locale.Default");
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
            if (!CharacterDatabase.IsOpen())
                LOG_WARN("server.gameserver", "CharacterDatabaseInfo is empty, so live settings take their config values and cannot be changed or kept");
            if (!StartSettings(CharacterDatabase.IsOpen() ? SettingStores::ForCharacters() : nullptr))
            {
                _databases.Close();
                return false;
            }
            _settingsSubscription = sSettings.Subscribe([this](SettingChange const& change) { ApplySetting(change); });
            ExtractServerClasses(setup, system, *prompt);
            if (!LoadObjectSchema(setup) || !LoadObjectTemplates(setup) || !LoadSpells(setup) || !LoadSigils(setup))
            {
                _databases.Close();
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
            sPlayerLevelMgr.RegisterReloadTargets();
            if (!WorldDatabase.IsOpen())
                LOG_WARN("server.gameserver", "WorldDatabaseInfo is empty, so the level and stat tables are not loaded");
            else if (!LoadPlayerLevels(setup, *prompt))
            {
                _databases.Close();
                return false;
            }
            sZoneMgr.RegisterReloadTargets();
            sMapMgr.SetSettingsReader([]
            {
                MapSettings settings;
                settings.UnloadDelay = std::chrono::seconds(sSettings.Get<uint32>("Zone.UnloadDelay"));
                settings.MobileIdReleaseDelay = std::chrono::milliseconds(sSettings.Get<uint32>("Zone.MobileIdReleaseDelay"));
                return settings;
            });
            sMapMgr.SetObjectPopulator(&MapObjectSpawner::PopulateFromWorld);
            if (WorldDatabase.IsOpen())
            {
                ZoneLoadResult zones = sZoneMgr.LoadAll();
                if (zones.Loaded && zones.Zones == 0 && ExtractZones(setup, *prompt))
                    zones = sZoneMgr.LoadAll();
                if (!zones.Loaded)
                {
                    LOG_ERROR("server.gameserver", "Cannot load the zones from the world database");
                    _databases.Close();
                    return false;
                }
                if (zones.Zones == 0)
                    LOG_WARN("server.gameserver", "The world database holds no zone, so there is nowhere to stand; run the extractor's zones command against your install");
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

        bool LoadObjectSchema(ClientSetupResult const& setup)
        {
            std::vector<std::string> errors;
            if (!WorldDatabase.IsOpen())
                LOG_WARN("server.gameserver", "WorldDatabaseInfo is empty, so the classes the type dump does not describe, the core object types and the behavior classes are not loaded");
            else if (!sObjectSchemaMgr.LoadClasses(errors))
            {
                for (std::string const& problem : errors)
                    LOG_ERROR("server.gameserver", "Server classes: {}", problem);
                LOG_ERROR("server.gameserver", "Cannot load the classes the type dump does not describe from the world database");
                return false;
            }

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

            sObjectSchemaMgr.RegisterReloadTargets();
            if (!WorldDatabase.IsOpen() || !sTypeRegistry.IsLoaded())
                return true;
            ObjectSchemaLoadResult const tables = sObjectSchemaMgr.LoadTables();
            if (!tables.Loaded)
            {
                for (std::string const& problem : tables.Errors)
                    LOG_ERROR("server.gameserver", "Object schema: {}", problem);
                LOG_ERROR("server.gameserver", "Cannot load the core object types and behavior classes from the world database");
                return false;
            }
            return true;
        }

        bool LoadSigils(ClientSetupResult const& setup)
        {
            sSigilMgr.RegisterReloadTargets();
            if (!setup.Install || !sTypeRegistry.IsLoaded())
            {
                LOG_WARN("server.gameserver", "No Wizard101 install or type dump is in use, so no sigil is read");
                return true;
            }
            sSigilMgr.SetInstall(setup.Install->Root);
            std::vector<std::string> errors;
            if (sSigilMgr.Load(errors))
                return true;
            for (std::string const& problem : errors)
                LOG_ERROR("server.gameserver", "Sigils: {}", problem);
            LOG_ERROR("server.gameserver", "Cannot read the sigils from {}", ClientLocator::PathText(setup.Install->Root));
            return false;
        }

        bool LoadSpells(ClientSetupResult const& setup)
        {
            sSpellMgr.RegisterReloadTargets();
            if (!setup.Install || !sTypeRegistry.IsLoaded())
            {
                LOG_WARN("server.gameserver", "No Wizard101 install or type dump is in use, so no spell is read");
                return true;
            }
            sSpellMgr.SetInstall(setup.Install->Root);
            std::vector<std::string> errors;
            if (sSpellMgr.Load(errors))
                return true;
            for (std::string const& problem : errors)
                LOG_ERROR("server.gameserver", "Spells: {}", problem);
            LOG_ERROR("server.gameserver", "Cannot read the spells from {}", ClientLocator::PathText(setup.Install->Root));
            return false;
        }

        bool LoadObjectTemplates(ClientSetupResult const& setup)
        {
            sObjectTemplateMgr.RegisterReloadTargets();
            sObjectTemplateMgr.SetBudget(std::size_t{ sSettings.Get<uint32>("Templates.CacheSize") } << 20);
            if (!setup.Install || !sTypeRegistry.IsLoaded())
            {
                LOG_WARN("server.gameserver", "No Wizard101 install or type dump is in use, so no template is read and no wizard can enter the world");
                return true;
            }
            sObjectTemplateMgr.SetInstall(setup.Install->Root);
            std::vector<std::string> errors;
            if (sObjectTemplateMgr.LoadManifest(errors) && sObjectTemplateMgr.LoadPlayer(errors))
                return true;
            for (std::string const& problem : errors)
                LOG_ERROR("server.gameserver", "Object templates: {}", problem);
            LOG_ERROR("server.gameserver", "Cannot read the object templates from {}", ClientLocator::PathText(setup.Install->Root));
            return false;
        }

        static constexpr std::chrono::minutes ExtractionAllowance{ 5 };
        static constexpr std::chrono::minutes ArchiveAllowance{ 2 };
        static constexpr std::chrono::minutes WriteAllowance{ 15 };

        bool ConfirmExtraction(ClientSetupResult const& setup, SetupPrompt& prompt, std::string_view tables, std::string_view command, std::string_view state = "has no")
        {
            if (!setup.Install || !setup.TypeDump)
                return false;
            std::string const install = setup.Install->Describe();
            if (setup.Mode == SetupMode::Off)
            {
                LOG_WARN("server.gameserver", "The world database {} {}, and Setup.Mode is off, so they are not extracted from {}; set Setup.Mode = auto, or run the extractor's {} command", state, tables, install, command);
                return false;
            }
            if (setup.Mode == SetupMode::Ask)
            {
                if (!prompt.IsInteractive())
                {
                    LOG_WARN("server.gameserver", "The world database {} {}, and setup could not ask whether to extract them from {}; start the game server in a terminal, set Setup.Mode = auto, or run the extractor's {} command", state, tables, install, command);
                    return false;
                }
                if (!prompt.Confirm(fmt::format("The world database {} {}. Extract them now from your install {}?", state, tables, install)))
                    return false;
            }
            else
                LOG_INFO("server.gameserver", "The world database {} {}, so they are extracted from {}", state, tables, install);
            return true;
        }

        void ExtractServerClasses(ClientSetupResult const& setup, ClientSystem const& system, SetupPrompt& prompt)
        {
            if (!WorldDatabase.IsOpen() || !setup.Install || !setup.TypeDump)
                return;
            TypeDumpLoader::RawDump held;
            std::vector<std::string> problems;
            if (!sObjectSchemaMgr.ReadInstallClasses(held, problems))
            {
                LOG_WARN("server.gameserver", "The classes the world database holds from the install cannot be read, so they are left as they are: {}",
                    problems.empty() ? std::string("no reason was given") : problems.front());
                return;
            }
            std::string const program(Ambrose::Trim(Config().GetOption<std::string>("Setup.SchemaProbe", "", true)));
            ServerClassCacheOptions options;
            options.DataFolder = ClientLocator::GetDataFolder(system);
            options.Program = program.empty() ? ServerClassCache::DefaultProgram(system.GetExecutableDirectory()) : ConfigMgr::PathFromUtf8(program);
            options.Timeout = std::chrono::seconds(Config().GetOption<uint32>("Setup.SchemaProbeTimeout", 3600, true));
            options.Report = [](std::string const& line)
            {
                StartProgress::Report("finding the classes the install holds", ExtractionAllowance);
                LOG_INFO("server.gameserver", "{}", line);
            };
            std::string error;
            TypeDumpLoader::RawDump found;
            if (!held.Classes.empty())
            {
                std::optional<std::filesystem::path> const cached = ServerClassCache::PathFor(options.DataFolder, setup.Install->Revision);
                if (cached && ServerClassCache::IsCurrent(*cached, *setup.TypeDump) && ServerClassCache::Read(*cached, found, error) && ServerClassScript::Matches(held, found))
                    return;
                found = {};
                error.clear();
            }
            if (!ConfirmExtraction(setup, prompt, "classes from the install", "classes", held.Classes.empty() ? "has no" : "holds out-of-date"))
                return;
            StartProgress::Report("finding the classes the install holds", ExtractionAllowance);
            std::optional<std::filesystem::path> const classes = ServerClassCache::Ensure(*setup.Install, *setup.TypeDump, options, error);
            std::optional<MySQLConnectionInfo> const world = MySQLConnectionInfo::Parse(Config().GetOption<std::string>("WorldDatabaseInfo", "", true), &error);
            if (!classes || !ServerClassCache::Read(*classes, found, error) || !world)
            {
                LOG_WARN("server.gameserver", "The classes {} holds that its type dump does not describe are not found, so the server starts with {}: {}", setup.Install->Describe(),
                    held.Classes.empty() ? "none of them" : "the ones the world database held", error);
                return;
            }
            if (!held.Classes.empty() && ServerClassScript::Matches(held, found))
            {
                LOG_INFO("server.gameserver", "The world database already holds the {} class(es) {} holds that its type dump does not describe", found.Classes.size(), setup.Install->Describe());
                return;
            }
            StartProgress::Report("writing the classes the install holds to the world database", WriteAllowance);
            if (!ServerClassScript::Build(found).Apply(*world, error))
            {
                LOG_WARN("server.gameserver", "The classes {} holds cannot be written to the world database, so the server starts with {}: {}", setup.Install->Describe(),
                    held.Classes.empty() ? "none of them" : "the ones it held", error);
                return;
            }
            LOG_INFO("server.gameserver", "{} the {} class(es) {} holds that its type dump does not describe, from {}", held.Classes.empty() ? "Wrote" : "Replaced the classes from the install with",
                found.Classes.size(), setup.Install->Describe(), ConfigMgr::PathToUtf8(*classes));
        }

        bool ExtractNames(ClientSetupResult const& setup, SetupPrompt& prompt)
        {
            if (!ConfirmExtraction(setup, prompt, "character name tables", "names"))
                return false;
            StartProgress::Report("extracting the character name tables", ExtractionAllowance);
            std::string const install = setup.Install->Describe();
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
            StartProgress::Report("writing the character name tables to the world database", WriteAllowance);
            if (!world || !CharacterNameScript::Build(*extraction).Apply(*world, error))
            {
                LOG_ERROR("server.gameserver", "Cannot write the character name tables to the world database: {}", error);
                return false;
            }
            LOG_INFO("server.gameserver", "Extracted {} character name tables holding {} names from {}", extraction->Tables.size(), extraction->GetPartCount(), install);
            return true;
        }

        bool LoadPlayerLevels(ClientSetupResult const& setup, SetupPrompt& prompt)
        {
            PlayerLevelLoadResult levels = sPlayerLevelMgr.Load();
            if (levels.Loaded && levels.Empty && ExtractLevels(setup, prompt))
                levels = sPlayerLevelMgr.Load();
            if (!levels.Loaded)
            {
                for (std::string const& problem : levels.Errors)
                    LOG_ERROR("server.gameserver", "Level and stat tables: {}", problem);
                LOG_ERROR("server.gameserver", "Cannot load the level and stat tables from the world database");
                return false;
            }
            if (levels.Empty)
                LOG_WARN("server.gameserver", "The world database holds no level or stat tables, so wizards have no base health, mana or experience thresholds; run the extractor's levels command against your install");
            else
            {
                LOG_INFO("server.gameserver", "Loaded {} magic schools with level tables for {} of them up to level {}, and {} stat settings with {} band values", levels.Schools, levels.LevelTables, levels.MaxLevel, levels.StatSettings, levels.BandValues);
                if (levels.Levels == 0 || levels.StatSettings == 0)
                    LOG_WARN("server.gameserver", "The world database holds {} but no {}; run the extractor's levels command against your install to fill both", levels.Levels == 0 ? "stat tables" : "level tables", levels.Levels == 0 ? "level tables" : "stat tables");
            }
            return true;
        }

        bool ExtractLevels(ClientSetupResult const& setup, SetupPrompt& prompt)
        {
            if (!ConfirmExtraction(setup, prompt, "level or stat tables", "levels"))
                return false;
            StartProgress::Report("extracting the level and stat tables", ExtractionAllowance);
            std::string const install = setup.Install->Describe();
            std::string error;
            std::optional<LevelExtraction> const extraction = LevelExtractor::ExtractFromInstall(setup.Install->Root, *setup.TypeDump, error);
            if (!extraction)
            {
                LOG_ERROR("server.gameserver", "Cannot extract the level and stat tables: {}", error);
                return false;
            }
            if (!extraction->Ok())
            {
                for (std::string const& problem : extraction->Errors)
                    LOG_ERROR("server.gameserver", "Level extraction: {}", problem);
                return false;
            }
            std::optional<MySQLConnectionInfo> const world = MySQLConnectionInfo::Parse(Config().GetOption<std::string>("WorldDatabaseInfo", "", true), &error);
            StartProgress::Report("writing the level and stat tables to the world database", WriteAllowance);
            if (!world || !LevelScript::Build(*extraction).Apply(*world, error))
            {
                LOG_ERROR("server.gameserver", "Cannot write the level and stat tables to the world database: {}", error);
                return false;
            }
            LOG_INFO("server.gameserver", "Extracted {} level rows for {} schools, {} magic schools and {} stat settings from {}", extraction->Levels.Levels.size(), extraction->SchoolsWithTables.size(),
                extraction->Levels.Schools.size(), extraction->Stats.Settings.size(), install);
            return true;
        }

        bool ExtractZones(ClientSetupResult const& setup, SetupPrompt& prompt)
        {
            if (!ConfirmExtraction(setup, prompt, "zones", "zones"))
                return false;
            StartProgress::Report("extracting the zones", ExtractionAllowance);
            std::string const install = setup.Install->Describe();
            std::string error;
            std::optional<ZoneExtraction> const extraction = ZoneExtractor::ExtractFromInstall(setup.Install->Root, *setup.TypeDump, error,
                [](std::size_t, std::size_t) { StartProgress::Report("extracting the zones", ArchiveAllowance); });
            if (!extraction)
            {
                LOG_ERROR("server.gameserver", "Cannot extract the zones: {}", error);
                return false;
            }
            if (!extraction->Ok())
            {
                for (std::string const& problem : extraction->Errors)
                    LOG_ERROR("server.gameserver", "Zone extraction: {}", problem);
                return false;
            }
            std::optional<MySQLConnectionInfo> const world = MySQLConnectionInfo::Parse(Config().GetOption<std::string>("WorldDatabaseInfo", "", true), &error);
            StartProgress::Report("writing the zones to the world database", WriteAllowance);
            if (!world || !ZoneScript::Build(*extraction).Apply(*world, error))
            {
                LOG_ERROR("server.gameserver", "Cannot write the zones to the world database: {}", error);
                return false;
            }
            LOG_INFO("server.gameserver", "Extracted {} zones with {} named places and {} placed objects from {}, leaving out {} object list entries of classes the type dump does not describe",
                extraction->Zones.size(), extraction->GetLocationCount(), extraction->GetObjectCount(), install, extraction->GetSkippedObjectCount());
            return true;
        }

        void LoadCommands()
        {
            sCommandMgr.SetPrefix(sSettings.Get<std::string>("GM.CommandPrefix"));
            sCommandMgr.SetLogging(sSettings.Get<bool>("GM.LogCommands"));
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
                        std::vector<std::string> words = CommandMgr::Split(name);
                        words.insert(words.end(), arguments.begin(), arguments.end());
                        ConsoleCaller caller([&reply](std::string_view text) { reply(text); });
                        return sCommandMgr.Execute(caller, std::move(words)) != CommandResult::Empty;
                    } });
            }
        }

        void ApplySetting(SettingChange const& change)
        {
            std::string_view const key = change.Key;
            if (key == "GM.CommandPrefix")
                sCommandMgr.SetPrefix(sSettings.Get<std::string>("GM.CommandPrefix"));
            else if (key == "GM.LogCommands")
                sCommandMgr.SetLogging(sSettings.Get<bool>("GM.LogCommands"));
            else if (key == "Locale.Default")
            {
                std::string const locale = sSettings.Get<std::string>("Locale.Default");
                std::string error;
                if (sLocaleStore.IsLoaded() && !sLocaleStore.SetDefaultLocale(locale, error))
                {
                    LOG_WARN("server.gameserver", "Locale.Default {} cannot be used, so names and text stay in {}: {}", locale, sLocaleStore.GetDefaultLocale(), error);
                    return;
                }
                sCharacterNameMgr.SetDefaultLocale(locale);
            }
            else if (key == "Templates.CacheSize")
                sObjectTemplateMgr.SetBudget(std::size_t{ sSettings.Get<uint32>("Templates.CacheSize") } << 20);
            else if (key == "Realm.Name" || key == "Realm.Address" || key == "PublicAddress" || key == "Realm.HeartbeatInterval")
                _heartbeat.Reconfigure(RealmHeartbeatSettings::Load(Config()));
            else if (_context && (key.starts_with("Network.") || key == "Attach.Timeout"))
            {
                std::vector<std::string> problems;
                _context->SetSettings(SessionSettings::Load(Config(), &problems));
                for (std::string const& problem : problems)
                    LOG_WARN("server.gameserver", "{}", problem);
                if (_sockets && key.starts_with("Network."))
                {
                    problems.clear();
                    NetworkSettings const settings = NetworkSettings::Load(Config(), "WorldServerPort", DefaultWorldPort, &problems);
                    std::string error;
                    if (!_sockets->ApplySettings(settings, error))
                        LOG_WARN("server.gameserver", "Cannot apply network settings: {}", error);
                    for (std::string const& problem : problems)
                        LOG_WARN("server.gameserver", "{}", problem);
                }
            }
        }

        uint8 GetSettingApps() const override
        {
            return SettingApps::Game;
        }

        void OnStop() override
        {
            if (_settingsSubscription != 0)
                sSettings.Unsubscribe(_settingsSubscription);
            _settingsSubscription = 0;
            sStats.Unpublish("sessions");
            sStats.Unpublish("realm_beating");
            _heartbeat.Stop();
            if (_sockets)
                _sockets->StopNetwork();
            _sockets.reset();
            for (std::shared_ptr<GameSession> const& session : sWorld.GetSessions())
                session->LeaveWorld();
            sWorld.Clear();
            sCommandMgr.Clear();
            sScriptMgr.OnShutdown();
            sScriptMgr.Unload();
            AppenderDB::Disable(Logger());
            _databases.Close();
        }

        std::chrono::milliseconds GetUpdateInterval() const override
        {
            return std::chrono::milliseconds(sSettings.Get<uint32>("World.UpdateInterval"));
        }

        void OnUpdate(std::chrono::milliseconds diff) override
        {
            sWorld.Update(diff);
            _heartbeat.Update(diff);
        }

    private:
        uint64 _settingsSubscription = 0;
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
