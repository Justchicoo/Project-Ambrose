/*
 * Project Ambrose by Imjustchico
 * Login server entry point: runs setup in Setup.Mode for the install and type dump, stopping cleanly when a stop arrives meanwhile and never saving an install it has no type dump for, loads account and login settings and the type dump, declares the login message table and checks it against the client's message definitions, refuses to serve clients from an install without a type dump, naming why and where ClientDir came from, or without both databases, opens the login and characters databases, listens for clients, and offers account console commands until shutdown, telling connected clients before it shuts down and closing the databases, which drains their callbacks, before its network threads stop.
 */

#include "AccountCommands.h"
#include "AccountMgr.h"
#include "AppenderDB.h"
#include "ClientLocator.h"
#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Environment.h"
#include "Log.h"
#include "LoginMessageTable.h"
#include "LoginMgr.h"
#include "LoginSession.h"
#include "LoginShutdown.h"
#include "MessageRegistry.h"
#include "NetworkSettings.h"
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
    class LoginServerApp : public ServerApp
    {
    public:
        static constexpr uint16 DefaultPort = 12000;

        LoginServerApp() : ServerApp({ "loginserver", "loginserver.conf" }, sConfigMgr, sLog, std::cout, std::cerr)
        {
        }

    protected:
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

            std::vector<std::string> limitProblems;
            SerializerLimits::Apply(SerializerLimits::Load(Config(), &limitProblems));
            for (std::string const& problem : limitProblems)
                LOG_WARN("server.loginserver", "{}", problem);

            if (!setup.TypeDump)
                LOG_WARN("server.loginserver", "No type dump is in use, so ObjectProperty data cannot be read or written: {}", setup.TypeDumpError);
            else if (!sTypeRegistry.LoadFromFile(*setup.TypeDump))
            {
                LOG_ERROR("server.loginserver", "Cannot load the type dump {}", ConfigMgr::PathToUtf8(*setup.TypeDump));
                return false;
            }

            _databases = std::make_unique<DatabaseLoader>(Config());
            _databases->AddDatabase(LoginDatabase, "Login", DatabaseLoader::DATABASE_LOGIN)
                .AddDatabase(CharacterDatabase, "Character", DatabaseLoader::DATABASE_CHARACTER);
            if (!_databases->Load())
            {
                LOG_ERROR("server.loginserver", "Cannot open the login and characters databases");
                _databases.reset();
                return false;
            }
            AppenderDB::Enable(Logger(), 0);

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
                _databases->Close();
                _databases.reset();
                return false;
            }
            AccountCommands::Register(Commands());
            return true;
        }

        void OnStop() override
        {
            AccountCommands::Unregister(Commands());
            if (_sockets)
                LoginShutdown::NotifyAndDrain(*_sockets, sLoginMgr.GetSettings()->ShutdownGrace);
            AppenderDB::Disable(Logger());
            if (_databases)
                _databases->Close();
            if (_sockets)
                _sockets->StopNetwork();
            _sockets.reset();
            _databases.reset();
        }

    private:
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<LoginSession>> _sockets;
        std::unique_ptr<DatabaseLoader> _databases;
    };
}

int main(int argc, char** argv)
{
    LoginServerApp app;
    return app.Run(Ambrose::GetArguments(argc, argv));
}
