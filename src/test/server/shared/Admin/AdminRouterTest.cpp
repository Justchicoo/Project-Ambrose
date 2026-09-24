/*
 * Project Ambrose by Imjustchico
 * Tests admin API routing without sockets: authentication runs before the table, a wrong token is rate limited while the right one still answers, a request naming no caller address is refused, an oversized body is refused before the handler, a known method and path reaches its handler, another method answers 405, an unknown path answers 404, a handler that throws becomes a 500 problem, a host header is read down to its name and only an IP address, localhost or an allowed name is answered, every answer carries a request id and the security headers with the id in any error body, a 422 names each field, paths outside /api and public routes need no token, and a browser session authenticates by cookie with its origin and CSRF token checked where a request changes something or upgrades, and that a route ships only when it says which permission it needs or that any signed-in member may call it, that one naming a permission nothing holds is never registered, and that a route without its permission answers 403 while one in a scope the caller cannot see answers 404 saying nothing about what was wanted.
 */

#include "AdminAuth.h"
#include "AdminRouter.h"
#include "AdminSessions.h"

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

TEST(AdminRouterTest, ReadsTheHostNameOutOfAHostHeader)
{
    EXPECT_EQ(AdminRouter::HostName("127.0.0.1:12080"), "127.0.0.1");
    EXPECT_EQ(AdminRouter::HostName("[::1]:12080"), "::1");
    EXPECT_EQ(AdminRouter::HostName("[::1]"), "::1");
    EXPECT_EQ(AdminRouter::HostName("::1"), "::1");
    EXPECT_EQ(AdminRouter::HostName("LocalHost:1"), "localhost");
    EXPECT_EQ(AdminRouter::HostName(" Panel.Example "), "panel.example");
    EXPECT_EQ(AdminRouter::HostName("[::1"), "");
}

TEST(AdminRouterTest, AnswersOnlyForItsOwnHosts)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("GET", "/api/health", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });

    AdminRequest request = Get("/api/health");
    for (std::string const host : { "", "127.0.0.1:1", "localhost:1", "[::1]:1", "10.0.0.5" })
    {
        request.Host = host;
        EXPECT_EQ(router.Dispatch(request).Status, 200) << host;
    }

    request.Host = "evil.example:1";
    AdminResponse const refused = router.Dispatch(request);
    EXPECT_EQ(refused.Status, 400);
    EXPECT_EQ(nlohmann::json::parse(refused.Body)["error"], "host_not_allowed");

    router.SetAllowedHosts({ " Panel.Example ", "" });
    request.Host = "panel.example:443";
    EXPECT_EQ(router.Dispatch(request).Status, 200);
    request.Host = "evil.example";
    EXPECT_EQ(router.Dispatch(request).Status, 400);
}

TEST(AdminRouterTest, StampsARequestIdAndTheSecurityHeaders)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("GET", "/api/health", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    std::vector<std::string> logged;
    router.SetProblemLog([&logged](AdminRequest const& request, AdminResponse const& response) { logged.push_back(request.Id + " " + std::to_string(response.Status)); });

    AdminResponse const served = router.Dispatch(Get("/api/health"));
    EXPECT_EQ(served.Status, 200);
    EXPECT_EQ(HeaderValue(served, "X-Request-Id").size(), 16u);
    EXPECT_EQ(HeaderValue(served, "Content-Security-Policy"), std::string(AdminRouter::SecurityPolicy));
    EXPECT_EQ(HeaderValue(served, "X-Content-Type-Options"), "nosniff");
    EXPECT_EQ(HeaderValue(served, "Referrer-Policy"), "same-origin");
    EXPECT_EQ(HeaderValue(served, "Cache-Control"), "no-store");
    EXPECT_TRUE(logged.empty());

    AdminResponse const missing = router.Dispatch(Get("/api/nothing"));
    std::string const id = HeaderValue(missing, "X-Request-Id");
    EXPECT_EQ(missing.Status, 404);
    EXPECT_EQ(nlohmann::json::parse(missing.Body)["request_id"], id);
    ASSERT_EQ(logged.size(), 1u);
    EXPECT_EQ(logged.front(), id + " 404");
    EXPECT_NE(HeaderValue(router.Dispatch(Get("/api/health")), "X-Request-Id"), HeaderValue(served, "X-Request-Id"));
}

TEST(AdminRouterTest, AnInvalidAnswerNamesEachField)
{
    AdminResponse const invalid = AdminResponse::Invalid("Two things are wrong", { { "token", "Enter the token" }, { "extra", "Not taken" } });
    EXPECT_EQ(invalid.Status, 422);
    nlohmann::json const body = nlohmann::json::parse(invalid.Body);
    EXPECT_EQ(body["error"], "invalid");
    EXPECT_EQ(body["message"], "Two things are wrong");
    EXPECT_EQ(body["fields"]["token"], "Enter the token");
    EXPECT_EQ(body["fields"]["extra"], "Not taken");
}

TEST(AdminRouterTest, ServesFilesAndPublicRoutesWithoutTheToken)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    router.Add("GET", "/api/session", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    router.AddPublic("POST", "/api/session", [](AdminRequest const&) { return AdminResponse::Json(201, "{}"); });

    EXPECT_EQ(router.Dispatch(Get("/", "")).Status, 404);
    router.SetFiles([](AdminRequest const& request) { return AdminResponse::Json(200, request.Path); });
    AdminResponse const file = router.Dispatch(Get("/assets/app.js", ""));
    EXPECT_EQ(file.Status, 200);
    EXPECT_EQ(file.Body, "/assets/app.js");

    AdminRequest signIn = Get("/api/session", "");
    signIn.Method = "POST";
    EXPECT_EQ(router.Dispatch(signIn).Status, 201);
    EXPECT_EQ(router.Dispatch(Get("/api/session", "")).Status, 401);
    EXPECT_EQ(router.Dispatch(Get("/api/session")).Status, 200);
}

TEST(AdminRouterTest, ASessionCookieAuthenticatesWithItsOriginAndCsrfToken)
{
    AdminAuth auth(10, 0.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    AdminSessions sessions;
    router.SetBrowserAccess({ &sessions, "ambrose_admin_1", false });
    router.Add("GET", "/api/health", [](AdminRequest const& request) { return AdminResponse::Json(200, request.SessionCsrf.value_or("none")); });
    router.Add("POST", "/api/echo", [](AdminRequest const& request) { return AdminResponse::Json(200, request.Body); });
    AdminSession const opened = sessions.Open();

    AdminRequest read = Get("/api/health", "");
    read.Host = "127.0.0.1:1";
    read.Cookie = "theme=dark; ambrose_admin_1=" + opened.Secret + "; other=1";
    AdminResponse const served = router.Dispatch(read);
    EXPECT_EQ(served.Status, 200);
    EXPECT_EQ(served.Body, opened.Csrf);

    AdminRequest change = read;
    change.Path = "/api/echo";
    change.Method = "POST";
    change.Body = "sent";
    EXPECT_EQ(router.Dispatch(change).Status, 403);
    change.Csrf = opened.Csrf;
    EXPECT_EQ(router.Dispatch(change).Status, 403);
    change.Origin = "http://evil.example";
    EXPECT_EQ(router.Dispatch(change).Status, 403);
    change.Origin = "http://127.0.0.1:1";
    AdminResponse const echoed = router.Dispatch(change);
    EXPECT_EQ(echoed.Status, 200);
    EXPECT_EQ(echoed.Body, "sent");
    change.Csrf = opened.Secret;
    EXPECT_EQ(router.Dispatch(change).Status, 403);

    AdminRequest upgrade = read;
    upgrade.Upgrade = true;
    EXPECT_EQ(router.Authenticate(upgrade), AdminAuthResult::Forbidden);
    upgrade.Origin = "http://127.0.0.1:1";
    EXPECT_EQ(router.Authenticate(upgrade), AdminAuthResult::Ok);

    AdminRequest keyed = read;
    keyed.Authorization = "Bearer wrong-token-wrong-token-wrong";
    EXPECT_EQ(router.Dispatch(keyed).Status, 401);

    AdminRequest otherCookie = read;
    otherCookie.Cookie = "ambrose_admin_2=" + opened.Secret;
    EXPECT_EQ(router.Dispatch(otherCookie).Status, 401);

    sessions.CloseAll();
    EXPECT_EQ(router.Dispatch(read).Status, 401);
}

TEST(AdminRouterTest, EveryRouteSaysWhatItNeedsOrItIsNotServed)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken("a-token-for-the-registry");
    AdminRouter routes(auth);
    routes.SetPermissionKnown([](std::string_view permission) { return permission == "status.read" || permission == "console.write"; });

    routes.AddPublic("POST", "/api/session", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    routes.AddOpen("GET", "/api/health", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    routes.AddGuarded("GET", "/api/status", "status.read", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    EXPECT_TRUE(routes.RouteProblems().empty());

    routes.AddGuarded("POST", "/api/reveal", "settings.secrets.read", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    EXPECT_FALSE(routes.Has("POST", "/api/reveal")) << "a route asking for a key nothing holds is not registered at all";
    EXPECT_TRUE(routes.RouteProblems().empty()) << "and so there is nothing left to report about it";

    routes.Add("GET", "/api/quiet", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    std::vector<std::string> const problems = routes.RouteProblems();
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("GET /api/quiet"), std::string::npos) << problems[0];
}

TEST(AdminRouterTest, ARouteAnswers403WithoutItsPermissionAnd404ForAScopeTheCallerCannotSee)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken("a-token-for-the-verdicts");
    AdminRouter routes(auth);
    routes.AddGuarded("POST", "/api/apps/gameserver/api/command", "console.write", [](AdminRequest const&) { return AdminResponse::Json(200, "{\"ran\":true}"); });

    PermissionVerdict answer = PermissionVerdict::Allowed;
    routes.SetPermissionCheck([&answer](AdminRequest const&, std::string_view) { return answer; });

    AdminRequest request;
    request.Method = "POST";
    request.Path = "/api/apps/gameserver/api/command";
    request.RemoteAddress = "127.0.0.1";
    request.Authorization = "Bearer a-token-for-the-verdicts";

    EXPECT_EQ(routes.Dispatch(request).Status, 200);

    answer = PermissionVerdict::Forbidden;
    EXPECT_EQ(routes.Dispatch(request).Status, 403);

    answer = PermissionVerdict::OutOfScope;
    AdminResponse const unseen = routes.Dispatch(request);
    EXPECT_EQ(unseen.Status, 404);
    EXPECT_EQ(unseen.Body.find("console.write"), std::string::npos) << "a refusal must not name what the caller would have needed";
}
