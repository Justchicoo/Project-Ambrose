/*
 * Project Ambrose by Imjustchico
 * The admin API's route table and front door: a request carries its query both decoded and exactly as it was sent, so a route that decodes a value itself, as the file jail does, decodes it once; every request gets a request id that its answer and any error body carry, keeping one a caller such as the supervisor sent when it has the same form, so one id names the request in both logs, a host that is no IP address, localhost or a name the operator allows is refused so a page elsewhere cannot rebind a name onto this listener, paths outside /api that no route claims go to the panel's files without a token, so a route a scraper expects at a fixed place such as /metrics is still served and still guarded, public routes such as signing in run without one, a route may answer every path under a prefix when no exact route claims it, the longest prefix first, and every other path needs the bearer token or a browser session whose unsafe requests and socket upgrades name this listener's own origin and carry the session's CSRF token, with every answer stamped with the panel's security headers and every error handed to a log. A caller may be asked about a permission beyond its route's, which a token caller the supervisor relays is held to only when the supervisor forwarded it, and a route may charge the listener's rate limit a cost of its own. A route that asks for a permission the listener's catalog does not hold is not served, and is kept among the refused routes so the listener can say which pages it left out rather than losing them unseen. A request says whether its address carried a query at all, even one with no value a route could read, without keeping the query itself. A listener may hold every authenticated caller to an admission rule, such as a requirement to turn on two-factor sign-in, which every route and socket passes after authentication except the routes registered as the way to meet it, and may ask for a fresh check of who the caller is before a permission it allows is used, the check deciding when a change or any use needs one and its answer standing in for the handler's. A listener may also be told of every request a route's permission refused, so the refusal can be recorded where the listener keeps its record.
 */

#ifndef AMBROSE_ADMINROUTER_H
#define AMBROSE_ADMINROUTER_H

#include "AdminAuth.h"
#include "AdminSessions.h"
#include "TrustedProxies.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
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
    std::string ForwardedActor;
    std::optional<std::string> SessionCsrf;
    std::map<std::string, std::string, std::less<>> QueryValues;
    bool HasQuery = false;
    std::string RawQuery = {};
    std::string Actor;
    std::string ActorName;
    std::optional<std::set<std::string, std::less<>>> ForwardedGrants;

    std::string_view Query(std::string_view name) const
    {
        auto const found = QueryValues.find(name);
        return found == QueryValues.end() ? std::string_view() : std::string_view(found->second);
    }

    std::optional<std::string_view> RawQueryValue(std::string_view name) const
    {
        std::string_view rest(RawQuery);
        while (!rest.empty())
        {
            std::size_t const separator = rest.find('&');
            std::string_view const pair = rest.substr(0, separator);
            std::size_t const equals = pair.find('=');
            if (pair.substr(0, equals) == name)
                return equals == std::string_view::npos ? std::string_view() : pair.substr(equals + 1);
            if (separator == std::string_view::npos)
                break;
            rest.remove_prefix(separator + 1);
        }
        return std::nullopt;
    }
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

enum class StepUpWhen : uint8
{
    Changing,
    Always
};

class AdminRouter
{
public:
    using Handler = std::function<AdminResponse(AdminRequest const&)>;
    using PermissionCheck = std::function<PermissionVerdict(AdminRequest const&, std::string_view permission)>;
    using ProblemLog = std::function<void(AdminRequest const&, AdminResponse const&)>;
    using Known = std::function<bool(std::string_view permission)>;
    using PermissionResolver = std::function<std::string(AdminRequest const&)>;
    using RefusalLog = std::function<void(AdminRequest const&, std::string_view permission, PermissionVerdict verdict)>;

    static constexpr std::string_view SecurityPolicy =
        "default-src 'none'; script-src 'self'; style-src 'self'; style-src-attr 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'self'; manifest-src 'self'; "
        "base-uri 'none'; form-action 'self'; frame-ancestors 'none'";

    explicit AdminRouter(AdminAuth& auth);

    AdminRouter(AdminRouter const&) = delete;
    AdminRouter& operator=(AdminRouter const&) = delete;

    using Throttle = std::function<std::optional<AdminResponse>(AdminRequest const&, uint32 cost)>;
    using Admission = std::function<std::optional<AdminResponse>(AdminRequest const&)>;
    using StepUpCheck = std::function<std::optional<AdminResponse>(AdminRequest const&, std::string_view permission, StepUpWhen when)>;

    void Add(std::string method, std::string path, Handler handler);
    void AddGuarded(std::string method, std::string path, std::string permission, Handler handler);
    void AddGuardedPrefix(std::string method, std::string prefix, std::string permission, Handler handler);
    void AddDynamicGuardedPrefix(std::string method, std::string prefix, std::string fallbackPermission, PermissionResolver resolver, Handler handler);
    void AddOpen(std::string method, std::string path, Handler handler);
    void AddOpenPrefix(std::string method, std::string prefix, Handler handler);
    void AddEnrollment(std::string method, std::string path, Handler handler, uint32 cost = 0);
    void SetAdmission(Admission admission);
    std::optional<AdminResponse> Admit(AdminRequest const& request) const;
    void SetStepUp(StepUpCheck check);
    void SetRefusalLog(RefusalLog log);
    std::optional<AdminResponse> StepUp(AdminRequest const& request, std::string_view permission, StepUpWhen when) const;
    void SetPermissionKnown(Known known);
    std::vector<std::string> RouteProblems() const;
    std::vector<std::string> RefusedRoutes() const;
    void AddCosting(std::string method, std::string path, std::string permission, uint32 cost, Handler handler);
    void AddOpenCosting(std::string method, std::string path, uint32 cost, Handler handler);
    void SetThrottle(Throttle throttle);
    std::optional<AdminResponse> Charge(AdminRequest const& request, uint32 cost) const;
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
    bool HasRoute(std::string_view path) const;
    void SetPermissionCheck(PermissionCheck check);
    PermissionVerdict MayI(AdminRequest const& request, std::string_view permission) const;
    bool Holds(AdminRequest const& request, std::string_view permission) const;
    bool Permits(AdminRequest const& request, std::string_view permission) const;
    std::vector<std::pair<std::string, std::string>> DeclaredRoutes() const;
    void Finish(AdminRequest const& request, AdminResponse& response) const;
    std::optional<std::string> SessionSecret(AdminRequest const& request) const;
    std::optional<std::string> CookieOf(AdminRequest const& request, std::string_view suffix) const;
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
        bool Enrollment = false;
        PermissionResolver ResolvePermission = {};

        bool Public() const { return Access == RouteAccess::Public; }
    };

    AdminResponse Answer(AdminRequest& request) const;
    AdminResponse Serve(AdminRequest const& request) const;
    void Put(std::string method, std::string path, Handler handler, RouteAccess access, bool prefix = false, uint32 cost = 0, std::string permission = {}, bool enrollment = false);

    PermissionCheck _permission;
    Known _known;

    AdminAuth& _auth;
    std::atomic<std::size_t> _maxBodyBytes{ 0 };
    std::atomic<bool> _secure{ false };
    TrustedProxies _trustedProxies;
    Throttle _throttle;
    Admission _admission;
    StepUpCheck _stepUp;
    RefusalLog _refusalLog;
    mutable std::shared_mutex _mutex;
    std::vector<Route> _routes;
    std::vector<std::string> _refused;
    Handler _files;
    std::vector<std::string> _allowedHosts;
    AdminBrowserAccess _browser;
    ProblemLog _problemLog;
};

#endif
