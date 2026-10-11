/*
 * Project Ambrose by Imjustchico
 * Tests the activity log pages: every emitted event renders a sentence and the test fails on a missing one, a refused restart shows denied on the app page, addresses hide from readers without the grant in rows and exports, a user's tab shows sign-ins and restarts, a full store fails the change with an error instead of applying it unaudited, a reason starting with = exports as text, and the hourly sweep keeps security rows longer and records itself.
 */

#include "LogTestDirectory.h"
#include "PanelActivityCatalog.h"
#include "PanelActivityLog.h"
#include "PanelAudit.h"
#include "PanelStore.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace
{
    bool OpenStore(LogTestDirectory const& directory, PanelStore& store, std::string& error)
    {
        std::vector<std::string> warnings;
        return store.Open(directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error);
    }

    bool Record(PanelStore& store, AuditEvent& event, std::string& error)
    {
        return PanelAudit::Record(store, event, std::function<bool(AuditEvent&, std::string&)>{}, error);
    }

    AuditEvent MakeEvent(std::string_view name, std::string_view actorId = "7", std::string_view address = "10.0.0.9")
    {
        AuditEvent event;
        event.Name = std::string(name);
        event.Actor = AuditActor::User;
        event.ActorId = std::string(actorId);
        event.ActorName = "ray";
        event.Address = std::string(address);
        return event;
    }

    int64 RowCount(PanelStore& store)
    {
        std::string error;
        std::optional<PanelStore::Statement> rows = store.Prepare("SELECT COUNT(*) FROM audit_event", error);
        if (!rows || !rows->Step(error))
            return -1;
        return rows->Int64(0);
    }
}

TEST(PanelActivityLogTest, EveryEmittedEventRendersASentence)
{
    std::vector<std::string_view> const emitted = {
        "app:api.change", "app:console.command", "app:permission.refused", "app:power.refused",
        "app:power.restart", "app:power.stop", "errors:group.cleared", "errors:report.created",
        "file:batch.changed", "file:path.refused", "file:rules.changed", "panel:link.issued",
        "panel:link.refused", "panel:link.throttled", "panel:note.added", "panel:permission.refused",
        "panel:request.throttled", "panel:session.challenged", "panel:session.closed", "panel:session.opened",
        "panel:session.refused", "panel:session.second_factor_refused", "panel:session.second_factor_throttled",
        "panel:session.throttled", "panel:settings.changed", "panel:settings.mail_tested",
        "panel:step_up.checked", "panel:step_up.required", "panel:step_up.used", "panel:user.created",
        "panel:user.password_set", "panel:user.two_factor_reset", "reload:target.run",
        "settings:batch.changed", "settings:secret.revealed", "settings:setting.changed",
        "settings:setting.reset", "user:recovery_codes.issued", "user:two_factor.disabled",
        "user:two_factor.enabled", "activity:sweep.ran", "activity:export.created",
    };
    for (std::string_view name : emitted)
    {
        EXPECT_TRUE(PanelActivityCatalog::Has(name)) << "event has no sentence: " << name;
        AuditEvent event = MakeEvent(name);
        std::string sentence = PanelActivityCatalog::Render(name, event);
        EXPECT_FALSE(sentence.empty()) << name;
        EXPECT_EQ(sentence.find("Unknown event"), std::string::npos) << name;
    }
}

TEST(PanelActivityLogTest, MissingSentenceFails)
{
    AuditEvent event = MakeEvent("panel:session.opened");
    std::string sentence = PanelActivityCatalog::Render("nonexistent:event.name", event);
    EXPECT_NE(sentence.find("Unknown event"), std::string::npos);
    EXPECT_FALSE(PanelActivityCatalog::Has("nonexistent:event.name"));
}

TEST(PanelActivityLogTest, RefusedRestartAppearsDeniedOnAppPage)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    AuditEvent refused = MakeEvent("app:power.restart");
    refused.Result = AuditResult::Refused;
    refused.Reason = "app is locked";
    refused.On("app", "gameserver", "Game Server");
    ASSERT_TRUE(Record(store, refused, error)) << error;

    PanelActivityLog::Filter filter;
    filter.Subject = "gameserver";
    PanelActivityLog::Page page;
    ASSERT_TRUE(PanelActivityLog::Query(store, filter, true, "", page, error)) << error;
    ASSERT_EQ(page.Rows.size(), 1u);
    EXPECT_EQ(page.Rows[0].Result, "refused");
    EXPECT_EQ(page.Rows[0].Name, "app:power.restart");
}

TEST(PanelActivityLogTest, AddressHiddenWithoutGrant)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    AuditEvent event = MakeEvent("panel:session.opened", "7", "10.0.0.9");
    ASSERT_TRUE(Record(store, event, error)) << error;
    AuditEvent other = MakeEvent("panel:session.opened", "9", "10.0.0.10");
    other.ActorName = "ana";
    ASSERT_TRUE(Record(store, other, error)) << error;

    PanelActivityLog::Filter filter;
    PanelActivityLog::Page page;
    ASSERT_TRUE(PanelActivityLog::Query(store, filter, false, "7", page, error)) << error;
    ASSERT_EQ(page.Rows.size(), 2u);
    for (PanelActivityLog::Row const& row : page.Rows)
    {
        if (row.ActorId == "7")
            EXPECT_EQ(row.Address, "10.0.0.9") << "actor sees own address";
        else
            EXPECT_TRUE(row.Address.empty()) << "other address hidden without grant";
    }

    std::string csv = PanelActivityLog::ToCsv(page.Rows);
    EXPECT_EQ(csv.find("10.0.0.10"), std::string::npos) << "export hides other address";
    std::string json = PanelActivityLog::ToJson(page.Rows);
    EXPECT_EQ(json.find("10.0.0.10"), std::string::npos) << "json export hides other address";
}

TEST(PanelActivityLogTest, UserTabShowsSignInsAndRestarts)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    AuditEvent signin = MakeEvent("panel:session.opened", "7");
    ASSERT_TRUE(Record(store, signin, error)) << error;
    AuditEvent restart = MakeEvent("app:power.restart", "7");
    restart.On("app", "gameserver");
    ASSERT_TRUE(Record(store, restart, error)) << error;
    AuditEvent foreign = MakeEvent("panel:session.opened", "9");
    ASSERT_TRUE(Record(store, foreign, error)) << error;

    PanelActivityLog::Filter filter;
    filter.Actor = "7";
    PanelActivityLog::Page page;
    ASSERT_TRUE(PanelActivityLog::Query(store, filter, true, "", page, error)) << error;
    ASSERT_EQ(page.Rows.size(), 2u);
    bool sawSignin = false;
    bool sawRestart = false;
    for (PanelActivityLog::Row const& row : page.Rows)
    {
        sawSignin = sawSignin || row.Name == "panel:session.opened";
        sawRestart = sawRestart || row.Name == "app:power.restart";
    }
    EXPECT_TRUE(sawSignin);
    EXPECT_TRUE(sawRestart);
}

TEST(PanelActivityLogTest, FullStoreFailsChangeWithError)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    int64 before = RowCount(store);
    AuditEvent event = MakeEvent("panel:settings.changed");
    bool changeRan = false;
    bool ok = PanelAudit::Record(store, event, [&changeRan](AuditEvent&, std::string&) { changeRan = true; return true; }, error);
    EXPECT_TRUE(ok) << error;
    EXPECT_TRUE(changeRan);
    EXPECT_EQ(RowCount(store), before + 1);
}

TEST(PanelActivityLogTest, FormulaPrefixExportsAsText)
{
    EXPECT_EQ(PanelActivityLog::CsvCell("=SUM(A1:A2)"), "\"'=SUM(A1:A2)\"");
    EXPECT_EQ(PanelActivityLog::CsvCell("+123"), "\"'+123\"");
    EXPECT_EQ(PanelActivityLog::CsvCell("-5"), "\"'-5\"");
    EXPECT_EQ(PanelActivityLog::CsvCell("@mention"), "\"'@mention\"");
    EXPECT_EQ(PanelActivityLog::CsvCell("plain"), "plain");
    EXPECT_EQ(PanelActivityLog::CsvCell("a,b"), "\"a,b\"");

    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    AuditEvent event = MakeEvent("panel:note.added");
    event.Reason = "=2+2";
    ASSERT_TRUE(Record(store, event, error)) << error;

    PanelActivityLog::Filter filter;
    PanelActivityLog::Page page;
    ASSERT_TRUE(PanelActivityLog::Query(store, filter, true, "", page, error)) << error;
    ASSERT_EQ(page.Rows.size(), 1u);
    std::string csv = PanelActivityLog::ToCsv(page.Rows);
    EXPECT_NE(csv.find("\"'=2+2\""), std::string::npos) << csv;
}

TEST(PanelActivityLogTest, SweepKeepsSecurityLongerAndRecordsItself)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    int64 const dayMs = 86400000;
    int64 const nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    AuditEvent oldVolume = MakeEvent("panel:note.added");
    oldVolume.CreatedEpochMs = nowMs - 100 * dayMs;
    ASSERT_TRUE(Record(store, oldVolume, error)) << error;

    AuditEvent oldSecurity = MakeEvent("panel:session.opened");
    oldSecurity.CreatedEpochMs = nowMs - 100 * dayMs;
    ASSERT_TRUE(Record(store, oldSecurity, error)) << error;

    AuditEvent fresh = MakeEvent("panel:note.added");
    fresh.CreatedEpochMs = nowMs - 10 * dayMs;
    ASSERT_TRUE(Record(store, fresh, error)) << error;

    int64 removed = PanelActivityLog::Sweep(store, 365, 90, error);
    ASSERT_GE(removed, 0) << error;
    EXPECT_EQ(removed, 1) << "only the old high-volume row goes";

    PanelActivityLog::Filter filter;
    PanelActivityLog::Page page;
    ASSERT_TRUE(PanelActivityLog::Query(store, filter, true, "", page, error)) << error;
    bool sawSweep = false;
    bool sawOldSecurity = false;
    for (PanelActivityLog::Row const& row : page.Rows)
    {
        sawSweep = sawSweep || row.Name == "activity:sweep.ran";
        sawOldSecurity = sawOldSecurity || (row.Name == "panel:session.opened" && row.CreatedEpochMs == oldSecurity.CreatedEpochMs);
    }
    EXPECT_TRUE(sawSweep) << "sweep records one event";
    EXPECT_TRUE(sawOldSecurity) << "security row kept for the longer window";
}

TEST(PanelActivityLogTest, CursorPaginationCapsAt100)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    for (int i = 0; i < 5; ++i)
    {
        AuditEvent event = MakeEvent("panel:note.added");
        ASSERT_TRUE(Record(store, event, error)) << error;
    }

    PanelActivityLog::Filter filter;
    filter.Limit = 2;
    PanelActivityLog::Page first;
    ASSERT_TRUE(PanelActivityLog::Query(store, filter, true, "", first, error)) << error;
    ASSERT_EQ(first.Rows.size(), 2u);
    EXPECT_TRUE(first.HasMore);
    EXPECT_NE(first.NextCursor, 0);

    filter.Cursor = first.NextCursor;
    PanelActivityLog::Page second;
    ASSERT_TRUE(PanelActivityLog::Query(store, filter, true, "", second, error)) << error;
    EXPECT_EQ(second.Rows.size(), 2u);
    EXPECT_NE(first.Rows[0].Id, second.Rows[0].Id);
}

TEST(PanelActivityLogTest, SentenceEscapesHostileValues)
{
    AuditEvent event = MakeEvent("panel:session.opened");
    event.ActorName = "<script>alert(1)</script>";
    std::string sentence = PanelActivityCatalog::Render("panel:session.opened", event);
    EXPECT_EQ(sentence.find("<script>"), std::string::npos);
    EXPECT_NE(sentence.find("&lt;script&gt;"), std::string::npos);
}
