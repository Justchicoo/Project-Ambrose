/*
 * Project Ambrose by Imjustchico
 * Tests the operations calendar 17.96 reads from: the collector naming each unlanded source with its milestone instead of a silent empty, the installation maintenance window from 17.64's maintenance_state appearing at its window, entries a viewer may not see filtered by permission, an owner holding no grant and a viewer both seeing the maintenance entry through the route, since the route decides each source by role as well as grant, a player-facing entry running across a disruptive one flagged as a conflict, and the answer carrying events, sources and conflicts in their shapes.
 */

#include "AdminTestClient.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"
#include "PanelOpsCalendar.h"
#include "PanelStore.h"
#include "PanelTestSession.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace
{
    bool OpenStore(LogTestDirectory const& directory, PanelStore& store, std::string& error)
    {
        std::vector<std::string> warnings;
        if (!store.Open(directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error))
            return false;
        return PanelAudit::EnsureChain(store, error);
    }

    bool SeedMaintenance(PanelStore& store, bool active, int64 startMs, int64 windowStartMs, int64 windowEndMs)
    {
        std::string error;
        std::optional<PanelStore::Statement> write = store.Prepare(
            "INSERT INTO maintenance_state (id, active, reason, started_by, started_epoch_ms, window_start_epoch_ms, window_end_epoch_ms) "
            "VALUES (1, ?, 'db upgrade', 'tester', ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET active = excluded.active, reason = excluded.reason, started_by = excluded.started_by, "
            "started_epoch_ms = excluded.started_epoch_ms, window_start_epoch_ms = excluded.window_start_epoch_ms, "
            "window_end_epoch_ms = excluded.window_end_epoch_ms",
            error);
        if (!write)
            return false;
        write->Bind(1, active ? int64(1) : int64(0));
        write->Bind(2, startMs);
        write->Bind(3, windowStartMs);
        write->Bind(4, windowEndMs);
        return write->Run(error);
    }

    PanelOpsCalendar::CalendarEvent FakeEvent(std::string id, PanelOpsCalendar::Source source,
        int64 startMs, int64 endMs, bool disruptive, std::string permission)
    {
        PanelOpsCalendar::CalendarEvent event;
        event.Id = std::move(id);
        event.EventSource = source;
        event.Title = event.Id;
        event.StartEpochMs = startMs;
        event.EndEpochMs = endMs;
        event.Disruptive = disruptive;
        event.Permission = std::move(permission);
        return event;
    }

    const std::vector<std::string> Viewer{ "status.read", "schedules.read", "realms.read" };
}

TEST(PanelOpsCalendar, UnlandedSourcesAreNamed)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    std::vector<PanelOpsCalendar::SourceRegistration> registrations;
    PanelOpsCalendar::RegisterDefaults(store, registrations);
    ASSERT_EQ(registrations.size(), 4u);

    std::vector<PanelOpsCalendar::CalendarEvent> events;
    std::vector<PanelOpsCalendar::SourceAvailability> sources;
    std::vector<PanelOpsCalendar::CalendarConflict> conflicts;
    ASSERT_TRUE(PanelOpsCalendar::Collect(registrations, Viewer, 0, 1000, events, sources, conflicts, error)) << error;
    ASSERT_EQ(sources.size(), 4u);
    for (auto const& source : sources)
    {
        if (source.EventSource == PanelOpsCalendar::Source::InstallationMaintenance)
        {
            EXPECT_TRUE(source.Available);
        }
        else
        {
            EXPECT_FALSE(source.Available);
            EXPECT_NE(source.UnavailableReason.find(std::string(PanelOpsCalendar::SourceMilestone(source.EventSource))), std::string::npos)
                << source.UnavailableReason;
        }
    }
    EXPECT_TRUE(events.empty());
}

TEST(PanelOpsCalendar, InstallationMaintenanceWindowAppears)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(SeedMaintenance(store, true, 1000, 2000, 3000));

    std::vector<PanelOpsCalendar::SourceRegistration> registrations;
    PanelOpsCalendar::RegisterDefaults(store, registrations);

    std::vector<PanelOpsCalendar::CalendarEvent> events;
    std::vector<PanelOpsCalendar::SourceAvailability> sources;
    std::vector<PanelOpsCalendar::CalendarConflict> conflicts;
    ASSERT_TRUE(PanelOpsCalendar::Collect(registrations, Viewer, 0, 9000, events, sources, conflicts, error)) << error;
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].Id, "installation-maintenance");
    EXPECT_EQ(events[0].StartEpochMs, 2000);
    EXPECT_EQ(events[0].EndEpochMs, 3000);
    EXPECT_TRUE(events[0].Disruptive);
    EXPECT_EQ(events[0].Url, "#/overview");

    nlohmann::json answer = PanelOpsCalendar::AnswerJson(events, sources, conflicts);
    EXPECT_EQ(answer["events"][0]["color"], "#ef4444");
    EXPECT_EQ(answer["events"][0]["source"], "installation_maintenance");
}

TEST(PanelOpsCalendar, InactiveMaintenanceShowsNothing)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(SeedMaintenance(store, false, 1000, 2000, 3000));

    std::vector<PanelOpsCalendar::SourceRegistration> registrations;
    PanelOpsCalendar::RegisterDefaults(store, registrations);

    std::vector<PanelOpsCalendar::CalendarEvent> events;
    std::vector<PanelOpsCalendar::SourceAvailability> sources;
    std::vector<PanelOpsCalendar::CalendarConflict> conflicts;
    ASSERT_TRUE(PanelOpsCalendar::Collect(registrations, Viewer, 0, 9000, events, sources, conflicts, error)) << error;
    EXPECT_TRUE(events.empty());
}

TEST(PanelOpsCalendar, EventAcrossRestartIsAConflict)
{
    PanelOpsCalendar::SourceRegistration gameEvents;
    gameEvents.EventSource = PanelOpsCalendar::Source::GameEvents;
    gameEvents.RequiredPermission = "events.read";
    gameEvents.Provider = [](int64, int64, std::vector<PanelOpsCalendar::CalendarEvent>& events, std::string&)
    {
        events.push_back(FakeEvent("double-xp", PanelOpsCalendar::Source::GameEvents, 2500, 5000, false, "events.read"));
        return true;
    };
    PanelOpsCalendar::SourceRegistration maintenance;
    maintenance.EventSource = PanelOpsCalendar::Source::InstallationMaintenance;
    maintenance.RequiredPermission = "status.read";
    maintenance.Provider = [](int64, int64, std::vector<PanelOpsCalendar::CalendarEvent>& events, std::string&)
    {
        events.push_back(FakeEvent("window", PanelOpsCalendar::Source::InstallationMaintenance, 3000, 4000, true, "status.read"));
        return true;
    };

    std::vector<PanelOpsCalendar::CalendarEvent> events;
    std::vector<PanelOpsCalendar::SourceAvailability> sources;
    std::vector<PanelOpsCalendar::CalendarConflict> conflicts;
    std::string error;
    std::vector<std::string> viewer{ "status.read", "events.read" };
    ASSERT_TRUE(PanelOpsCalendar::Collect({ gameEvents, maintenance }, viewer, 0, 9000, events, sources, conflicts, error)) << error;
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].EventAId, "double-xp");
    EXPECT_EQ(conflicts[0].EventBId, "window");
    EXPECT_NE(conflicts[0].Reason.find("double-xp"), std::string::npos);
}

TEST(PanelOpsCalendar, SeparateEntriesAreNoConflict)
{
    std::vector<PanelOpsCalendar::CalendarEvent> events{
        FakeEvent("a", PanelOpsCalendar::Source::GameEvents, 1000, 2000, false, ""),
        FakeEvent("b", PanelOpsCalendar::Source::InstallationMaintenance, 3000, 4000, true, ""),
    };
    EXPECT_TRUE(PanelOpsCalendar::FindConflicts(events).empty());
}

TEST(PanelOpsCalendar, EntriesTheViewerMayNotSeeAreHidden)
{
    PanelOpsCalendar::SourceRegistration gameEvents;
    gameEvents.EventSource = PanelOpsCalendar::Source::GameEvents;
    gameEvents.RequiredPermission = "events.read";
    gameEvents.Provider = [](int64, int64, std::vector<PanelOpsCalendar::CalendarEvent>& events, std::string&)
    {
        events.push_back(FakeEvent("secret", PanelOpsCalendar::Source::GameEvents, 1000, 2000, false, "events.read"));
        return true;
    };

    std::vector<PanelOpsCalendar::CalendarEvent> events;
    std::vector<PanelOpsCalendar::SourceAvailability> sources;
    std::vector<PanelOpsCalendar::CalendarConflict> conflicts;
    std::string error;
    std::vector<std::string> viewer{ "status.read" };
    ASSERT_TRUE(PanelOpsCalendar::Collect({ gameEvents }, viewer, 0, 9000, events, sources, conflicts, error)) << error;
    EXPECT_TRUE(events.empty());
    ASSERT_EQ(sources.size(), 1u);
    EXPECT_TRUE(sources[0].Available);
}

TEST(PanelOpsCalendar, ParseEpochMs)
{
    int64 value = 0;
    EXPECT_TRUE(PanelOpsCalendar::ParseEpochMs("1728500000000", value));
    EXPECT_EQ(value, 1728500000000);
    EXPECT_FALSE(PanelOpsCalendar::ParseEpochMs("", value));
    EXPECT_FALSE(PanelOpsCalendar::ParseEpochMs("soon", value));
    EXPECT_FALSE(PanelOpsCalendar::ParseEpochMs("-5", value));
    EXPECT_FALSE(PanelOpsCalendar::ParseEpochMs("12x", value));
}

TEST(PanelOpsCalendar, AnOwnerWithNoGrantAndAViewerSeeTheMaintenanceEntryThroughTheRoute)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    LogTestDirectory directory;
    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    ASSERT_TRUE(config.LoadInitial(directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
    Panel panel(harness.GetLog(), directory.Path() / "data", directory.Path());
    std::string error;
    ASSERT_TRUE(panel.Start(config, error)) << error;
    ASSERT_TRUE(SeedMaintenance(panel.Store(), true, 1000, 2000, 3000));

    PanelTest::Browser owner;
    ASSERT_TRUE(PanelTest::ClaimOwner(panel, harness, owner));
    int64 viewerId = 0;
    ASSERT_EQ(panel.Users().Create("viewer", PanelTest::Password, false, false, &viewerId, error), PanelUserResult::Ok) << error;
    PanelTest::Browser viewer;
    ASSERT_EQ(PanelTest::SignIn(panel, viewer, "viewer").Status, 200);

    for (PanelTest::Browser const* who : { &owner, &viewer })
    {
        AdminClientResponse const answer = PanelTest::Send(panel, "GET", "/api/ops/calendar?from=0&to=9000", nullptr, who);
        ASSERT_EQ(answer.Status, 200) << answer.Body;
        nlohmann::json const body = PanelTest::Json(answer);
        ASSERT_EQ(body["events"].size(), 1u) << answer.Body;
        EXPECT_EQ(body["events"][0]["id"], "installation-maintenance");
    }
    panel.Stop();
}
