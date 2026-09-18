/*
 * Project Ambrose by Imjustchico
 * Answers an admin API request: the token is checked first, so an unauthenticated caller learns nothing about which paths exist, an oversized body is refused next, then the table picks the handler for the method and path, and anything else becomes a JSON problem with the right status.
 */

#include "AdminRouter.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
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

AdminRouter::AdminRouter(AdminAuth& auth) : _auth(auth)
{
}

void AdminRouter::Add(std::string method, std::string path, Handler handler)
{
    std::unique_lock const lock(_mutex);
    std::string const upper = Ambrose::ToUpper(method);
    auto const existing = std::find_if(_routes.begin(), _routes.end(), [&](Route const& route) { return route.Method == upper && route.Path == path; });
    if (existing != _routes.end())
    {
        existing->Run = std::move(handler);
        return;
    }
    _routes.push_back({ upper, std::move(path), std::move(handler) });
}

void AdminRouter::SetMaxBodyBytes(std::size_t bytes)
{
    _maxBodyBytes.store(bytes);
}

bool AdminRouter::Has(std::string const& method, std::string const& path) const
{
    std::shared_lock const lock(_mutex);
    std::string const upper = Ambrose::ToUpper(method);
    return std::any_of(_routes.begin(), _routes.end(), [&](Route const& route) { return route.Method == upper && route.Path == path; });
}

std::vector<std::string> AdminRouter::Describe() const
{
    std::shared_lock const lock(_mutex);
    std::vector<std::string> lines;
    lines.reserve(_routes.size());
    for (Route const& route : _routes)
        lines.push_back(route.Method + " " + route.Path);
    std::sort(lines.begin(), lines.end());
    return lines;
}

AdminAuthResult AdminRouter::Authenticate(AdminRequest const& request) const
{
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
    AdminResponse response = AdminResponse::Problem(401, "unauthorized", "The admin API needs an Authorization header holding its bearer token");
    response.Headers.emplace_back("WWW-Authenticate", "Bearer");
    return response;
}

AdminResponse AdminRouter::Dispatch(AdminRequest const& request) const
{
    AdminAuthResult const authenticated = Authenticate(request);
    if (authenticated != AdminAuthResult::Ok)
        return Refused(authenticated);
    std::size_t const limit = _maxBodyBytes.load();
    if (limit != 0 && request.Body.size() > limit)
        return AdminResponse::Problem(413, "payload_too_large", "The admin API takes at most " + std::to_string(limit) + " bytes of request body");
    return Serve(request);
}

AdminResponse AdminRouter::Serve(AdminRequest const& request) const
{
    Handler handler;
    std::vector<std::string> allowed;
    {
        std::shared_lock const lock(_mutex);
        std::string const method = Ambrose::ToUpper(request.Method);
        for (Route const& route : _routes)
        {
            if (route.Path != request.Path)
                continue;
            if (route.Method == method)
            {
                handler = route.Run;
                break;
            }
            allowed.push_back(route.Method);
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

    try
    {
        return handler(request);
    }
    catch (std::exception const& failure)
    {
        return AdminResponse::Problem(500, "handler_failed", std::string("The admin API handler failed: ") + failure.what());
    }
}
