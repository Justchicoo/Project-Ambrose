/*
 * Project Ambrose by Imjustchico
 * Tests that the panel and the apps agree on what each admin route needs: every route an app or the supervisor serves asks for a permission the panel's catalog holds, so no page is lost from the panel unseen, the supervisor's relay asks the same permission the app's own route asks for, method by method, settings changes, resets, batches, history and events included, and a write the relay does not know, or a sign-in, is never relayed at all; a settings relay names the caller and forwards only the rights they hold, and the query goes with it encoded; and before relaying a reveal of secrets, a change naming a restricted setting or a kill, the relay asks the listener for a fresh check of who the caller is, which it never asks for a caller refused the permission or a dry run.
 */

#include "AdminActivityView.h"
#include "AdminAuth.h"
#include "AdminClientView.h"
#include "AdminCommand.h"
#include "AdminConfigView.h"
#include "AdminGraphsView.h"
#include "AdminMetricsView.h"
#include "AdminRealmsView.h"
#include "AdminReloadView.h"
#include "AdminSettingsView.h"
#include "AdminRouter.h"
#include "AdminStatus.h"
#include "ConfigMgr.h"
#include "ConsoleCommandTable.h"
#include "OnlinePlayersView.h"
#include "PanelPermissions.h"
#include "SeriesStore.h"
#include "Settings.h"
#include "Supervisor.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    ClientSetupResult const& NoClient()
    {
        static ClientSetupResult const none;
        return none;
    }

    Ambrose::SeriesStore const& NoHistory()
    {
        static Ambrose::SeriesStore const none;
        return none;
    }

    void RegisterAppRoutes(AdminRouter& routes, ConsoleCommandTable const& commands)
    {
        AdminStatus::Register(routes, [] { return AdminStatusSnapshot{}; });
        static Settings settings;
        AdminConfigView::Register(routes, sConfigMgr, {}, &settings);
        AdminSettingsView::Register(routes, settings);
        AdminReloadView::Register(routes);
        AdminMetricsView::Register(routes);
        AdminActivityView::Register(routes, "activity.jsonl");
        AdminClientView::Register(routes, [] () -> ClientSetupResult const& { return NoClient(); });
        AdminCommand::Register(routes, commands, "gameserver", "activity.jsonl");
        AdminRealmsView::Register(routes);
        OnlinePlayersView::Register(routes);
    }

    std::pair<std::string, std::string> Split(std::string const& route)
    {
        std::size_t const space = route.find(' ');
        return { route.substr(0, space), route.substr(space + 1) };
    }
}

TEST(PanelRoutesTest, EveryRouteAnAppServesAsksForAPermissionThePanelHolds)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken("a-token-for-the-catalog");
    AdminRouter routes(auth);
    routes.SetPermissionKnown([](std::string_view permission) { return PanelPermissions::Holds(permission); });
    ConsoleCommandTable const commands;
    RegisterAppRoutes(routes, commands);
    AdminGraphsView::Register(routes, [] () -> Ambrose::SeriesStore const& { return NoHistory(); });

    std::vector<std::string> problems = routes.RouteProblems();
    std::vector<std::string> const refused = routes.RefusedRoutes();
    problems.insert(problems.end(), refused.begin(), refused.end());
    std::string listed;
    for (std::string const& problem : problems)
        listed += problem + "; ";
    EXPECT_TRUE(problems.empty()) << listed;
    for (std::string_view const page : { "GET /api/client", "GET /api/activity", "GET /api/realms", "GET /api/players", "GET /api/metrics", "GET /api/graphs" })
    {
        std::pair<std::string, std::string> const route = Split(std::string(page));
        EXPECT_TRUE(routes.Has(route.first, route.second)) << page << " is served through the panel";
    }
}

TEST(PanelRoutesTest, TheRelayAsksThePermissionTheAppsOwnRouteAsks)
{
    AdminAuth auth(10, 1.0);
    auth.SetToken("a-token-for-the-relay");
    AdminRouter routes(auth);
    ConsoleCommandTable const commands;
    RegisterAppRoutes(routes, commands);

    std::size_t compared = 0;
    for (auto const& [route, permission] : routes.DeclaredRoutes())
    {
        if (permission.empty())
            continue;
        auto [method, path] = Split(route);
        if (path == "/api/settings/")
            path += method == "GET" ? "World.UpdateInterval/history" : "World.UpdateInterval";
        else if (path.ends_with('/'))
            path += "templates";
        std::optional<std::string_view> const relayed = Supervisor::PermissionFor(method, path);
        ASSERT_TRUE(relayed.has_value()) << route << " is not relayed";
        EXPECT_EQ(*relayed, permission) << route;
        ++compared;
    }
    EXPECT_GE(compared, 10u);
}

TEST(PanelRoutesTest, AWriteTheRelayDoesNotKnowOrASignInIsNeverRelayed)
{
    EXPECT_EQ(Supervisor::PermissionFor("GET", "/api/database"), std::optional<std::string_view>("database.read"));
    EXPECT_EQ(Supervisor::PermissionFor("GET", "/api/database/updates"), std::optional<std::string_view>("database.read"));
    EXPECT_EQ(Supervisor::PermissionFor("POST", "/api/database/apply"), std::optional<std::string_view>("updates.apply"));
    EXPECT_EQ(Supervisor::PermissionFor("POST", "/api/database/reload"), std::optional<std::string_view>("reload.run"));
    EXPECT_EQ(Supervisor::PermissionFor("GET", "/api/logs/after/0"), std::optional<std::string_view>("console.read"));
    EXPECT_EQ(Supervisor::PermissionFor("POST", "/api/shutdown"), std::optional<std::string_view>("power.stop"));
    EXPECT_EQ(Supervisor::PermissionFor("PATCH", "/api/settings"), std::optional<std::string_view>("settings.edit"));
    EXPECT_EQ(Supervisor::PermissionFor("PUT", "/api/settings/World.UpdateInterval"), std::optional<std::string_view>("settings.edit"));
    EXPECT_EQ(Supervisor::PermissionFor("GET", "/api/settings/World.UpdateInterval/history"), std::optional<std::string_view>("settings.read"));
    EXPECT_EQ(Supervisor::PermissionFor("POST", "/api/settings/batch"), std::optional<std::string_view>("settings.edit"));
    EXPECT_EQ(Supervisor::PermissionFor("GET", "/api/events/after/0"), std::optional<std::string_view>("settings.read"));
    EXPECT_FALSE(Supervisor::PermissionFor("GET", "/api/settings/batch").has_value());
    EXPECT_FALSE(Supervisor::PermissionFor("PUT", "/api/settings/").has_value()) << "a change names its key";
    EXPECT_FALSE(Supervisor::PermissionFor("PUT", "/api/settings/World.UpdateInterval/history").has_value());
    EXPECT_EQ(Supervisor::PermissionFor("DELETE", "/api/settings/World.UpdateInterval"), std::optional<std::string_view>("settings.edit"));
    EXPECT_FALSE(Supervisor::PermissionFor("POST", "/api/events/after/0").has_value());

    EXPECT_FALSE(Supervisor::PermissionFor("POST", "/api/session").has_value()) << "a sign-in stays the supervisor's own";
    EXPECT_FALSE(Supervisor::PermissionFor("POST", "/api/realms").has_value()) << "a read route is not a write route";
    EXPECT_FALSE(Supervisor::PermissionFor("DELETE", "/api/settings").has_value());
    EXPECT_FALSE(Supervisor::PermissionFor("GET", "/api/nothing").has_value()) << "a path the relay does not know is not relayed under a looser permission";
    EXPECT_FALSE(Supervisor::PermissionFor("GET", "/api/database/apply").has_value());
    for (std::string_view const key : { "database.read", "updates.apply", "reload.run", "console.read", "power.stop", "settings.edit", "clientdata.read", "activity.read", "metrics.read" })
        EXPECT_TRUE(PanelPermissions::Holds(key)) << key;
}

TEST(PanelRoutesTest, ASettingsRelayNamesTheCallerAndForwardsOnlyTheRightsTheyHold)
{
    AdminAuth auth(10, 1.0);
    AdminRouter routes(auth);
    std::set<std::string, std::less<>> held{ "settings.read", "settings.edit" };
    routes.SetPermissionCheck([&held](AdminRequest const&, std::string_view permission) { return held.contains(permission) ? PermissionVerdict::Allowed : PermissionVerdict::Forbidden; });
    auto const headersOf = [&routes](AdminRequest const& request, std::string_view method, std::string_view tail, std::string_view permission)
    {
        std::map<std::string, std::string> out;
        for (auto const& [name, value] : Supervisor::ForwardedHeaders(request, routes, method, tail, permission, "Merle"))
            out[name] = value;
        return out;
    };

    AdminRequest request;
    request.Principal = "user:3";
    std::map<std::string, std::string> put = headersOf(request, "PUT", "/api/settings/Account.VerifierKeys", "settings.edit");
    EXPECT_EQ(put["X-Ambrose-Actor"], "user:3");
    EXPECT_EQ(put["X-Ambrose-Actor-Name"], "Merle");
    EXPECT_EQ(put["X-Ambrose-Grants"], "settings.edit") << "a right the caller lacks is never forwarded";
    held.insert("settings.edit.restricted");
    EXPECT_EQ(headersOf(request, "PUT", "/api/settings/Account.VerifierKeys", "settings.edit")["X-Ambrose-Grants"], "settings.edit,settings.edit.restricted");

    held.insert("settings.secrets.read");
    EXPECT_EQ(headersOf(request, "GET", "/api/settings", "settings.read")["X-Ambrose-Grants"], "settings.read") << "a read that does not ask to reveal forwards no reveal";
    request.QueryValues["reveal"] = "1";
    EXPECT_EQ(headersOf(request, "GET", "/api/settings", "settings.read")["X-Ambrose-Grants"], "settings.read,settings.secrets.read");
    EXPECT_TRUE(headersOf(request, "GET", "/api/status", "status.read").empty()) << "only a settings route carries who asked";

    request.QueryValues["q"] = "a b&c";
    EXPECT_EQ(Supervisor::QueryString(request), "?q=a%20b%26c&reveal=1");
    EXPECT_EQ(Supervisor::QueryString(AdminRequest{}), "");
}

TEST(PanelRoutesTest, TheRelayAsksForAFreshCheckBeforeARevealAKillOrARestrictedChange)
{
    AdminAuth auth(10, 1.0);
    AdminRouter routes(auth);
    std::set<std::string, std::less<>> held{ "settings.read", "settings.edit", "settings.edit.restricted", "settings.secrets.read", "power.kill" };
    routes.SetPermissionCheck([&held](AdminRequest const&, std::string_view permission) { return held.contains(permission) ? PermissionVerdict::Allowed : PermissionVerdict::Forbidden; });
    std::vector<std::pair<std::string, StepUpWhen>> asked;
    routes.SetStepUp([&asked](AdminRequest const&, std::string_view permission, StepUpWhen when) -> std::optional<AdminResponse>
    {
        asked.emplace_back(std::string(permission), when);
        return AdminResponse::Problem(403, "step_up_required", "Confirm it is you");
    });
    auto const request = [](std::string method, std::string body = {})
    {
        AdminRequest built;
        built.Method = std::move(method);
        built.Principal = "user:3";
        built.Body = std::move(body);
        return built;
    };
    auto const last = [&asked]() { return asked.empty() ? std::pair<std::string, StepUpWhen>{} : asked.back(); };

    AdminRequest reveal = request("GET");
    reveal.QueryValues["reveal"] = "1";
    std::optional<AdminResponse> const revealed = Supervisor::StepUpFor(reveal, routes, "GET", "/api/settings");
    ASSERT_TRUE(revealed.has_value());
    EXPECT_EQ(revealed->Status, 403);
    EXPECT_EQ(last(), (std::pair<std::string, StepUpWhen>{ "settings.secrets.read", StepUpWhen::Always }));

    std::size_t const before = asked.size();
    EXPECT_FALSE(Supervisor::StepUpFor(request("GET"), routes, "GET", "/api/settings").has_value()) << "a read that reveals nothing asks nothing";
    EXPECT_FALSE(Supervisor::StepUpFor(request("PUT", R"({"value":"100"})"), routes, "PUT", "/api/settings/World.UpdateInterval").has_value())
        << "a setting the declarations do not restrict asks nothing beyond its danger rule";
    EXPECT_FALSE(Supervisor::StepUpFor(request("POST", R"({"entries":[{"key":"Account.VerifierActiveKey","value":"1"}],"dry_run":true})"), routes, "POST", "/api/settings/batch")
                     .has_value())
        << "a dry run changes nothing, so it asks nothing";
    EXPECT_EQ(asked.size(), before);

    ASSERT_TRUE(Supervisor::StepUpFor(request("PUT", R"({"value":"2"})"), routes, "PUT", "/api/settings/Account.VerifierActiveKey").has_value());
    EXPECT_EQ(last(), (std::pair<std::string, StepUpWhen>{ "settings.edit.restricted", StepUpWhen::Always }));
    ASSERT_TRUE(Supervisor::StepUpFor(request("DELETE"), routes, "DELETE", "/api/settings/Account.VerifierKeys").has_value());
    ASSERT_TRUE(Supervisor::StepUpFor(request("POST", R"({"entries":[{"key":"World.UpdateInterval","value":"60"},{"key":"Account.AllowPlainVerifiers","value":"false"}]})"), routes,
        "POST", "/api/settings/batch")
                    .has_value())
        << "a batch naming one restricted key among others asks";
    EXPECT_EQ(last(), (std::pair<std::string, StepUpWhen>{ "settings.edit.restricted", StepUpWhen::Always }));

    std::optional<AdminResponse> const killed = Supervisor::Refuse(request("POST", R"({"action":"kill"})"), "power.kill", routes);
    ASSERT_TRUE(killed.has_value());
    EXPECT_EQ(nlohmann::json::parse(killed->Body)["error"], "step_up_required") << "an allowed kill still asks the listener before it goes ahead";
    EXPECT_EQ(last(), (std::pair<std::string, StepUpWhen>{ "power.kill", StepUpWhen::Changing }));

    held.erase("settings.secrets.read");
    held.erase("settings.edit.restricted");
    held.erase("power.kill");
    std::size_t const refusedBefore = asked.size();
    EXPECT_FALSE(Supervisor::StepUpFor(reveal, routes, "GET", "/api/settings").has_value()) << "a caller who may not see secrets is shown the mask and asked nothing";
    EXPECT_FALSE(Supervisor::StepUpFor(request("PUT", R"({"value":"2"})"), routes, "PUT", "/api/settings/Account.VerifierActiveKey").has_value())
        << "the app refuses a restricted change to a caller without the right, so nothing is asked first";
    std::optional<AdminResponse> const forbidden = Supervisor::Refuse(request("POST"), "power.kill", routes);
    ASSERT_TRUE(forbidden.has_value());
    EXPECT_EQ(forbidden->Status, 403);
    EXPECT_EQ(nlohmann::json::parse(forbidden->Body)["error"], "forbidden");
    EXPECT_EQ(asked.size(), refusedBefore) << "a refused permission is never followed by a check";
}
