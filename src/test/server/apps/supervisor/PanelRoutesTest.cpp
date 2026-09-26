/*
 * Project Ambrose by Imjustchico
 * Tests that the panel and the apps agree on what each admin route needs: every route an app or the supervisor serves asks for a permission the panel's catalog holds, so no page is lost from the panel unseen, the supervisor's relay asks the same permission the app's own route asks for, method by method, and a write the relay does not know, or a sign-in, is never relayed at all.
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
#include "AdminRouter.h"
#include "AdminStatus.h"
#include "ConfigMgr.h"
#include "ConsoleCommandTable.h"
#include "OnlinePlayersView.h"
#include "PanelPermissions.h"
#include "SeriesStore.h"
#include "Supervisor.h"

#include <gtest/gtest.h>

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
        AdminConfigView::Register(routes, sConfigMgr);
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
        if (path.ends_with('/'))
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

    EXPECT_FALSE(Supervisor::PermissionFor("POST", "/api/session").has_value()) << "a sign-in stays the supervisor's own";
    EXPECT_FALSE(Supervisor::PermissionFor("POST", "/api/realms").has_value()) << "a read route is not a write route";
    EXPECT_FALSE(Supervisor::PermissionFor("DELETE", "/api/settings").has_value());
    EXPECT_FALSE(Supervisor::PermissionFor("GET", "/api/nothing").has_value()) << "a path the relay does not know is not relayed under a looser permission";
    EXPECT_FALSE(Supervisor::PermissionFor("GET", "/api/database/apply").has_value());
    for (std::string_view const key : { "database.read", "updates.apply", "reload.run", "console.read", "power.stop", "settings.edit", "clientdata.read", "activity.read", "metrics.read" })
        EXPECT_TRUE(PanelPermissions::Holds(key)) << key;
}
