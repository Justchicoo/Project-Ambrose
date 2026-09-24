/*
 * Project Ambrose by Imjustchico
 * Answers every admin API request in one order: a request id, the host check, the panel's files for paths outside /api, public routes, then the bearer token or a browser session with its origin and CSRF checks, the body limit and the route, an exact one before the longest prefix that covers the path, and finally the security headers, the request id in the answer and in any error body, and the error log.
 */

#include "AdminRouter.h"
#include "AdminSessions.h"
#include "Base64.h"
#include "SHA256.h"
#include "ConstantTime.h"
#include "CryptoRandom.h"
#include "IpAddress.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <mutex>
#include <string>
#include <utility>

namespace
{
    std::string ProblemBody(std::string const& code, std::string const& message)
    {
        nlohmann::json body;
        body["error"] = code;
        body["message"] = message;
        return body.dump();
    }

    bool IsApiPath(std::string_view path)
    {
        return path == "/api" || path.starts_with("/api/");
    }

    bool Unsafe(std::string_view method)
    {
        return method != "GET" && method != "HEAD" && method != "OPTIONS";
    }

    bool HasHeader(AdminResponse const& response, std::string_view name)
    {
        return std::any_of(response.Headers.begin(), response.Headers.end(), [name](std::pair<std::string, std::string> const& header) { return Ambrose::EqualsIgnoreCase(header.first, name); });
    }

    std::optional<std::string> CookieValue(std::string_view header, std::string_view name)
    {
        while (!header.empty())
        {
            std::size_t const semicolon = header.find(';');
            std::string_view const pair = Ambrose::Trim(header.substr(0, semicolon));
            std::size_t const equals = pair.find('=');
            if (equals != std::string_view::npos && Ambrose::Trim(pair.substr(0, equals)) == name)
                return std::string(Ambrose::Trim(pair.substr(equals + 1)));
            if (semicolon == std::string_view::npos)
                break;
            header.remove_prefix(semicolon + 1);
        }
        return std::nullopt;
    }
}

AdminResponse AdminResponse::Json(int status, std::string body)
{
    AdminResponse response;
    response.Status = status;
    response.Body = std::move(body);
    return response;
}

AdminResponse AdminResponse::Problem(int status, std::string code, std::string message)
{
    return Json(status, ProblemBody(code, message));
}

AdminResponse AdminResponse::Invalid(std::string message, std::vector<std::pair<std::string, std::string>> fields)
{
    nlohmann::json body;
    body["error"] = "invalid";
    body["message"] = std::move(message);
    body["fields"] = nlohmann::json::object();
    for (auto& [field, problem] : fields)
        body["fields"][field] = std::move(problem);
    return Json(422, body.dump());
}

AdminRouter::AdminRouter(AdminAuth& auth) : _auth(auth)
{
}

void AdminRouter::Put(std::string method, std::string path, Handler handler, RouteAccess access, bool prefix, uint32 cost, std::string permission)
{
    std::unique_lock const lock(_mutex);
    if (access == RouteAccess::Permission && _known && !_known(permission))
        return;
    std::string const upper = Ambrose::ToUpper(method);
    auto const existing = std::find_if(_routes.begin(), _routes.end(), [&](Route const& route) { return route.Method == upper && route.Path == path && route.Prefix == prefix; });
    if (existing != _routes.end())
    {
        existing->Run = std::move(handler);
        existing->Access = access;
        existing->Cost = cost;
        existing->Permission = std::move(permission);
        return;
    }
    _routes.push_back({ upper, std::move(path), std::move(handler), access, prefix, cost, std::move(permission) });
}

void AdminRouter::AddOpen(std::string method, std::string path, Handler handler)
{
    Put(std::move(method), std::move(path), std::move(handler), RouteAccess::AnyMember);
}

void AdminRouter::AddOpenPrefix(std::string method, std::string prefix, Handler handler)
{
    if (prefix.empty() || prefix.back() != '/')
        prefix.push_back('/');
    Put(std::move(method), std::move(prefix), std::move(handler), RouteAccess::AnyMember, true);
}

void AdminRouter::SetPermissionKnown(Known known)
{
    std::unique_lock const lock(_mutex);
    _known = std::move(known);
}

std::vector<std::string> AdminRouter::RouteProblems() const
{
    std::shared_lock const lock(_mutex);
    std::vector<std::string> problems;
    for (Route const& route : _routes)
    {
        std::string const where = route.Method + " " + route.Path + (route.Prefix ? "*" : "");
        if (route.Access == RouteAccess::Undeclared)
            problems.push_back(where + " says neither which permission it needs nor that any signed-in member may call it");
        else if (route.Access == RouteAccess::Permission && _known && !_known(route.Permission))
            problems.push_back(where + " asks for " + route.Permission + ", which the catalog does not hold");
    }
    std::sort(problems.begin(), problems.end());
    return problems;
}

void AdminRouter::AddGuarded(std::string method, std::string path, std::string permission, Handler handler)
{
    Put(std::move(method), std::move(path), std::move(handler), RouteAccess::Permission, false, 0, std::move(permission));
}

void AdminRouter::AddGuardedPrefix(std::string method, std::string prefix, std::string permission, Handler handler)
{
    Put(std::move(method), std::move(prefix), std::move(handler), RouteAccess::Permission, true, 0, std::move(permission));
}

PermissionVerdict AdminRouter::MayI(AdminRequest const& request, std::string_view permission) const
{
    PermissionCheck check;
    {
        std::shared_lock const lock(_mutex);
        check = _permission;
    }
    return check ? check(request, permission) : PermissionVerdict::Allowed;
}

bool AdminRouter::Holds(AdminRequest const& request, std::string_view permission) const
{
    PermissionCheck check;
    {
        std::shared_lock const lock(_mutex);
        check = _permission;
    }
    return check && check(request, permission) == PermissionVerdict::Allowed;
}

void AdminRouter::SetPermissionCheck(PermissionCheck check)
{
    std::unique_lock const lock(_mutex);
    _permission = std::move(check);
}

std::vector<std::pair<std::string, std::string>> AdminRouter::DeclaredRoutes() const
{
    std::shared_lock const lock(_mutex);
    std::vector<std::pair<std::string, std::string>> declared;
    declared.reserve(_routes.size());
    for (Route const& route : _routes)
        if (!route.Public())
            declared.emplace_back(route.Method + " " + route.Path, route.Permission);
    return declared;
}

void AdminRouter::Add(std::string method, std::string path, Handler handler)
{
    Put(std::move(method), std::move(path), std::move(handler), RouteAccess::Undeclared);
}

void AdminRouter::AddCosting(std::string method, std::string path, std::string permission, uint32 cost, Handler handler)
{
    Put(std::move(method), std::move(path), std::move(handler), RouteAccess::Permission, false, cost, std::move(permission));
}

void AdminRouter::AddOpenCosting(std::string method, std::string path, uint32 cost, Handler handler)
{
    Put(std::move(method), std::move(path), std::move(handler), RouteAccess::AnyMember, false, cost);
}

void AdminRouter::SetThrottle(Throttle throttle)
{
    std::unique_lock const lock(_mutex);
    _throttle = std::move(throttle);
}

uint32 AdminRouter::CostOf(std::string_view method, std::string_view path) const
{
    std::shared_lock const lock(_mutex);
    std::string const upper = Ambrose::ToUpper(method);
    for (Route const& route : _routes)
    {
        if (route.Prefix ? path.starts_with(route.Path) : route.Path == path)
        {
            if (route.Method == upper)
                return route.Cost;
        }
    }
    return 0;
}

void AdminRouter::AddPrefix(std::string method, std::string prefix, Handler handler)
{
    if (prefix.empty() || prefix.back() != '/')
        prefix.push_back('/');
    Put(std::move(method), std::move(prefix), std::move(handler), RouteAccess::Undeclared, true);
}

void AdminRouter::AddPublic(std::string method, std::string path, Handler handler)
{
    Put(std::move(method), std::move(path), std::move(handler), RouteAccess::Public);
}

void AdminRouter::SetFiles(Handler files)
{
    std::unique_lock const lock(_mutex);
    _files = std::move(files);
}

void AdminRouter::SetAllowedHosts(std::vector<std::string> names)
{
    for (std::string& name : names)
        name = Ambrose::ToLower(Ambrose::Trim(name));
    std::erase_if(names, [](std::string const& name) { return name.empty(); });
    std::unique_lock const lock(_mutex);
    _allowedHosts = std::move(names);
}

void AdminRouter::SetBrowserAccess(AdminBrowserAccess access)
{
    std::unique_lock const lock(_mutex);
    _browser = std::move(access);
}

AdminBrowserAccess AdminRouter::GetBrowserAccess() const
{
    std::shared_lock const lock(_mutex);
    return _browser;
}

void AdminRouter::SetProblemLog(ProblemLog log)
{
    std::unique_lock const lock(_mutex);
    _problemLog = std::move(log);
}

void AdminRouter::SetSecure(bool secure)
{
    _secure.store(secure);
}

void AdminRouter::SetTrustedProxies(TrustedProxies proxies)
{
    std::unique_lock lock(_mutex);
    _trustedProxies = std::move(proxies);
}

std::string AdminRouter::ResolveAddress(std::string_view peer, std::string_view forwardedFor) const
{
    std::shared_lock lock(_mutex);
    return _trustedProxies.ClientAddress(peer, forwardedFor);
}

void AdminRouter::SetMaxBodyBytes(std::size_t bytes)
{
    _maxBodyBytes.store(bytes);
}

bool AdminRouter::Has(std::string const& method, std::string const& path) const
{
    std::shared_lock const lock(_mutex);
    std::string const upper = Ambrose::ToUpper(method);
    return std::any_of(_routes.begin(), _routes.end(), [&](Route const& route) { return route.Method == upper && route.Path == path && !route.Prefix; });
}

std::vector<std::string> AdminRouter::Describe() const
{
    std::shared_lock const lock(_mutex);
    std::vector<std::string> lines;
    lines.reserve(_routes.size());
    for (Route const& route : _routes)
        lines.push_back(route.Method + " " + route.Path + (route.Prefix ? "*" : ""));
    std::sort(lines.begin(), lines.end());
    return lines;
}

std::string AdminRouter::HostName(std::string_view host)
{
    host = Ambrose::Trim(host);
    if (host.starts_with('['))
    {
        std::size_t const close = host.find(']');
        return close == std::string_view::npos ? std::string() : Ambrose::ToLower(host.substr(1, close - 1));
    }
    std::size_t const colon = host.find(':');
    if (colon != std::string_view::npos && host.find(':', colon + 1) == std::string_view::npos)
        host = host.substr(0, colon);
    return Ambrose::ToLower(host);
}

bool AdminRouter::HostAllowed(std::string_view host) const
{
    if (Ambrose::Trim(host).empty())
        return true;
    std::string const name = HostName(host);
    if (name.empty())
        return false;
    if (name == "localhost" || Ambrose::Asio::MakeAddress(name))
        return true;
    std::shared_lock const lock(_mutex);
    return std::find(_allowedHosts.begin(), _allowedHosts.end(), name) != _allowedHosts.end();
}

std::string AdminRouter::ExpectedOrigin(AdminRequest const& request) const
{
    return std::string(GetBrowserAccess().Secure ? "https://" : "http://") + Ambrose::ToLower(Ambrose::Trim(request.Host));
}

std::optional<std::string> AdminRouter::SessionSecret(AdminRequest const& request) const
{
    AdminBrowserAccess const browser = GetBrowserAccess();
    if (!browser.Sessions || browser.CookieName.empty() || request.Cookie.empty())
        return std::nullopt;
    return CookieValue(request.Cookie, browser.CookieName);
}

AdminAuthResult AdminRouter::Authenticate(AdminRequest& request) const
{
    if (!request.Authorization.empty())
    {
        AdminAuthResult const result = _auth.Check(request.RemoteAddress, request.Authorization);
        if (result == AdminAuthResult::Ok)
            request.Principal = "token";
        return result;
    }

    AdminBrowserAccess const browser = GetBrowserAccess();
    if (browser.Sessions)
    {
        if (std::optional<std::string> const secret = SessionSecret(request))
        {
            if (std::optional<SessionHolder> const held = browser.Sessions->Hold(*secret))
            {
                std::string const& csrf = held->Csrf;
                std::string const method = Ambrose::ToUpper(request.Method);
                bool const changes = Unsafe(method);
                if ((changes || request.Upgrade) && !Ambrose::EqualsIgnoreCase(request.Origin, ExpectedOrigin(request)))
                    return AdminAuthResult::Forbidden;
                if (changes && !request.Upgrade && !Ambrose::Crypto::ConstantTimeEquals(request.Csrf, csrf))
                    return AdminAuthResult::Forbidden;
                request.SessionCsrf = csrf;
                request.Principal = held->Principal.empty()
                    ? "session:" + Base64::Encode(SHA256::GetDigestOf(*secret), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted).substr(0, 16)
                    : held->Principal;
                return AdminAuthResult::Ok;
            }
        }
    }
    return _auth.Check(request.RemoteAddress, request.Authorization);
}

AdminResponse AdminRouter::Refused(AdminAuthResult result)
{
    if (result == AdminAuthResult::RateLimited)
    {
        AdminResponse response = AdminResponse::Problem(429, "too_many_requests", "Too many failed admin API authentication attempts from this address");
        response.Headers.emplace_back("Retry-After", "1");
        return response;
    }
    if (result == AdminAuthResult::Forbidden)
        return AdminResponse::Problem(403, "cross_origin", "The request did not come from this listener's own page: its origin or its CSRF token does not match the session");
    AdminResponse response = AdminResponse::Problem(401, "unauthorized", "The admin API needs its bearer token in an Authorization header, or a signed-in browser session");
    response.Headers.emplace_back("WWW-Authenticate", "Bearer");
    return response;
}

AdminResponse AdminRouter::HostRefused(std::string_view host)
{
    return AdminResponse::Problem(400, "host_not_allowed", "The admin API does not answer for " + HostName(host) + "; add that name to Admin.AllowedHosts to reach it by that name");
}

std::string AdminRouter::NewRequestId()
{
    std::array<uint8, 12> const bytes = Ambrose::Crypto::GetRandomArray<12>();
    return Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
}

bool AdminRouter::IsRequestId(std::string_view text) noexcept
{
    return text.size() == 16 && std::all_of(text.begin(), text.end(), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; });
}

AdminResponse AdminRouter::Dispatch(AdminRequest const& incoming) const
{
    AdminRequest request = incoming;
    if (request.Id.empty())
        request.Id = NewRequestId();
    AdminResponse response = Answer(request);
    Finish(request, response);
    return response;
}

AdminResponse AdminRouter::Answer(AdminRequest& request) const
{
    if (!HostAllowed(request.Host))
        return HostRefused(request.Host);

    if (!IsApiPath(request.Path))
    {
        Handler files;
        {
            std::shared_lock const lock(_mutex);
            files = _files;
        }
        if (!files)
            return AdminResponse::Problem(404, "not_found", "The admin API has no " + request.Path);
        return files(request);
    }

    std::size_t const limit = _maxBodyBytes.load();
    Handler open;
    {
        std::shared_lock const lock(_mutex);
        std::string const method = Ambrose::ToUpper(request.Method);
        for (Route const& route : _routes)
        {
            if (route.Public() && !route.Prefix && route.Path == request.Path && route.Method == method)
            {
                open = route.Run;
                break;
            }
        }
    }
    if (open)
    {
        if (limit != 0 && request.Body.size() > limit)
            return AdminResponse::Problem(413, "payload_too_large", "The admin API takes at most " + std::to_string(limit) + " bytes of request body");
        try
        {
            return open(request);
        }
        catch (std::exception const& failure)
        {
            return AdminResponse::Problem(500, "handler_failed", std::string("The admin API handler failed: ") + failure.what());
        }
    }

    AdminAuthResult const authenticated = Authenticate(request);
    if (authenticated != AdminAuthResult::Ok)
        return Refused(authenticated);
    if (limit != 0 && request.Body.size() > limit)
        return AdminResponse::Problem(413, "payload_too_large", "The admin API takes at most " + std::to_string(limit) + " bytes of request body");
    Throttle throttle;
    {
        std::shared_lock const lock(_mutex);
        throttle = _throttle;
    }
    if (throttle)
    {
        if (std::optional<AdminResponse> held = throttle(request, CostOf(request.Method, request.Path)))
            return std::move(*held);
    }
    return Serve(request);
}

void AdminRouter::Finish(AdminRequest const& request, AdminResponse& response) const
{
    response.Headers.emplace_back("X-Request-Id", request.Id);
    response.Headers.emplace_back("Content-Security-Policy", std::string(SecurityPolicy));
    response.Headers.emplace_back("X-Content-Type-Options", "nosniff");
    response.Headers.emplace_back("X-Frame-Options", "DENY");
    response.Headers.emplace_back("Referrer-Policy", "same-origin");
    response.Headers.emplace_back("Cross-Origin-Opener-Policy", "same-origin");
    response.Headers.emplace_back("Cross-Origin-Resource-Policy", "same-origin");
    if (_secure.load())
        response.Headers.emplace_back("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    if (!HasHeader(response, "Cache-Control"))
        response.Headers.emplace_back("Cache-Control", "no-store");

    if (response.Status < 400)
        return;
    if (response.ContentType == "application/json")
    {
        nlohmann::json body = nlohmann::json::parse(response.Body, nullptr, false);
        if (body.is_object())
        {
            body["request_id"] = request.Id;
            response.Body = body.dump();
        }
    }
    ProblemLog log;
    {
        std::shared_lock const lock(_mutex);
        log = _problemLog;
    }
    if (log)
        log(request, response);
}

AdminResponse AdminRouter::Serve(AdminRequest const& request) const
{
    Handler handler;
    std::string permission;
    PermissionCheck check;
    std::vector<std::string> allowed;
    {
        std::shared_lock const lock(_mutex);
        check = _permission;
        std::string const method = Ambrose::ToUpper(request.Method);
        for (Route const& route : _routes)
        {
            if (route.Prefix || route.Path != request.Path)
                continue;
            if (route.Method == method)
            {
                handler = route.Run;
                permission = route.Permission;
                break;
            }
            allowed.push_back(route.Method);
        }
        if (!handler && allowed.empty())
        {
            std::size_t longest = 0;
            for (Route const& route : _routes)
            {
                if (!route.Prefix || request.Path.size() <= route.Path.size() || request.Path.compare(0, route.Path.size(), route.Path) != 0 || route.Path.size() < longest)
                    continue;
                if (route.Path.size() > longest)
                {
                    longest = route.Path.size();
                    handler = nullptr;
                    permission.clear();
                    allowed.clear();
                }
                if (route.Method == method)
                {
                    handler = route.Run;
                    permission = route.Permission;
                }
                else
                {
                    allowed.push_back(route.Method);
                }
            }
            if (handler)
                allowed.clear();
        }
    }

    if (!handler)
    {
        if (allowed.empty())
            return AdminResponse::Problem(404, "not_found", "The admin API has no " + request.Path);
        std::sort(allowed.begin(), allowed.end());
        std::string methods;
        for (std::string const& method : allowed)
            methods += (methods.empty() ? "" : ", ") + method;
        AdminResponse response = AdminResponse::Problem(405, "method_not_allowed", request.Path + " answers " + methods);
        response.Headers.emplace_back("Allow", methods);
        return response;
    }

    if (check && !permission.empty())
    {
        switch (check(request, permission))
        {
            case PermissionVerdict::Allowed:
                break;
            case PermissionVerdict::OutOfScope:
                return AdminResponse::Problem(404, "not_found", "The admin API has no " + request.Path);
            case PermissionVerdict::Forbidden:
                return AdminResponse::Problem(403, "forbidden", "This account is not allowed to " + permission);
        }
    }

    try
    {
        return handler(request);
    }
    catch (std::exception const& failure)
    {
        return AdminResponse::Problem(500, "handler_failed", std::string("The admin API handler failed: ") + failure.what());
    }
}
