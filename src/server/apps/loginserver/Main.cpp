/*
 * Project Ambrose by Imjustchico
 * Login server entry point: loads account settings and the client's message definitions, opens the login database, listens for clients, runs the session handshake, and offers account console commands until shutdown.
 */

#include "AccountCommands.h"
#include "AccountMgr.h"
#include "AppenderDB.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Log.h"
#include "LogConfig.h"
#include "LoginSession.h"
#include "MessageRegistry.h"
#include "NetworkSettings.h"
#include "ServerApp.h"
#include "SessionContext.h"
#include "SocketMgr.h"

#include <iostream>
#include <memory>
#include <string>
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

            std::string const clientDir = Config().GetOption<std::string>("ClientDir", "", true);
            if (clientDir.empty())
                LOG_WARN("server.loginserver", "ClientDir is not set, so client messages are logged by service and order only");
            else if (!sMessageRegistry.LoadFromClient(LogConfig::Utf8Path(clientDir)))
            {
                LOG_ERROR("server.loginserver", "Cannot load the message definitions from the client in {}", clientDir);
                return false;
            }

            _databases = std::make_unique<DatabaseLoader>(Config());
            _databases->AddDatabase(LoginDatabase, "Login", DatabaseLoader::DATABASE_LOGIN);
            if (!_databases->Load())
            {
                LOG_ERROR("server.loginserver", "Cannot open the login database");
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
                _sockets->StopNetwork();
            _sockets.reset();
            AppenderDB::Disable(Logger());
            if (_databases)
                _databases->Close();
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
