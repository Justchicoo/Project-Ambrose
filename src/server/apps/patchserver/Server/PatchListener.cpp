/*
 * Project Ambrose by Imjustchico
 * Starts and stops the patch server's socket manager from config, logs each problem the network and session options have, and hands a changed configuration to the session context and the socket manager, whose rebind keeps the old listener serving when the new address cannot be bound.
 */

#include "PatchListener.h"
#include "Log.h"
#include "NetworkSettings.h"
#include "SessionSettings.h"

#include <vector>

PatchListener::~PatchListener()
{
    Stop();
}

bool PatchListener::Start(ConfigMgr const& config, std::string& error)
{
    if (_sockets)
    {
        error = "the patch listener is already running";
        return false;
    }
    std::vector<std::string> problems;
    _context = std::make_shared<SessionContext>(SessionSettings::Load(config, &problems));
    NetworkSettings const network = NetworkSettings::Load(config, std::string(PortOption), DefaultPort, &problems);
    for (std::string const& problem : problems)
        LOG_WARN("server.patchserver", "{}", problem);
    auto sockets = std::make_unique<SocketMgr<PatchSession>>([context = _context](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
    {
        return std::make_shared<PatchSession>(std::move(socket), limits, context);
    });
    if (!sockets->StartNetwork(network, error))
    {
        _context.reset();
        return false;
    }
    _sockets = std::move(sockets);
    return true;
}

bool PatchListener::Apply(ConfigMgr const& config, std::string& error)
{
    if (!_sockets)
    {
        error = "the patch listener is not running";
        return false;
    }
    std::vector<std::string> problems;
    _context->SetSettings(SessionSettings::Load(config, &problems));
    NetworkSettings const network = NetworkSettings::Load(config, std::string(PortOption), DefaultPort, &problems);
    for (std::string const& problem : problems)
        LOG_WARN("server.patchserver", "{}", problem);
    return _sockets->ApplySettings(network, error);
}

void PatchListener::Stop()
{
    if (_sockets)
        _sockets->StopNetwork();
    _sockets.reset();
    _context.reset();
}

bool PatchListener::IsListenerOption(std::string_view key) noexcept
{
    return key == "BindIP" || key == PortOption || key.starts_with("Network.");
}

std::string PatchListener::GetBindIp() const
{
    return _sockets ? _sockets->GetSettings().BindIp : std::string();
}

uint16 PatchListener::GetPort() const
{
    return _sockets ? _sockets->GetPort() : uint16{ 0 };
}

std::size_t PatchListener::GetConnectionCount() const
{
    return _sockets ? _sockets->GetConnectionCount() : 0;
}
