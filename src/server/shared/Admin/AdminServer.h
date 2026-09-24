/*
 * Project Ambrose by Imjustchico
 * The optional admin API listener every operations feature builds on: it binds only where the remote-access rule allows, keeps a generated token in the data folder or, where the machine names none, beside the config file, holds the route table, the bearer token and the failure limiter, answers the same 401 on every path and every method without it, serves GET /api/health, serves the built panel at / without a token and lets it sign in by trading the token once for a browser session, and takes the WebSocket routes later milestones register, before or after it opens.
 */

#ifndef AMBROSE_ADMINSERVER_H
#define AMBROSE_ADMINSERVER_H

#include "AdminAuth.h"
#include "AdminFiles.h"
#include "AdminRouter.h"
#include "AdminSessions.h"
#include "ListenerSettings.h"
#include "Log.h"
#include "Types.h"

#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>


struct AdminHealth
{
    std::string App;
    std::string Realm;
    std::string Revision;
    uint64 UptimeSeconds = 0;
    std::string State;
};

class AdminSocket
{
public:
    virtual ~AdminSocket() = default;

    virtual void SendText(std::string text) = 0;
    virtual void Close(std::string reason) = 0;
    virtual std::string GetRemoteAddress() = 0;
    virtual std::shared_ptr<AdminSocket> Keep() { return {}; }
};

struct AdminSocketRoute
{
    std::string Path;
    std::function<void(AdminSocket&)> Opened;
    std::function<void(AdminSocket&, std::string const&, bool)> Received;
    std::function<void(AdminSocket&, std::string const&, uint16)> Closed;
};

class AdminServer
{
public:
    AdminServer(Log& log, std::string appName, std::filesystem::path dataFolder, std::filesystem::path configFolder = {});
    ~AdminServer();

    AdminServer(AdminServer const&) = delete;
    AdminServer& operator=(AdminServer const&) = delete;

    void SetHealthSource(std::function<AdminHealth()> health);
    void SetSessionSource(SessionSource* source);
    std::string MakeSessionCookie(std::string const& value, bool clear) const { return SessionCookie(value, clear); }
    void AddSocket(AdminSocketRoute route);
    AdminRouter& Routes() { return _router; }

    bool Start(ListenerSettings const& settings, std::string& error);
    bool Reload(ListenerSettings const& settings);
    void Stop();

    bool IsRunning() const;
    uint16 GetPort() const;
    std::string GetBindIp() const;
    std::string GetToken() const;

private:
    struct Listener;

    template<typename... Args>
    void LogPanelOrAdmin(LogLevel level, fmt::format_string<Args...> format, Args&&... args) const
    {
        _log.Write(std::string_view(_active.LogCategory), level, format, std::forward<Args>(args)...);
    }

    void AdoptIdentity(ListenerSettings const& settings);
    std::string_view Label() const { return _active.Label; }
    std::string Capitalised() const;

    bool Open(ListenerSettings const& settings, std::string const& token, std::string& error);
    bool SwapCertificate();
    void Close();
    AdminSocketRoute const* FindSocket(std::string const& path) const;
    void ApplyLiveSettings(ListenerSettings const& settings);
    AdminResponse SignIn(AdminRequest const& request);
    std::string SessionCookie(std::string const& value, bool clear) const;

    Log& _log;
    std::string _appName;
    std::filesystem::path _dataFolder;
    std::filesystem::path _configFolder;
    AdminAuth _auth;
    AdminSessions _sessions;
    SessionSource* _sessionSource = nullptr;
    AdminFiles _files;
    AdminRouter _router;
    std::function<AdminHealth()> _health;
    mutable std::mutex _socketMutex;
    std::deque<AdminSocketRoute> _sockets;
    std::unique_ptr<Listener> _listener;
    ListenerSettings _active;
    std::string _token;
};

#endif
