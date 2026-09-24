/*
 * Project Ambrose by Imjustchico
 * The admin API's route table and front door: every request gets a request id that its answer and any error body carry, keeping one a caller such as the supervisor sent when it has the same form, so one id names the request in both logs, a host that is no IP address, localhost or a name the operator allows is refused so a page elsewhere cannot rebind a name onto this listener, paths outside /api go to the panel's files without a token, public routes such as signing in run without one, a route may answer every path under a prefix when no exact route claims it, the longest prefix first, and every other path needs the bearer token or a browser session whose unsafe requests and socket upgrades name this listener's own origin and carry the session's CSRF token, with every answer stamped with the panel's security headers and every error handed to a log.
 */

#ifndef AMBROSE_ADMINROUTER_H
#define AMBROSE_ADMINROUTER_H

#include "AdminAuth.h"
#include "AdminSessions.h"
#include "TrustedProxies.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


struct AdminRequest
{
    std::string Method;
    std::string Path;
    std::string RemoteAddress;
    std::string Authorization;
    std::string Body;
    std::string Host;
    std::string Origin;
    std::string UserAgent;
    std::string Cookie;
    std::string Csrf;
    bool Upgrade = false;
    std::string Id;
    std::string Principal;
    std::optional<std::string> SessionCsrf;
};

struct AdminResponse
{
    int Status = 200;
    std::string ContentType = "application/json";
    std::string Body;
    std::vector<std::pair<std::string, std::string>> Headers;

    static AdminResponse Json(int status, std::string body);
    static AdminResponse Problem(int status, std::string code, std::string message);
    static AdminResponse Invalid(std::string message, std::vector<std::pair<std::string, std::string>> fields);
};

struct AdminBrowserAccess
{
    SessionSource* Sessions = nullptr;
    std::string CookieName;
    bool Secure = false;
};

enum class PermissionVerdict : uint8
{
    Allowed,
    Forbidden,
    OutOfScope
};

enum class RouteAccess : uint8
{
    Undeclared,
    Public,
    AnyMember,
    Permission
};

class AdminRouter
{
public:
    using Handler = std::function<AdminResponse(AdminRequest const&)>;
    using PermissionCheck = std::function<PermissionVerdict(AdminRequest const&, std::string_view permission)>;
    using ProblemLog = std::function<void(AdminRequest const&, AdminResponse const&)>;
    using Known = std::function<bool(std::string_view permission)>;

    static constexpr std::string_view SecurityPolicy =
        "default-src 'none'; script-src 'self'; style-src 'self'; style-src-attr 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'self'; manifest-src 'self'; "
        "base-uri 'none'; form-action 'self'; frame-ancestors 'none'";

    explicit AdminRouter(AdminAuth& auth);

    AdminRouter(AdminRouter const&) = delete;
    AdminRouter& operator=(AdminRouter const&) = delete;

    using Throttle = std::function<std::optional<AdminResponse>(AdminRequest const&, uint32 cost)>;

    void Add(std::string method, std::string path, Handler handler);
    void AddGuarded(std::string method, std::string path, std::string permission, Handler handler);
    void AddGuardedPrefix(std::string method, std::string prefix, std::string permission, Handler handler);
    void AddOpen(std::string method, std::string path, Handler handler);
    void AddOpenPrefix(std::string method, std::string prefix, Handler handler);
    void SetPermissionKnown(Known known);
    std::vector<std::string> RouteProblems() const;
    void AddCosting(std::string method, std::string path, std::string permission, uint32 cost, Handler handler);
    void AddOpenCosting(std::string method, std::string path, uint32 cost, Handler handler);
    void SetThrottle(Throttle throttle);
    uint32 CostOf(std::string_view method, std::string_view path) const;
    void AddPublic(std::string method, std::string path, Handler handler);
    void AddPrefix(std::string method, std::string prefix, Handler handler);
    void SetFiles(Handler files);
    void SetAllowedHosts(std::vector<std::string> names);
    void SetBrowserAccess(AdminBrowserAccess access);
    void SetProblemLog(ProblemLog log);
    void SetMaxBodyBytes(std::size_t bytes);
    void SetSecure(bool secure);
    void SetTrustedProxies(TrustedProxies proxies);
    std::string ResolveAddress(std::string_view peer, std::string_view forwardedFor) const;
    bool Has(std::string const& method, std::string const& path) const;
    std::vector<std::string> Describe() const;

    bool HostAllowed(std::string_view host) const;
    std::string ExpectedOrigin(AdminRequest const& request) const;
    AdminAuthResult Authenticate(AdminRequest& request) const;
    AdminResponse Dispatch(AdminRequest const& request) const;
    void SetPermissionCheck(PermissionCheck check);
    PermissionVerdict MayI(AdminRequest const& request, std::string_view permission) const;
    bool Holds(AdminRequest const& request, std::string_view permission) const;
    std::vector<std::pair<std::string, std::string>> DeclaredRoutes() const;
    void Finish(AdminRequest const& request, AdminResponse& response) const;
    std::optional<std::string> SessionSecret(AdminRequest const& request) const;
    AdminBrowserAccess GetBrowserAccess() const;
    static AdminResponse Refused(AdminAuthResult result);
    static AdminResponse HostRefused(std::string_view host);
    static std::string NewRequestId();
    static bool IsRequestId(std::string_view text) noexcept;
    static std::string HostName(std::string_view host);

private:
    struct Route
    {
        std::string Method;
        std::string Path;
        Handler Run;
        RouteAccess Access = RouteAccess::Undeclared;
        bool Prefix = false;
        uint32 Cost = 0;
        std::string Permission;

        bool Public() const { return Access == RouteAccess::Public; }
    };

    AdminResponse Answer(AdminRequest& request) const;
    AdminResponse Serve(AdminRequest const& request) const;
    void Put(std::string method, std::string path, Handler handler, RouteAccess access, bool prefix = false, uint32 cost = 0, std::string permission = {});

    PermissionCheck _permission;
    Known _known;

    AdminAuth& _auth;
    std::atomic<std::size_t> _maxBodyBytes{ 0 };
    std::atomic<bool> _secure{ false };
    TrustedProxies _trustedProxies;
    Throttle _throttle;
    mutable std::shared_mutex _mutex;
    std::vector<Route> _routes;
    Handler _files;
    std::vector<std::string> _allowedHosts;
    AdminBrowserAccess _browser;
    ProblemLog _problemLog;
};

#endif
