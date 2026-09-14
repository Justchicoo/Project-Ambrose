/*
 * Project Ambrose by Imjustchico
 * Login server entry point: loads the client's message definitions, listens for clients, and runs the session handshake until a shutdown signal.
 */

#include "ConfigMgr.h"
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
            std::string const clientDir = Config().GetOption<std::string>("ClientDir", "", true);
            if (clientDir.empty())
                LOG_WARN("server.loginserver", "ClientDir is not set, so client messages are logged by service and order only");
            else if (!sMessageRegistry.LoadFromClient(LogConfig::Utf8Path(clientDir)))
            {
                LOG_ERROR("server.loginserver", "Cannot load the message definitions from the client in {}", clientDir);
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
                return false;
            }
            return true;
        }

        void OnStop() override
        {
            if (_sockets)
                _sockets->StopNetwork();
            _sockets.reset();
        }

    private:
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<LoginSession>> _sockets;
    };
}

int main(int argc, char** argv)
{
    LoginServerApp app;
    return app.Run(std::vector<std::string>(argv, argv + argc));
}
