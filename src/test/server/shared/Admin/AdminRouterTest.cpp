/*
 * Project Ambrose by Imjustchico
 * Tests admin API routing without sockets: authentication runs before the table, a wrong token is rate limited while the right one still answers, a request naming no caller address is refused, an oversized body is refused before the handler, a known method and path reaches its handler, another method answers 405, an unknown path answers 404, and a handler that throws becomes a 500 problem.
 */

#include "AdminAuth.h"
#include "AdminRouter.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    AdminRequest Get(std::string path, std::string authorization = std::string("Bearer ") + Token)
    {
        AdminRequest request;
        request.Method = "GET";
        request.Path = std::move(path);
        request.RemoteAddress = "127.0.0.1";
        request.Authorization = std::move(authorization);
        return request;
    }

    std::string HeaderValue(AdminResponse const& response, std::string const& name)
    {
        for (std::pair<std::string, std::string> const& header : response.Headers)
            if (header.first == name)
                return header.second;
        return {};
    }
}

TEST(AdminRouterTest, ServesARegisteredRoute)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("get", "/api/health", [](AdminRequest const& request)
    {
        nlohmann::json body;
        body["path"] = request.Path;
        return AdminResponse::Json(200, body.dump());
    });

    EXPECT_TRUE(router.Has("GET", "/api/health"));
    EXPECT_FALSE(router.Has("POST", "/api/health"));
    EXPECT_EQ(router.Describe(), (std::vector<std::string>{ "GET /api/health" }));

    AdminResponse const answer = router.Dispatch(Get("/api/health"));
    EXPECT_EQ(answer.Status, 200);
    EXPECT_EQ(answer.ContentType, "application/json");
    EXPECT_EQ(nlohmann::json::parse(answer.Body)["path"], "/api/health");
}

TEST(AdminRouterTest, RefusesEveryPathWithoutTheToken)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("GET", "/api/health", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });

    AdminResponse const missing = router.Dispatch(Get("/api/health", ""));
    EXPECT_EQ(missing.Status, 401);
    EXPECT_EQ(HeaderValue(missing, "WWW-Authenticate"), "Bearer");
    EXPECT_EQ(nlohmann::json::parse(missing.Body)["error"], "unauthorized");

    AdminResponse const unknown = router.Dispatch(Get("/api/secret", ""));
    EXPECT_EQ(unknown.Status, 401);
}

TEST(AdminRouterTest, AnswersUnknownPathsAndMethods)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("POST", "/api/commands", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    router.Add("DELETE", "/api/commands", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });

    AdminResponse const missing = router.Dispatch(Get("/api/nothing"));
    EXPECT_EQ(missing.Status, 404);
    EXPECT_EQ(nlohmann::json::parse(missing.Body)["error"], "not_found");

    AdminResponse const wrongMethod = router.Dispatch(Get("/api/commands"));
    EXPECT_EQ(wrongMethod.Status, 405);
    EXPECT_EQ(HeaderValue(wrongMethod, "Allow"), "DELETE, POST");
}

TEST(AdminRouterTest, ReplacesAHandlerRegisteredTwice)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("GET", "/api/health", [](AdminRequest const&) { return AdminResponse::Json(200, "{\"first\":1}"); });
    router.Add("GET", "/api/health", [](AdminRequest const&) { return AdminResponse::Json(200, "{\"second\":1}"); });

    EXPECT_EQ(router.Describe().size(), 1u);
    EXPECT_EQ(router.Dispatch(Get("/api/health")).Body, "{\"second\":1}");
}

TEST(AdminRouterTest, AFailingHandlerBecomesAProblem)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("GET", "/api/health", [](AdminRequest const&) -> AdminResponse { throw std::runtime_error("no health here"); });

    AdminResponse const answer = router.Dispatch(Get("/api/health"));
    EXPECT_EQ(answer.Status, 500);
    EXPECT_EQ(nlohmann::json::parse(answer.Body)["error"], "handler_failed");
    EXPECT_NE(answer.Body.find("no health here"), std::string::npos);
}

TEST(AdminRouterTest, LimitsRepeatedFailuresFromOneAddress)
{
    AdminAuth auth(10, 0.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("GET", "/api/health", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });

    int limited = 0;
    for (int attempt = 0; attempt < 20; ++attempt)
        if (router.Dispatch(Get("/api/health", "Bearer wrong")).Status == 429)
            ++limited;
    EXPECT_EQ(limited, 10);

    AdminResponse const refused = router.Dispatch(Get("/api/health", "Bearer wrong"));
    EXPECT_EQ(refused.Status, 429);
    EXPECT_EQ(HeaderValue(refused, "Retry-After"), "1");
    EXPECT_EQ(nlohmann::json::parse(refused.Body)["error"], "too_many_requests");
    EXPECT_EQ(router.Dispatch(Get("/api/health")).Status, 200);

    AdminRequest anonymous = Get("/api/health");
    anonymous.RemoteAddress.clear();
    EXPECT_EQ(router.Dispatch(anonymous).Status, 401);
}

TEST(AdminRouterTest, RefusesABodyOverTheLimitAfterTheToken)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("POST", "/api/echo", [](AdminRequest const& request) { return AdminResponse::Json(200, request.Body); });
    router.SetMaxBodyBytes(16);

    AdminRequest request = Get("/api/echo");
    request.Method = "POST";
    request.Body = std::string(16, 'a');
    EXPECT_EQ(router.Dispatch(request).Status, 200);

    request.Body = std::string(17, 'a');
    AdminResponse const refused = router.Dispatch(request);
    EXPECT_EQ(refused.Status, 413);
    EXPECT_EQ(nlohmann::json::parse(refused.Body)["error"], "payload_too_large");

    request.Authorization.clear();
    EXPECT_EQ(router.Dispatch(request).Status, 401);

    router.SetMaxBodyBytes(0);
    request.Authorization = std::string("Bearer ") + Token;
    EXPECT_EQ(router.Dispatch(request).Status, 200);
}
