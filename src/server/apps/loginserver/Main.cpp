/*
 * Project Ambrose by Imjustchico
 * Login server entry point: loads account and login settings and the type dump, declares the login message table and checks it against the client's message definitions, refuses to serve clients without the type dump and both databases, opens the login and characters databases, listens for clients, and offers account console commands until shutdown, telling connected clients before it shuts down and closing the databases, which drains their callbacks, before its network threads stop.
 */

#include "AccountCommands.h"
#include "AccountMgr.h"
#include "AppenderDB.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Log.h"
#include "LogConfig.h"
#include "LoginMessageTable.h"
#include "LoginMgr.h"
#include "LoginShutdown.h"
#include "LoginSession.h"
#include "MessageRegistry.h"
#include "NetworkSettings.h"
#include "ObjectSerializer.h"
#include "ServerApp.h"
#include "SessionContext.h"
#include "SocketMgr.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <iostream>
#include <memory>
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

            std::string const clientDir = Config().GetOption<std::string>("ClientDir", "", true);
            if (!clientDir.empty())
            {
                std::vector<std::string_view> missing;
                for (std::string_view const option : { "TypeDumpPath", "LoginDatabaseInfo", "CharacterDatabaseInfo" })
                    if (Config().GetOption<std::string>(std::string(option), "", true).empty())
                        missing.push_back(option);
                if (!missing.empty())
                {
                    LOG_ERROR("server.loginserver", "ClientDir is set, so clients will be served, but {} {} empty; the login server needs the type dump and both databases to authenticate clients and list their characters",
                        fmt::join(missing, ", "), missing.size() == 1 ? "is" : "are");
                    return false;
                }
            }
            if (clientDir.empty())
                LOG_WARN("server.loginserver", "ClientDir is not set, so client messages are logged by service and order only");
            else if (!sMessageRegistry.LoadFromClient(LogConfig::Utf8Path(clientDir)))
            {
                LOG_ERROR("server.loginserver", "Cannot load the message definitions from the client in {}", clientDir);
                return false;
            }
            else if (!messages.Validate(*sMessageRegistry.GetCatalog(), messageErrors))
            {
                for (std::string const& error : messageErrors)
                    LOG_ERROR("server.loginserver", "{}", error);
                LOG_ERROR("server.loginserver", "The login message table does not match the client's message definitions in {}", clientDir);
                return false;
            }

            std::vector<std::string> limitProblems;
            SerializerLimits::Apply(SerializerLimits::Load(Config(), &limitProblems));
            for (std::string const& problem : limitProblems)
                LOG_WARN("server.loginserver", "{}", problem);

            std::string const typeDump = Config().GetOption<std::string>("TypeDumpPath", "", true);
            if (typeDump.empty())
                LOG_WARN("server.loginserver", "TypeDumpPath is not set, so ObjectProperty data cannot be read or written");
            else if (!sTypeRegistry.LoadFromFile(LogConfig::Utf8Path(typeDump)))
            {
                LOG_ERROR("server.loginserver", "Cannot load the type dump {}", typeDump);
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
    return app.Run(std::vector<std::string>(argv, argv + argc));
}
