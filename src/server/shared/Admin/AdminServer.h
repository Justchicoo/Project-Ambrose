/*
 * Project Ambrose by Imjustchico
 * The optional admin API listener every operations feature builds on: it binds only where the remote-access rule allows, keeps a generated token in the data folder or, where the machine names none, beside the config file, holds the route table, the bearer token and the failure limiter, answers the same 401 on every path and every method without it, serves GET /api/health, and takes the WebSocket routes later milestones register, before or after it opens.
 */

#ifndef AMBROSE_ADMINSERVER_H
#define AMBROSE_ADMINSERVER_H

#include "AdminAuth.h"
#include "AdminRouter.h"
#include "AdminSettings.h"
#include "Types.h"

#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

class Log;

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
    void AddSocket(AdminSocketRoute route);
    AdminRouter& Routes() { return _router; }

    bool Start(AdminSettings const& settings, std::string& error);
    bool Reload(AdminSettings const& settings);
    void Stop();

    bool IsRunning() const;
    uint16 GetPort() const;
    std::string GetBindIp() const;
    std::string GetToken() const;

private:
    struct Listener;

    bool Open(AdminSettings const& settings, std::string const& token, std::string& error);
    void Close();
    AdminSocketRoute const* FindSocket(std::string const& path) const;

    Log& _log;
    std::string _appName;
    std::filesystem::path _dataFolder;
    std::filesystem::path _configFolder;
    AdminAuth _auth;
    AdminRouter _router;
    std::function<AdminHealth()> _health;
    mutable std::mutex _socketMutex;
    std::deque<AdminSocketRoute> _sockets;
    std::unique_ptr<Listener> _listener;
    AdminSettings _active;
    std::string _token;
};

#endif
