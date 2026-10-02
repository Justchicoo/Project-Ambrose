/*
 * Project Ambrose by Imjustchico
 * Tests the activity pages against a real panel listener and its store: a restart refused to a viewer is on the app's page with result denied, an operator's own tab holds their sign-ins and the restarts they ran, a user without activity.ip.read sees only their own address in rows and exports, pages are cursored at no more than 100 rows, a store filled to Panel.StoreMaxMegabytes refuses a settings change rather than applying it unrecorded, a reason that starts like a formula exports as text, and the hourly sweep removes each class's rows past its live retention, keeps the chain's anchors and records itself once.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelActivity.h"
#include "PanelAudit.h"
#include "PanelTestSession.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace PanelTest;

namespace
{
    constexpr int64 DayMs = 86400000;

    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class PanelActivityTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
        }

        void TearDown() override
        {
            if (_panel)
                _panel->Stop();
        }

        void Begin(std::string const& extra = {})
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n" + extra)).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            _panel->Routes().AddDynamicGuardedPrefix("POST", "/api/apps/", "status.read", [](AdminRequest const&) { return std::string("power.restart"); },
                [this](AdminRequest const& request)
                {
                    return _panel->AuditRequest(request, "gameserver", "app:power.restart", [] { return AdminResponse::Json(202, R"({"accepted":true})"); });
                });
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
        }

        int64 MakeViewer(Browser& browser, std::string const& name = "viewer")
        {
            std::string error;
            int64 id = 0;
            EXPECT_EQ(_panel->Users().Create(name, Password, false, false, &id, error), PanelUserResult::Ok) << error;
            EXPECT_EQ(SignIn(*_panel, browser, name).Status, 200);
            return id;
        }

        AdminClientResponse Restart(Browser const& browser)
        {
            return Send(*_panel, "POST", "/api/apps/gameserver/power", { { "action", "restart" } }, &browser);
        }

        nlohmann::json Rows(Browser const& browser, std::string const& path)
        {
            AdminClientResponse const answer = Send(*_panel, "GET", path, nullptr, &browser);
            EXPECT_EQ(answer.Status, 200) << path << ": " << answer.Body;
            nlohmann::json const body = Json(answer);
            return body.is_object() && body.contains("rows") ? body["rows"] : nlohmann::json::array();
        }

        static std::set<std::string> Names(nlohmann::json const& rows)
        {
            std::set<std::string> names;
            for (nlohmann::json const& row : rows)
                names.insert(row["name"].get<std::string>());
            return names;
        }

        AuditEvent Aged(std::string const& name, int64 ageMs, std::string const& eventId)
        {
            AuditEvent event;
            event.EventId = eventId;
            event.Name = name;
            event.Actor = AuditActor::System;
            event.CreatedEpochMs = PanelStore::NowEpochMs() - ageMs;
            return event;
        }

        bool Exists(std::string const& eventId)
        {
            std::string error;
            std::optional<PanelStore::Statement> row = _panel->Store().Prepare("SELECT 1 FROM audit_event WHERE event_id = ?", error);
            if (!row)
                return false;
            row->Bind(1, eventId);
            return row->Step(error);
        }

        int64 Scalar(std::string const& sql)
        {
            std::string error;
            std::optional<PanelStore::Statement> row = _panel->Store().Prepare(sql, error);
            if (!row || !row->Step(error))
                return -1;
            return row->Int64(0);
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
    };
}

TEST_F(PanelActivityTest, ARefusedRestartAppearsOnTheAppsActivityPageWithResultDenied)
{
    Begin();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    Browser viewer;
    MakeViewer(viewer);

    AdminClientResponse const refused = Restart(viewer);
    EXPECT_TRUE(refused.Status == 403 || refused.Status == 404) << refused.Body;
    ASSERT_EQ(Restart(owner).Status, 202);

    nlohmann::json const rows = Rows(owner, "/api/panel/apps/gameserver/activity");
    bool denied = false;
    bool ran = false;
    for (nlohmann::json const& row : rows)
    {
        if (row["name"] == "app:permission.refused")
        {
            denied = true;
            EXPECT_EQ(row["result"], "denied");
            EXPECT_EQ(row["actor"]["name"], "viewer");
            EXPECT_EQ(row["sentence"], "viewer was refused power.restart on gameserver");
        }
        if (row["name"] == "app:power.restart")
        {
            ran = true;
            EXPECT_EQ(row["result"], "ok");
            EXPECT_EQ(row["sentence"], "owner restarted gameserver");
        }
        bool onTheApp = false;
        for (nlohmann::json const& subject : row["subjects"])
            onTheApp = onTheApp || (subject["kind"] == "app" && subject["id"] == "gameserver");
        EXPECT_TRUE(onTheApp) << row.dump();
    }
    EXPECT_TRUE(denied) << rows.dump();
    EXPECT_TRUE(ran) << rows.dump();

    nlohmann::json const deniedOnly = Rows(owner, "/api/panel/apps/gameserver/activity?result=denied");
    ASSERT_FALSE(deniedOnly.empty());
    for (nlohmann::json const& row : deniedOnly)
        EXPECT_EQ(row["result"], "denied");
}

TEST_F(PanelActivityTest, AUsersActivityTabShowsTheirSignInsAndTheRestartsTheyRan)
{
    Begin();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_EQ(Restart(owner).Status, 202);
    Browser again;
    ASSERT_EQ(SignIn(*_panel, again, "owner").Status, 200);
    Browser viewer;
    int64 const viewerId = MakeViewer(viewer);

    std::set<std::string> const own = Names(Rows(again, "/api/panel/me/activity"));
    EXPECT_TRUE(own.contains("panel:session.opened"));
    EXPECT_TRUE(own.contains("app:power.restart"));

    std::set<std::string> const byFilter = Names(Rows(owner, "/api/panel/activity?user=" + std::to_string(owner.UserId)));
    EXPECT_TRUE(byFilter.contains("panel:session.opened"));
    EXPECT_TRUE(byFilter.contains("app:power.restart"));

    nlohmann::json const viewers = Rows(viewer, "/api/panel/me/activity");
    ASSERT_FALSE(viewers.empty());
    for (nlohmann::json const& row : viewers)
    {
        EXPECT_NE(row["name"], "app:power.restart") << "the viewer ran no restart";
        bool involved = row["actor"]["id"] == std::to_string(viewerId);
        for (nlohmann::json const& subject : row["subjects"])
            involved = involved || (subject["kind"] == "panel_user" && subject["id"] == std::to_string(viewerId));
        EXPECT_TRUE(involved) << row.dump();
    }
}

TEST_F(PanelActivityTest, AUserWithoutIpReadSeesNoOtherUsersAddressInRowsOrExports)
{
    Begin();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_EQ(Restart(owner).Status, 202);
    Browser viewer;
    int64 const viewerId = MakeViewer(viewer);

    AdminClientResponse const page = Send(*_panel, "GET", "/api/panel/activity", nullptr, &viewer);
    ASSERT_EQ(page.Status, 200) << page.Body;
    nlohmann::json const body = Json(page);
    EXPECT_FALSE(body["sees_addresses"].get<bool>());
    bool sawOwn = false;
    bool sawOther = false;
    for (nlohmann::json const& row : body["rows"])
    {
        bool const mine = row["actor"]["type"] == "user" && row["actor"]["id"] == std::to_string(viewerId);
        if (mine)
        {
            sawOwn = true;
            EXPECT_EQ(row["address"], "127.0.0.1") << row.dump();
        }
        else
        {
            sawOther = true;
            EXPECT_TRUE(row["address"].is_null()) << row.dump();
            EXPECT_TRUE(row["user_agent"].is_null()) << row.dump();
        }
    }
    EXPECT_TRUE(sawOwn);
    EXPECT_TRUE(sawOther);

    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/activity?address=127.0.0.1", nullptr, &viewer).Status, 403)
        << "filtering by address would tell a viewer where others signed in from";
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/activity/export", nullptr, &viewer).Status, 403);

    nlohmann::json const ownerRows = Rows(owner, "/api/panel/activity");
    for (nlohmann::json const& row : ownerRows)
        if (row["actor"]["type"] == "user")
            EXPECT_EQ(row["address"], "127.0.0.1") << "an owner sees every address";

    std::vector<AuditEvent> events(2);
    events[0].Name = "panel:session.opened";
    events[0].Actor = AuditActor::User;
    events[0].ActorId = std::to_string(owner.UserId);
    events[0].Address = "198.51.100.7";
    events[1].Name = "panel:session.opened";
    events[1].Actor = AuditActor::User;
    events[1].ActorId = "user:" + std::to_string(viewerId);
    events[1].Address = "198.51.100.8";
    std::string const viewerExport = PanelActivity::Csv(events, [viewerId](AuditEvent const& row)
    {
        return PanelActivity::ShowsAddress(row, std::to_string(viewerId), false);
    });
    EXPECT_EQ(viewerExport.find("198.51.100.7"), std::string::npos) << viewerExport;
    EXPECT_NE(viewerExport.find("198.51.100.8"), std::string::npos) << viewerExport;
}

TEST_F(PanelActivityTest, PagesHoldAtMostOneHundredRowsBehindACursor)
{
    Begin();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::string error;
    for (int index = 0; index < 130; ++index)
        ASSERT_TRUE(PanelAudit::Write(_panel->Store(), Aged("app:console.command", 1000, "filler-" + std::to_string(index)), error)) << error;

    AdminClientResponse const first = Send(*_panel, "GET", "/api/panel/activity?prefix=app:console&limit=100", nullptr, &owner);
    ASSERT_EQ(first.Status, 200) << first.Body;
    nlohmann::json const page = Json(first);
    ASSERT_EQ(page["rows"].size(), 100u);
    ASSERT_TRUE(page["next_cursor"].is_string());
    nlohmann::json const rest = Rows(owner, "/api/panel/activity?prefix=app:console&limit=100&cursor=" + page["next_cursor"].get<std::string>());
    EXPECT_EQ(rest.size(), 30u);
    EXPECT_LT(rest[0]["id"].get<int64>(), page["rows"][99]["id"].get<int64>());

    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/activity?limit=101", nullptr, &owner).Status, 422);
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/activity?result=maybe", nullptr, &owner).Status, 422);
    EXPECT_EQ(Send(*_panel, "GET", "/api/panel/activity?subject=gameserver", nullptr, &owner).Status, 422);
}

TEST_F(PanelActivityTest, AFullStoreMakesASettingsChangeFailRatherThanApplyUnaudited)
{
    Begin("Panel.StoreMaxMegabytes = 1\n");
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    std::string error;
    ASSERT_TRUE(_panel->Store().Execute("CREATE TABLE filler (data BLOB)", error)) << error;
    for (int64 const size : { 65536, 4096, 512, 64, 8 })
        for (int attempt = 0; attempt < 400; ++attempt)
            if (!_panel->Store().Execute(fmt::format("INSERT INTO filler VALUES (zeroblob({}))", size), error))
                break;
    EXPECT_FALSE(_panel->Store().Execute("INSERT INTO filler VALUES (zeroblob(65536))", error)) << "the store is filled to its limit";
    EXPECT_LE(_panel->Store().GetSizeBytes(), 1024u * 1024u);

    AdminClientResponse const changed = Send(*_panel, "PATCH", "/api/panel/settings", { { "values", { { "Panel.Name", "Unrecorded" } } } }, &owner);
    EXPECT_GE(changed.Status, 400) << changed.Body;
    EXPECT_NE(changed.Status, 401) << changed.Body;
    EXPECT_EQ(_panel->Settings().ValueOf("Panel.Name"), "Ambrose") << "nothing changed without its record";
    EXPECT_EQ(Scalar("SELECT COUNT(*) FROM audit_event WHERE name = 'panel:settings.changed' AND result = 'succeeded'"), 0);
}

TEST_F(PanelActivityTest, AReasonThatStartsLikeAFormulaExportsAsText)
{
    Begin();
    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    AuditEvent event = Aged("app:power.restart", 0, "formula");
    event.Reason = "=HYPERLINK(\"http://example.invalid\",\"open\")";
    event.On("app", "gameserver", "gameserver");
    std::string error;
    ASSERT_TRUE(PanelAudit::Write(_panel->Store(), event, error)) << error;

    AdminClientResponse const exported = Send(*_panel, "GET", "/api/panel/activity/export?format=csv&prefix=app:power", nullptr, &owner);
    ASSERT_EQ(exported.Status, 200) << exported.Body;
    EXPECT_NE(exported.ContentType.find("text/csv"), std::string::npos);
    EXPECT_NE(exported.Body.find("\"'=HYPERLINK(\"\"http://example.invalid\"\",\"\"open\"\")\""), std::string::npos) << exported.Body;
    EXPECT_EQ(exported.Body.find(",\"=HYPERLINK"), std::string::npos) << exported.Body;
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:activity.exported"), 1);

    EXPECT_EQ(PanelActivity::CsvField("+1"), "\"'+1\"");
    EXPECT_EQ(PanelActivity::CsvField("-2"), "\"'-2\"");
    EXPECT_EQ(PanelActivity::CsvField("@SUM(A1)"), "\"'@SUM(A1)\"");
    EXPECT_EQ(PanelActivity::CsvField("\tcell"), "\"'\tcell\"");
    EXPECT_EQ(PanelActivity::CsvField("plain"), "\"plain\"");

    AdminClientResponse const json = Send(*_panel, "GET", "/api/panel/activity/export?format=json&prefix=app:power", nullptr, &owner);
    ASSERT_EQ(json.Status, 200) << json.Body;
    EXPECT_EQ(Json(json)["rows"][0]["reason"], event.Reason);
}

TEST_F(PanelActivityTest, AnHourlySweepRemovesRowsPastTheirClassRetentionAndRecordsItself)
{
    Begin();
    std::string error;
    ASSERT_TRUE(PanelAudit::Write(_panel->Store(), Aged("panel:session.opened", 400 * DayMs, "old-security"), error)) << error;
    ASSERT_TRUE(PanelAudit::Write(_panel->Store(), Aged("panel:session.opened", 100 * DayMs, "kept-security"), error)) << error;
    ASSERT_TRUE(PanelAudit::Write(_panel->Store(), Aged("app:console.command", 100 * DayMs, "old-high-volume"), error)) << error;
    ASSERT_TRUE(PanelAudit::Write(_panel->Store(), Aged("app:console.command", 10 * DayMs, "kept-high-volume"), error)) << error;

    PanelActivitySweep removed;
    ASSERT_TRUE(_panel->SweepActivity(PanelStore::NowEpochMs(), removed, error)) << error;
    EXPECT_EQ(removed.Security, 1);
    EXPECT_EQ(removed.HighVolume, 1);
    EXPECT_EQ(removed.Anchored, 2);
    EXPECT_FALSE(Exists("old-security"));
    EXPECT_FALSE(Exists("old-high-volume"));
    EXPECT_TRUE(Exists("kept-security")) << "security rows keep the longer window";
    EXPECT_TRUE(Exists("kept-high-volume"));
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:activity.swept"), 1);
    EXPECT_EQ(Scalar("SELECT COUNT(*) FROM audit_pruned"), 2);
    EXPECT_EQ(Scalar("SELECT json_extract(properties, '$.removed') FROM audit_event WHERE name = 'panel:activity.swept'"), 2);

    Browser owner;
    ASSERT_TRUE(ClaimOwner(*_panel, _harness, owner));
    ASSERT_EQ(Send(*_panel, "PATCH", "/api/panel/settings", { { "values", { { "Panel.ActivityHighVolumeRetentionDays", "5" } } } }, &owner).Status, 200);
    ASSERT_TRUE(_panel->SweepActivity(PanelStore::NowEpochMs(), removed, error)) << error;
    EXPECT_EQ(removed.HighVolume, 1) << "a shorter live window applies on the next sweep";
    EXPECT_FALSE(Exists("kept-high-volume"));
    EXPECT_TRUE(Exists("kept-security"));
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:activity.swept"), 2);
}
