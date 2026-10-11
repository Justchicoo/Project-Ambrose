/*
 * Project Ambrose by Imjustchico
 * The patch server's TCP service 8 listener: opens on BindIP and PatchServerPort with patch sessions sharing one session context, and applies a changed configuration live when it touches the listener's address, port or network options, moving to a new address or port only once it is bound there and keeping the old listener when the bind fails.
 */

#ifndef AMBROSE_PATCHLISTENER_H
#define AMBROSE_PATCHLISTENER_H

#include "PatchSession.h"
#include "SessionContext.h"
#include "SocketMgr.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

class ConfigMgr;

class PatchListener
{
public:
    static constexpr uint16 DefaultPort = 12500;
    static constexpr std::string_view PortOption = "PatchServerPort";

    PatchListener() = default;
    ~PatchListener();
    PatchListener(PatchListener const&) = delete;
    PatchListener& operator=(PatchListener const&) = delete;

    bool Start(ConfigMgr const& config, std::string& error);
    bool Apply(ConfigMgr const& config, std::string& error);
    void Stop();
    static bool IsListenerOption(std::string_view key) noexcept;

    bool IsRunning() const noexcept { return _sockets != nullptr; }
    std::string GetBindIp() const;
    uint16 GetPort() const;
    std::size_t GetConnectionCount() const;

private:
    std::shared_ptr<SessionContext> _context;
    std::unique_ptr<SocketMgr<PatchSession>> _sockets;
};

#endif
