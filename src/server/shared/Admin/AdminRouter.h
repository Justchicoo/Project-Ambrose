/*
 * Project Ambrose by Imjustchico
 * The admin API request and response shapes and the route table every endpoint registers in, which authenticates a request, rate limits it and refuses an oversized body before it picks the handler for its method and path.
 */

#ifndef AMBROSE_ADMINROUTER_H
#define AMBROSE_ADMINROUTER_H

#include "AdminAuth.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

struct AdminRequest
{
    std::string Method;
    std::string Path;
    std::string RemoteAddress;
    std::string Authorization;
    std::string Body;
};

struct AdminResponse
{
    int Status = 200;
    std::string ContentType = "application/json";
    std::string Body;
    std::vector<std::pair<std::string, std::string>> Headers;

    static AdminResponse Json(int status, std::string body);
    static AdminResponse Problem(int status, std::string code, std::string message);
};

class AdminRouter
{
public:
    using Handler = std::function<AdminResponse(AdminRequest const&)>;

    explicit AdminRouter(AdminAuth& auth);

    AdminRouter(AdminRouter const&) = delete;
    AdminRouter& operator=(AdminRouter const&) = delete;

    void Add(std::string method, std::string path, Handler handler);
    void SetMaxBodyBytes(std::size_t bytes);
    bool Has(std::string const& method, std::string const& path) const;
    std::vector<std::string> Describe() const;

    AdminAuthResult Authenticate(AdminRequest const& request) const;
    AdminResponse Dispatch(AdminRequest const& request) const;
    static AdminResponse Refused(AdminAuthResult result);

private:
    struct Route
    {
        std::string Method;
        std::string Path;
        Handler Run;
    };

    AdminResponse Serve(AdminRequest const& request) const;

    AdminAuth& _auth;
    std::atomic<std::size_t> _maxBodyBytes{ 0 };
    mutable std::shared_mutex _mutex;
    std::vector<Route> _routes;
};

#endif
