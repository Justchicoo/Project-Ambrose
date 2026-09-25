/*
 * Project Ambrose by Imjustchico
 * Login server entry point: runs setup in Setup.Mode for the install and type dump, stopping cleanly when a stop arrives meanwhile and never saving an install it has no type dump for, loads account and login settings and the type dump, declares the login message table and checks it against the client's message definitions, refuses to serve clients from an install without a type dump, naming why and where ClientDir came from, or without both databases, opens the login and characters databases, which the admin API reports, lists the updates of and applies data-only updates to while the server runs, listens for clients, and offers account console commands until shutdown, telling connected clients before it shuts down and closing the databases, which drains their callbacks, before its network threads stop.
 */

#include "CharacterCreateStore.h"
#include "CharacterNameMgr.h"
#include "CharacterRepository.h"
#include "TypeDumpCache.h"
#include "RealmLoader.h"
#include "AccountCommands.h"
#include "AccountMgr.h"
#include "AppenderDB.h"
#include "ClientLocator.h"
#include "ClientSetup.h"
#include "StatsRegistry.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "AdminDatabaseView.h"
#include "AdminServer.h"
#include "DatabaseLoader.h"
#include "Environment.h"
#include "Log.h"
#include "LoginMessageTable.h"
#include "LoginMgr.h"
#include "LoginSession.h"
#include "LoginShutdown.h"
#include "MessageRegistry.h"
#include "NetworkSettings.h"
#include "OnlinePlayersView.h"
#include "ObjectSchemaMgr.h"
#include "ObjectSerializer.h"
#include "ServerApp.h"
#include "SessionContext.h"
#include "SocketMgr.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    int64 CountOutstandingKeys()
    {
        if (!LoginDatabase.IsOpen())
            return -1;
        QueryResult const result = LoginDatabase.Query(fmt::format("SELECT COUNT(*) FROM `login_key` WHERE `used` = 0 AND `expires` > {}",
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
        return result ? static_cast<int64>((*result)[0].Get<uint64>()) : -1;
    }

    class LoginServerApp : public ServerApp
    {
    public:
        static constexpr uint16 DefaultPort = 12000;
        static constexpr std::chrono::milliseconds RealmTick{ 1000 };

        LoginServerApp() : ServerApp({ "loginserver", "loginserver.conf", 12010 }, sConfigMgr, sLog, std::cout, std::cerr), _databases(Config()), _databaseView(_databases)
        {
            _databases.AddDatabase(LoginDatabase, "Login", DatabaseLoader::DATABASE_LOGIN)
                .AddDatabase(CharacterDatabase, "Character", DatabaseLoader::DATABASE_CHARACTER)
                .AddDatabase(WorldDatabase, "World", DatabaseLoader::DATABASE_WORLD);
            _databaseView.AddStore("character names", WorldDatabase.GetName(), []
            {
                CharacterNameLoadResult const names = sCharacterNameMgr.Load();
                if (names.Loaded)
                    LOG_INFO("server.loginserver", "Reloaded {} character name tables holding {} names in {} locales, and {} disallowed names", names.Tables, names.Parts, names.HumanLocales, names.Disallowed);
                else
                    for (std::string const& problem : names.Errors)
                        LOG_ERROR("server.loginserver", "Character name tables were not reloaded, and the loaded ones stay in use: {}", problem);
                for (std::string const& warning : names.Warnings)
                    LOG_WARN("server.loginserver", "Character name tables: {}", warning);
                return AdminStoreReload{ names.Loaded, names.Errors, names.Warnings };
            });
            _databaseView.AddStore("character creation", WorldDatabase.GetName(), []
            {
                CharacterCreateLoadResult const rows = sCharacterCreateStore.Load();
                if (rows.Loaded)
                    LOG_INFO("server.loginserver", "Reloaded {} schools a wizard may be given and {} starting states", rows.Schools, rows.Starts);
                else
                    for (std::string const& problem : rows.Errors)
                        LOG_ERROR("server.loginserver", "The creation rows were not reloaded, and the loaded ones stay in use: {}", problem);
                for (std::string const& warning : rows.Warnings)
                    LOG_WARN("server.loginserver", "Character creation: {}", warning);
                return AdminStoreReload{ rows.Loaded, rows.Errors, rows.Warnings };
            });
        }

    protected:
        void OnAdminApiReady(AdminServer& admin) override
        {
            _databaseView.Register(admin.Routes());
            OnlinePlayersView::Register(admin.Routes());
        }

        bool LoadCreationRows()
        {
            sCharacterNameMgr.SetDefaultLocale(Config().GetOption<std::string>("Locale.Default", "en-US", true));
            if (!WorldDatabase.IsOpen())
            {
                LOG_WARN("server.loginserver", "WorldDatabaseInfo is empty, so no wizard can be created until it names a world database");
                return true;
            }

            CharacterNameLoadResult const names = sCharacterNameMgr.Load();
            for (std::string const& problem : names.Errors)
                LOG_ERROR("server.loginserver", "Character name tables: {}", problem);
            for (std::string const& warning : names.Warnings)
                LOG_WARN("server.loginserver", "Character name tables: {}", warning);
            if (!names.Loaded)
            {
                LOG_ERROR("server.loginserver", "Cannot load the character name tables from the world database");
                return false;
            }

            CharacterCreateLoadResult const rows = sCharacterCreateStore.Load();
            for (std::string const& problem : rows.Errors)
                LOG_ERROR("server.loginserver", "Character creation: {}", problem);
            for (std::string const& warning : rows.Warnings)
                LOG_WARN("server.loginserver", "Character creation: {}", warning);
            if (!rows.Loaded)
            {
                LOG_ERROR("server.loginserver", "Cannot load the schools and starting states a wizard is made from");
                return false;
            }

            std::optional<uint64> const highest = CharacterRepository::GetMaxGuid();
            if (!highest)
            {
                LOG_ERROR("server.loginserver", "Cannot read the highest character id ever used, so no wizard can be given one safely");
                return false;
            }
            sLoginMgr.ResumeCharacterGuids(*highest);
            LOG_INFO("server.loginserver", "Wizards can be created from {} schools and {} starting states, with the next character id {}",
                rows.Schools, rows.Starts, sLoginMgr.PeekCharacterGuid().value_or(0));
            return true;
        }

        std::vector<RestartRequiredOption> GetRestartRequiredOptions() const override
        {
            std::vector<RestartRequiredOption> options(ClientSetup::RestartRequiredOptions.begin(), ClientSetup::RestartRequiredOptions.end());
            options.insert(options.end(), DatabaseLoader::RestartRequiredOptions.begin(), DatabaseLoader::RestartRequiredOptions.end());
            return options;
        }

        bool OnStart() override
        {
            LocalClientSystem const system;
            ClientSetup::Report const report = [](bool warning, std::string const& text)
            {
                if (warning)
                    LOG_WARN("server.loginserver", "{}", text);
                else
                    LOG_INFO("server.loginserver", "{}", text);
            };
            std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ServerPrompt(std::cout, Config());
            prompt->SetCancellation([this] { return PollStopRequested(); });
            ClientSetupResult const setup = ClientSetup::ForServer(Config(), *prompt, system, ClientSetup::ServerTypeDumps(Config(), system, report, [this] { return PollStopRequested(); }),
                { "loginserver", true, true, true }, report);
            SetClientSetup(setup);
            if (PollStopRequested())
                return false;

            if (!sAccountMgr.LoadSettings(Config()))
            {
                LOG_ERROR("server.loginserver", "Cannot load the account settings");
                return false;
            }
            sLoginMgr.LoadSettings(Config());

            MessageHandlerTable<LoginSession> const& messages = LoginMessageTable::Get();
            std::vector<std::string> messageErrors;
            if (!messages.Declare(sMessageRegistry, messageErrors))
            {
                for (std::string const& error : messageErrors)
                    LOG_ERROR("server.loginserver", "{}", error);
                LOG_ERROR("server.loginserver", "The login message table does not match the loaded message definitions");
                return false;
            }

            if (setup.Install)
            {
                std::string const install = ClientLocator::PathText(setup.Install->Root);
                std::optional<ConfigEntry> const configured = Config().Resolve(std::string(ClientSetup::ClientDirKey));
                std::string from;
                if (configured && ClientSetup::ConfigPath(ConfigMgr::PathFromUtf8(Ambrose::Trim(configured->Value))) == ClientSetup::ConfigPath(setup.Install->Root))
                {
                    if (configured->Kind == ConfigSourceKind::Environment)
                        from = fmt::format(" (ClientDir from the environment variable {})", ClientLocator::PathText(configured->File));
                    else if (configured->Kind == ConfigSourceKind::Override)
                        from = " (ClientDir from the command line)";
                    else if (!configured->File.empty())
                        from = fmt::format(" (ClientDir in {})", ClientLocator::PathText(configured->File));
                }
                if (!setup.TypeDump)
                {
                    LOG_ERROR("server.loginserver", "The login server uses the install {}{}, so clients will be served, but it has no type dump: {}; it needs the type dump to authenticate clients and list their characters",
                        install, from, setup.TypeDumpError);
                    return false;
                }
                std::vector<std::string_view> missing;
                for (std::string_view const option : { "LoginDatabaseInfo", "CharacterDatabaseInfo" })
                    if (Config().GetOption<std::string>(std::string(option), "", true).empty())
                        missing.push_back(option);
                if (!missing.empty())
                {
                    LOG_ERROR("server.loginserver", "The login server uses the install {}{}, so clients will be served, but {} {} empty; it needs both databases to authenticate clients and list their characters",
                        install, from, fmt::join(missing, ", "), missing.size() == 1 ? "is" : "are");
                    return false;
                }
            }
            if (!setup.Install)
                LOG_WARN("server.loginserver", "No Wizard101 install is in use, so client messages are logged by service and order only");
            else if (!sMessageRegistry.LoadFromClient(setup.Install->Root))
            {
                LOG_ERROR("server.loginserver", "Cannot load the message definitions from the client in {}", ClientLocator::PathText(setup.Install->Root));
                return false;
            }
            else if (!messages.Validate(*sMessageRegistry.GetCatalog(), messageErrors))
            {
                for (std::string const& error : messageErrors)
                    LOG_ERROR("server.loginserver", "{}", error);
                LOG_ERROR("server.loginserver", "The login message table does not match the client's message definitions in {}", ClientLocator::PathText(setup.Install->Root));
                return false;
            }
            else
                SetMessageSource(setup.Install->Root);

            std::vector<std::string> limitProblems;
            SerializerLimits::Apply(SerializerLimits::Load(Config(), &limitProblems));
            for (std::string const& problem : limitProblems)
                LOG_WARN("server.loginserver", "{}", problem);

            if (!_databases.Load())
            {
                LOG_ERROR("server.loginserver", "Cannot open the login, characters and world databases");
                return false;
            }
            if (std::vector<std::string> errors; WorldDatabase.IsOpen() && !sObjectSchemaMgr.LoadClasses(errors))
            {
                for (std::string const& problem : errors)
                    LOG_ERROR("server.loginserver", "Server classes: {}", problem);
                LOG_ERROR("server.loginserver", "Cannot load the classes the type dump does not describe from the world database");
                _databases.Close();
                return false;
            }

            if (!setup.TypeDump)
                LOG_WARN("server.loginserver", "No type dump is in use, so ObjectProperty data cannot be read or written: {}", setup.TypeDumpError);
            else
            {
                std::filesystem::path const binary = TypeDumpCache::FastCopyOf(*setup.TypeDump);
                if (std::string fastCopyError; !TypeDumpCache::EnsureFastCopy(*setup.TypeDump, fastCopyError))
                    LOG_WARN("server.loginserver", "The type dump's fast copy could not be built, so it is read from JSON this time: {}", fastCopyError);
                bool loaded = false;
                if (std::filesystem::exists(binary))
                    loaded = sTypeRegistry.LoadBinary(binary, *setup.TypeDump, setup.Install ? setup.Install->Revision : std::string_view{});
                else
                    loaded = sTypeRegistry.LoadFromFile(*setup.TypeDump);
                if (!loaded)
                {
                    LOG_ERROR("server.loginserver", "Cannot load the type dump {}", ConfigMgr::PathToUtf8(*setup.TypeDump));
                    _databases.Close();
                    return false;
                }
            }
            sObjectSchemaMgr.RegisterClassReloadTarget();
            AppenderDB::Enable(Logger(), 0);
            if (!LoadCreationRows())
            {
                AppenderDB::Disable(Logger());
                _databases.Close();
                return false;
            }

            std::vector<std::string> problems;
            _context = std::make_shared<SessionContext>(SessionSettings::Load(Config(), &problems));
            NetworkSettings const network = NetworkSettings::Load(Config(), "LoginServerPort", DefaultPort, &problems);
            for (std::string const& problem : problems)
                LOG_WARN("server.loginserver", "{}", problem);

            _sockets = std::make_unique<SocketMgr<LoginSession>>([context = _context](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
            {
                return std::make_shared<LoginSession>(std::move(socket), limits, context);
            });
            std::string error;
            if (!_sockets->StartNetwork(network, error))
            {
                LOG_ERROR("server.loginserver", "Cannot listen for clients: {}", error);
                _sockets.reset();
                AppenderDB::Disable(Logger());
                _databases.Close();
                return false;
            }
            SetListener(network.BindIp, _sockets->GetPort());
            SetClientSetup(setup.Install.has_value(), setup.TypeDump.has_value(), false, setup.TypeDumpError);
            sStats.Publish("sessions", [this] { return Ambrose::StatValue(static_cast<int64>(_sockets ? _sockets->GetConnectionCount() : 0)); });
            sStats.Publish("realms", [] { return Ambrose::StatValue(static_cast<int64>(sRealmList.All().size())); });
            sStats.Publish("realms_online", [] { return Ambrose::StatValue(static_cast<int64>(sRealmList.Online(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()).size())); });
            sStats.Publish("keys_outstanding", [] { return Ambrose::StatValue(CountOutstandingKeys()); });
            AccountCommands::Register(Commands());
            _realms.Configure(RealmLoaderSettings::Load(Config()));
            return true;
        }

        std::chrono::milliseconds GetUpdateInterval() const override
        {
            return RealmTick;
        }

        void OnUpdate(std::chrono::milliseconds diff) override
        {
            _realms.Update(diff);
        }

        void OnStatus(std::vector<std::pair<std::string, std::string>>& fields) override
        {
            fields.emplace_back("sessions", fmt::format("{}", _sockets ? _sockets->GetConnectionCount() : 0));
        }

        void OnStop() override
        {
            sStats.Unpublish("sessions");
            sStats.Unpublish("realms");
            sStats.Unpublish("realms_online");
            sStats.Unpublish("keys_outstanding");
            AccountCommands::Unregister(Commands());
            if (_sockets)
                LoginShutdown::NotifyAndDrain(*_sockets, sLoginMgr.GetSettings()->ShutdownGrace);
            AppenderDB::Disable(Logger());
            _databases.Close();
            if (_sockets)
                _sockets->StopNetwork();
            _sockets.reset();
        }

    private:
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<LoginSession>> _sockets;
        RealmLoader _realms;
        DatabaseLoader _databases;
        AdminDatabaseView _databaseView;
    };
}

int main(int argc, char** argv)
{
    LoginServerApp app;
    return app.Run(Ambrose::GetArguments(argc, argv));
}
