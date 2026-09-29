/*
 * Project Ambrose by Imjustchico
 * Tests the panel's own listener and what it holds: it serves nothing until Panel.Enable is set, it opens its store with the panel tables before it listens, it answers its own routes on a loopback port with its own token, a bind beyond this machine with no certificate is refused with the Panel option names in the message, the plain-HTTP opt-in lifts that refusal, a certificate and key are served over TLS with the fingerprint the files hold, a route that declares a cost is held back with a retry hint while an uncosted route from the same caller still answers, one audit row records the throttling however many requests are refused in that minute, and a change whose audit row cannot be written is not applied, the plain-HTTP opt-in lets it reach beyond this machine with the risk said out loud, a reload that would leave the bind unsafe is refused while the old listener goes on serving, and a replaced certificate is served after a reload on the same port, and it refuses to start at all when a route says neither which permission it needs nor that any signed-in member may call it, or names a permission the catalog does not hold; and a relayed settings change, reset, batch, reload or reveal is recorded with who, where, why and how it ended, a dry run not at all and a reveal naming a key with only the keys it showed, a refused change too, but never a value.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelErrors.h"
#include "PanelAudit.h"
#include "PanelStore.h"
#include "TlsCertificate.h"

#include <fmt/format.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class PanelTest : public testing::Test
    {
    protected:
        ConfigMgr& Configured(std::string const& text)
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            EXPECT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", text)).Succeeded());
            return *_config;
        }

        Panel Make()
        {
            return Panel(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
        }

        void AddPing(Panel& panel)
        {
            panel.Routes().AddOpen("GET", "/api/panel/ping", [](AdminRequest const&)
            {
                return AdminResponse::Json(200, "{\"pong\":true}");
            });
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
    };
}

TEST_F(PanelTest, ServesNothingUntilPanelEnableIsSet)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 0\n"), error)) << error;
    EXPECT_FALSE(panel.IsRunning());
    EXPECT_FALSE(panel.Store().IsOpen());
}

TEST_F(PanelTest, OpensItsStoreAndAnswersItsOwnRoutesOnLoopback)
{
    Panel panel = Make();
    AddPing(panel);
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    EXPECT_TRUE(panel.IsRunning());
    EXPECT_FALSE(panel.IsSecure());
    EXPECT_EQ(panel.GetBindIp(), "127.0.0.1");
    ASSERT_NE(panel.GetPort(), 0);

    ASSERT_TRUE(panel.Store().IsOpen());
    EXPECT_TRUE(std::filesystem::exists(_directory.Path() / "data" / "panel" / "panel.sqlite3"));
    std::optional<PanelStore::Statement> tables = panel.Store().Prepare(
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name IN ('audit_event', 'audit_subject', 'panel_session', 'panel_command_history')", error);
    ASSERT_TRUE(tables.has_value()) << error;
    ASSERT_TRUE(tables->Step(error)) << error;
    EXPECT_EQ(tables->Int64(0), 4);
    tables.reset();

    AdminClient const anonymous("127.0.0.1", panel.GetPort(), "");
    EXPECT_EQ(anonymous.Send({ "GET", "/api/panel/ping", "", "application/json", "" }, std::chrono::seconds(10)).Status, 401);

    AdminClient const signedIn("127.0.0.1", panel.GetPort(), panel.GetToken());
    AdminClientResponse const answer = signedIn.Send({ "GET", "/api/panel/ping", "", "application/json", "" }, std::chrono::seconds(10));
    ASSERT_TRUE(answer.Answered) << answer.Error;
    EXPECT_EQ(answer.Status, 200) << answer.Body;
    EXPECT_EQ(answer.Body, "{\"pong\":true}");

    panel.Stop();
    EXPECT_FALSE(panel.IsRunning());
    EXPECT_FALSE(panel.Store().IsOpen());
}

TEST_F(PanelTest, RefusesABindBeyondThisMachineInThePanelsOwnOptionNames)
{
    Panel panel = Make();
    std::string error;
    EXPECT_FALSE(panel.Start(Configured("Panel.Enable = 1\nPanel.BindIP = 0.0.0.0\nPanel.Port = 0\n"), error));
    EXPECT_FALSE(panel.IsRunning());
    EXPECT_NE(error.find("Panel.BindIP"), std::string::npos) << error;
    EXPECT_NE(error.find("Panel.CertificateFile"), std::string::npos) << error;
    EXPECT_NE(error.find("Panel.AllowPlainHttpRemote"), std::string::npos) << error;
    EXPECT_NE(error.find("session cookies"), std::string::npos) << error;
    EXPECT_EQ(error.find("Admin."), std::string::npos) << error;

    ListenerSettings const opted = Panel::LoadSettings(Configured("Panel.Enable = 1\nPanel.BindIP = 0.0.0.0\nPanel.AllowPlainHttpRemote = 1\n"));
    EXPECT_FALSE(opted.RemoteAccessError().has_value());
    ASSERT_EQ(opted.Warnings().size(), 1u);
    EXPECT_NE(opted.Warnings().front().find("Panel.AllowPlainHttpRemote"), std::string::npos) << opted.Warnings().front();
}

TEST_F(PanelTest, ServesTlsWithTheCertificateItWasGiven)
{
    std::filesystem::path const certificate = _directory.Path() / "panel.crt";
    std::filesystem::path const key = _directory.Path() / "panel.key";
    std::string error;
    ASSERT_TRUE(TlsCertificate::CreateSelfSigned(certificate, key, "Ambrose panel", { "localhost", "127.0.0.1" }, 60, error)) << error;
    TlsCertificate served;
    ASSERT_TRUE(served.Load(certificate, key, error)) << error;

    Panel panel = Make();
    AddPing(panel);
    ASSERT_TRUE(panel.Start(Configured(fmt::format("Panel.Enable = 1\nPanel.Port = 0\nPanel.CertificateFile = \"{}\"\nPanel.PrivateKeyFile = \"{}\"\n",
        certificate.generic_string(), key.generic_string())), error)) << error;
    EXPECT_TRUE(panel.IsSecure());

    AdminClient const client("127.0.0.1", panel.GetPort(), panel.GetToken(), true);
    AdminClientResponse const answer = client.Send({ "GET", "/api/panel/ping", "", "application/json", "" }, std::chrono::seconds(10));
    ASSERT_TRUE(answer.Answered) << answer.Error;
    EXPECT_EQ(answer.Status, 200) << answer.Body;
    EXPECT_EQ(answer.PeerFingerprint, served.GetInfo().Fingerprint);
    EXPECT_NE(answer.Head.find("Strict-Transport-Security"), std::string::npos) << answer.Head;
}

TEST_F(PanelTest, HoldsBackACostlyRouteAndRecordsItOnceAMinute)
{
    Panel panel = Make();
    AddPing(panel);
    panel.Routes().AddOpenCosting("POST", "/api/panel/work", 1, [](AdminRequest const&)
    {
        return AdminResponse::Json(200, "{\"done\":true}");
    });

    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\nPanel.RateLimitBurst = 120\nPanel.RateLimitPerSecond = 0\n"), error)) << error;

    AdminClient const client("127.0.0.1", panel.GetPort(), panel.GetToken());
    int answered = 0;
    int held = 0;
    std::string retryHint;
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        AdminClientResponse const answer = client.Send({ "POST", "/api/panel/work", "{}", "application/json", "" }, std::chrono::seconds(10));
        ASSERT_TRUE(answer.Answered) << answer.Error;
        if (answer.Status == 200)
        {
            ++answered;
            continue;
        }
        ASSERT_EQ(answer.Status, 429) << answer.Body;
        ++held;
        if (retryHint.empty())
        {
            std::size_t const at = answer.Head.find("Retry-After: ");
            ASSERT_NE(at, std::string::npos) << answer.Head;
            retryHint = answer.Head.substr(at + 13, answer.Head.find('\n', at) - at - 13);
            EXPECT_NE(answer.Body.find("too_many_requests"), std::string::npos) << answer.Body;
        }
    }
    EXPECT_EQ(answered, 120);
    EXPECT_EQ(held, 80);
    EXPECT_FALSE(retryHint.empty());

    AdminClientResponse const uncosted = client.Send({ "GET", "/api/panel/ping", "", "application/json", "" }, std::chrono::seconds(10));
    EXPECT_EQ(uncosted.Status, 200) << uncosted.Body;

    EXPECT_EQ(PanelAudit::Count(panel.Store(), "panel:request.throttled"), 1);
}

TEST_F(PanelTest, AChangeWhoseRecordCannotBeWrittenIsNotApplied)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    ASSERT_TRUE(panel.Store().Execute("CREATE TABLE note (id INTEGER PRIMARY KEY, body TEXT NOT NULL)", error)) << error;

    auto const insert = [&](std::string& failure)
    {
        return panel.Store().Execute("INSERT INTO note (body) VALUES ('kept')", failure);
    };

    AuditEvent named;
    named.Name = "panel:note.added";
    named.Actor = AuditActor::Token;
    named.Address = "127.0.0.1";
    named.On("note", "1", "kept");
    ASSERT_TRUE(panel.Record(named, insert, error)) << error;
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "panel:note.added"), 1);

    AuditEvent nameless;
    nameless.Actor = AuditActor::Token;
    EXPECT_FALSE(panel.Record(nameless, insert, error));
    EXPECT_NE(error.find("no name"), std::string::npos) << error;

    AuditEvent failing;
    failing.Name = "panel:note.added";
    EXPECT_FALSE(panel.Record(failing, [](std::string& failure) { failure = "the change refused itself"; return false; }, error));
    EXPECT_EQ(error, "the change refused itself");

    std::optional<PanelStore::Statement> rows = panel.Store().Prepare("SELECT COUNT(*) FROM note", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Int64(0), 1);
    rows.reset();
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "panel:note.added"), 1);

    ASSERT_TRUE(panel.Store().Execute("CREATE TRIGGER fail_audit BEFORE INSERT ON audit_event BEGIN SELECT RAISE(ABORT, 'forced audit failure'); END", error)) << error;
    AuditEvent forced;
    forced.Name = "panel:note.added";
    bool changed = false;
    EXPECT_FALSE(panel.Record(forced, [&](std::string& failure)
    {
        changed = true;
        return panel.Store().Execute("INSERT INTO note (body) VALUES ('not kept')", failure);
    }, error));
    EXPECT_FALSE(changed);
    rows = panel.Store().Prepare("SELECT COUNT(*) FROM note", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Int64(0), 1);
}

TEST_F(PanelTest, APanelRestartIsRecordedOnceWithItsUserAddressAppAndChainHash)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    int64 userId = 0;
    ASSERT_EQ(panel.Users().Create("operator", "a-long-test-password", false, false, &userId, error), PanelUserResult::Ok) << error;

    AdminRequest request;
    request.Method = "POST";
    request.Path = "/api/apps/gameserver/power";
    request.RemoteAddress = "203.0.113.18";
    request.UserAgent = "panel test";
    request.Principal = "user:" + std::to_string(userId);
    AdminResponse const restarted = panel.AuditRequest(request, "gameserver", "app:power.restart", []
    {
        return AdminResponse::Json(202, R"({"accepted":true})");
    });
    ASSERT_EQ(restarted.Status, 202) << restarted.Body;
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "app:power.restart"), 1);

    std::optional<PanelStore::Statement> row = panel.Store().Prepare(
        "SELECT id, actor_type, actor_id, actor_name, address, result, chain_hash FROM audit_event WHERE name = 'app:power.restart'", error);
    ASSERT_TRUE(row.has_value()) << error;
    ASSERT_TRUE(row->Step(error)) << error;
    int64 const eventId = row->Int64(0);
    EXPECT_EQ(row->Text(1), "user");
    EXPECT_EQ(row->Text(2), std::to_string(userId));
    EXPECT_EQ(row->Text(3), "operator");
    EXPECT_EQ(row->Text(4), "203.0.113.18");
    EXPECT_EQ(row->Text(5), "succeeded");
    EXPECT_EQ(row->Text(6).size(), 64u);
    row.reset();

    std::optional<PanelStore::Statement> subjects = panel.Store().Prepare(
        "SELECT kind, subject_id FROM audit_subject WHERE event = ? ORDER BY position", error);
    ASSERT_TRUE(subjects.has_value()) << error;
    subjects->Bind(1, eventId);
    ASSERT_TRUE(subjects->Step(error)) << error;
    EXPECT_EQ(subjects->Text(0), "panel_user");
    ASSERT_TRUE(subjects->Step(error)) << error;
    EXPECT_EQ(subjects->Text(0), "app");
    EXPECT_EQ(subjects->Text(1), "gameserver");
    EXPECT_FALSE(subjects->Step(error));
    EXPECT_TRUE(error.empty()) << error;
}

TEST_F(PanelTest, ARefusedOutOfScopeDangerousPermissionIsAuditedWithItsReason)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    int64 userId = 0;
    ASSERT_EQ(panel.Users().Create("viewer", "a-long-test-password", false, false, &userId, error), PanelUserResult::Ok) << error;

    AdminRequest request;
    request.Method = "POST";
    request.Path = "/api/apps/gameserver/power";
    request.RemoteAddress = "203.0.113.27";
    request.Principal = "user:" + std::to_string(userId);
    EXPECT_EQ(panel.Routes().MayI(request, "power.kill"), PermissionVerdict::OutOfScope);
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "app:permission.refused"), 1);

    std::optional<PanelStore::Statement> row = panel.Store().Prepare(
        "SELECT result, reason, properties FROM audit_event WHERE name = 'app:permission.refused'", error);
    ASSERT_TRUE(row.has_value()) << error;
    ASSERT_TRUE(row->Step(error)) << error;
    EXPECT_EQ(row->Text(0), "refused");
    EXPECT_NE(row->Text(1).find("no grant"), std::string::npos) << row->Text(1);
    EXPECT_NE(row->Text(2).find("power.kill"), std::string::npos) << row->Text(2);
}

TEST_F(PanelTest, ARefusedCommandIsAuditedWithoutRunningIt)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    int64 userId = 0;
    ASSERT_EQ(panel.Users().Create("operator", "a-long-test-password", false, false, &userId, error), PanelUserResult::Ok) << error;

    AdminRequest request;
    request.Method = "POST";
    request.Path = "/api/apps/gameserver/api/command";
    request.RemoteAddress = "203.0.113.31";
    request.Principal = "user:" + std::to_string(userId);
    bool ran = false;
    auto runAtLevel = [&ran](uint8 level)
    {
        if (level < 3)
            return AdminResponse::Problem(409, "command_refused", "there is no such command");
        ran = true;
        return AdminResponse::Json(200, "{}");
    };
    AdminResponse const refused = panel.AuditRequest(request, "gameserver", "app:console.command", [&] { return runAtLevel(2); });

    EXPECT_EQ(refused.Status, 409);
    EXPECT_FALSE(ran);
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "app:console.command"), 1);
    std::optional<PanelStore::Statement> row = panel.Store().Prepare(
        "SELECT result, reason FROM audit_event WHERE name = 'app:console.command'", error);
    ASSERT_TRUE(row.has_value()) << error;
    ASSERT_TRUE(row->Step(error)) << error;
    EXPECT_EQ(row->Text(0), "refused");
    EXPECT_EQ(row->Text(1), "there is no such command");
}

TEST_F(PanelTest, StoppedAppAndInvalidTokenRelayFailuresRemainDistinctInTheAuditRecord)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    int64 userId = 0;
    ASSERT_EQ(panel.Users().Create("operator", "a-long-test-password", false, false, &userId, error), PanelUserResult::Ok) << error;
    AdminRequest request;
    request.Method = "POST";
    request.Path = "/api/apps/gameserver/api/command";
    request.Principal = "user:" + std::to_string(userId);

    AdminResponse const stopped = panel.AuditRequest(request, "gameserver", "app:console.command", []
    {
        return AdminResponse::Problem(503, "app_not_running", "gameserver is offline");
    });
    AdminResponse const invalidToken = panel.AuditRequest(request, "gameserver", "app:console.command", []
    {
        return AdminResponse::Problem(502, "app_invalid_token", "gameserver refused the supervisor's app token");
    });
    EXPECT_EQ(nlohmann::json::parse(stopped.Body)["error"], "app_not_running");
    EXPECT_EQ(nlohmann::json::parse(invalidToken.Body)["error"], "app_invalid_token");

    std::optional<PanelStore::Statement> rows = panel.Store().Prepare(
        "SELECT result, error FROM audit_event WHERE name = 'app:console.command' ORDER BY id", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Text(0), "failed");
    EXPECT_EQ(nlohmann::json::parse(rows->Text(1))["error"], "app_not_running");
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Text(0), "failed");
    EXPECT_EQ(nlohmann::json::parse(rows->Text(1))["error"], "app_invalid_token");
    EXPECT_FALSE(rows->Step(error));
    EXPECT_TRUE(error.empty()) << error;
}

TEST_F(PanelTest, CommandHistoryIsStoredPerUserAndReturnsOnlyTheRedactedCommand)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    int64 userId = 0;
    ASSERT_EQ(panel.Users().Create("operator", "a-long-test-password", false, false, &userId, error), PanelUserResult::Ok) << error;
    AdminRequest request;
    request.Path = "/api/panel/apps/gameserver/command-history";
    request.Principal = "user:" + std::to_string(userId);
    ASSERT_TRUE(panel.StoreCommandHistoryWithinAudit(request, "gameserver", "account create (arguments hidden)", error)) << error;
    ASSERT_TRUE(panel.StoreCommandHistoryWithinAudit(request, "loginserver", "status", error)) << error;

    AdminResponse const history = panel.CommandHistoryGet(request);
    ASSERT_EQ(history.Status, 200) << history.Body;
    nlohmann::json const answer = nlohmann::json::parse(history.Body);
    ASSERT_EQ(answer["commands"].size(), 1u);
    EXPECT_EQ(answer["commands"][0], "account create (arguments hidden)");
    EXPECT_EQ(history.Body.find("hunter2"), std::string::npos);

    request.Principal = "user:999999";
    EXPECT_EQ(panel.CommandHistoryGet(request).Status, 403);
}

TEST_F(PanelTest, AForwardedHeaderChangesNothingWithNoTrustedProxies)
{
    Panel panel = Make();
    panel.Routes().AddOpenCosting("POST", "/api/panel/work", 200, [](AdminRequest const&)
    {
        return AdminResponse::Json(200, "{\"done\":true}");
    });

    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\nPanel.RateLimitBurst = 100\nPanel.RateLimitPerSecond = 0\nPanel.TrustedProxies =\n"), error)) << error;

    AdminClient const client("127.0.0.1", panel.GetPort(), panel.GetToken());
    AdminClientRequest request{ "POST", "/api/panel/work", "{}", "application/json", "" };
    request.Headers.push_back({ "X-Forwarded-For", "203.0.113.9" });
    AdminClientResponse const held = client.Send(request, std::chrono::seconds(10));
    EXPECT_EQ(held.Status, 429) << held.Body;

    std::optional<PanelStore::Statement> rows = panel.Store().Prepare("SELECT address FROM audit_event WHERE name = 'panel:request.throttled'", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Text(0), "127.0.0.1");
    EXPECT_FALSE(rows->Step(error));
    EXPECT_TRUE(error.empty()) << error;
}

TEST_F(PanelTest, StartsBeyondThisMachineWithThePlainHttpOptIn)
{
    std::optional<std::string> const address = Ambrose::GetEnv("AMBROSE_TEST_ADMIN_REMOTE_BIND");
    if (!address || address->empty())
        GTEST_SKIP() << "AMBROSE_TEST_ADMIN_REMOTE_BIND names no address to bind";

    _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured(fmt::format("Panel.Enable = 1\nPanel.Port = 0\nPanel.BindIP = {}\nPanel.AllowPlainHttpRemote = 1\n", *address)), error)) << error;
    EXPECT_TRUE(panel.IsRunning());

    std::vector<std::string> const lines = _harness.Store().Texts("Capture");
    EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [](std::string const& line)
    {
        return line.find("Panel.AllowPlainHttpRemote = 1") != std::string::npos && line.find("unencrypted") != std::string::npos;
    })) << lines.size() << " lines captured";
}

TEST_F(PanelTest, ARefusedReloadLeavesTheOldListenerServing)
{
    Panel panel = Make();
    AddPing(panel);
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    uint16 const port = panel.GetPort();
    ASSERT_NE(port, 0);

    EXPECT_FALSE(panel.Reload(Configured(fmt::format("Panel.Enable = 1\nPanel.Port = {}\nPanel.BindIP = 0.0.0.0\n", port))));
    EXPECT_TRUE(panel.IsRunning());
    EXPECT_EQ(panel.GetPort(), port);
    EXPECT_EQ(panel.GetBindIp(), "127.0.0.1");

    AdminClient const client("127.0.0.1", port, panel.GetToken());
    EXPECT_EQ(client.Send({ "GET", "/api/panel/ping", "", "application/json", "" }, std::chrono::seconds(10)).Status, 200);
}

TEST_F(PanelTest, AReloadSwapsTheCertificateWithNoRestart)
{
    std::filesystem::path const certificate = _directory.Path() / "panel.crt";
    std::filesystem::path const key = _directory.Path() / "panel.key";
    std::string error;
    ASSERT_TRUE(TlsCertificate::CreateSelfSigned(certificate, key, "Ambrose first", { "127.0.0.1" }, 60, error)) << error;

    Panel panel = Make();
    AddPing(panel);
    std::string const text = fmt::format("Panel.Enable = 1\nPanel.Port = 0\nPanel.CertificateFile = \"{}\"\nPanel.PrivateKeyFile = \"{}\"\n",
        certificate.generic_string(), key.generic_string());
    ASSERT_TRUE(panel.Start(Configured(text), error)) << error;
    uint16 const port = panel.GetPort();

    auto const fingerprintNow = [&]
    {
        AdminClient const client("127.0.0.1", port, panel.GetToken(), true);
        AdminClientResponse const answer = client.Send({ "GET", "/api/panel/ping", "", "application/json", "" }, std::chrono::seconds(10));
        EXPECT_EQ(answer.Status, 200) << answer.Error;
        return answer.PeerFingerprint;
    };

    std::filesystem::path const second = _directory.Path() / "second.crt";
    std::filesystem::path const secondKey = _directory.Path() / "second.key";
    ASSERT_TRUE(TlsCertificate::CreateSelfSigned(second, secondKey, "Ambrose second", { "127.0.0.1" }, 60, error)) << error;
    TlsCertificate replacement;
    ASSERT_TRUE(replacement.Load(second, secondKey, error)) << error;
    std::filesystem::copy_file(second, certificate, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(secondKey, key, std::filesystem::copy_options::overwrite_existing);

    std::string const again = fmt::format("Panel.Enable = 1\nPanel.Port = {}\nPanel.CertificateFile = \"{}\"\nPanel.PrivateKeyFile = \"{}\"\n",
        port, certificate.generic_string(), key.generic_string());
    ASSERT_TRUE(panel.Reload(Configured(again)));
    EXPECT_EQ(panel.GetPort(), port);
    EXPECT_EQ(fingerprintNow(), replacement.GetInfo().Fingerprint);
}

TEST_F(PanelTest, GatheringKeepsWhatAnAppReportedAndIgnoresAnAnswerItCannotRead)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;

    panel.SetErrorSource([]
    {
        return std::vector<std::pair<std::string, std::string>>{
            { "gameserver", R"({"schema":1,"dropped":0,"groups":[{"app":"gameserver","category":"server.database","file":"src/server/database/Pool.cpp","line":42,"function":"Open","template":"could not reach {}","level":"error","revision":"abc1234","count":3,"first_epoch_ms":1000,"last_epoch_ms":2000,"last_message":"could not reach the store"}]})" },
            { "patchserver", "this is not json at all" },
        };
    });

    EXPECT_EQ(panel.GatherErrorsOnce(), 1u) << "one group from the app that answered, none from the one that did not";

    std::vector<PanelErrorGroup> const kept = panel.Errors().List(error);
    ASSERT_EQ(kept.size(), 1u) << error;
    EXPECT_EQ(kept[0].App, "gameserver");
    EXPECT_EQ(kept[0].Line, 42u);
    EXPECT_EQ(kept[0].Template, "could not reach {}");
    EXPECT_EQ(kept[0].TotalCount, 3u);
    EXPECT_EQ(kept[0].LastMessage, "could not reach the store");

    EXPECT_EQ(panel.GatherErrorsOnce(), 1u);
    EXPECT_EQ(panel.Errors().List(error).size(), 1u) << "the same report twice is still one group";
    EXPECT_EQ(panel.Errors().List(error)[0].TotalCount, 3u) << "and the count is not doubled by reading it again";
    panel.Stop();
}

TEST_F(PanelTest, ErrorReportsPreviewPrivacyAndAuditExactlyTheSelectedGroups)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;
    std::string const verifier(64, 'a');
    std::string const secondVerifier(64, 'b');
    std::string const renderedText = "Account.VerifierKeys=1:" + verifier + ",2:" + secondVerifier +
        " Admin.Token=KnownSecret Login.Name=KnownAccountName";
    std::string const context = nlohmann::json({
        { "sequence", 77 },
        { "time", "2026-09-27T00:00:00Z" },
        { "epoch_ms", 1790467200000LL },
        { "level", "warn" },
        { "category", "server.database" },
        { "message", renderedText }
    }).dump();
    panel.SetErrorSource([context, renderedText]
    {
        nlohmann::json const before = nlohmann::json::parse(context);
        return std::vector<std::pair<std::string, std::string>>{
            { "loginserver", nlohmann::json({
                { "schema", 1 },
                { "groups", nlohmann::json::array({
                    {
                        { "category", "server.database" }, { "file", "src/server/database/Pool.cpp" }, { "line", 42 },
                        { "function", "Open" }, { "template", "could not open database" }, { "level", "error" },
                        { "revision", "login-revision" }, { "count", 2 }, { "first_epoch_ms", 1000 },
                        { "last_epoch_ms", 2000 },
                        { "last_message", renderedText },
                        { "context_before", nlohmann::json::array({ before }) }
                    }
                }) }
            }).dump() },
            { "gameserver", nlohmann::json({
                { "schema", 1 },
                { "groups", nlohmann::json::array({
                    {
                        { "category", "server.world" }, { "file", "src/server/game/World.cpp" }, { "line", 84 },
                        { "function", "Load" }, { "template", "world load failed" }, { "level", "error" },
                        { "revision", "game-revision" }, { "count", 1 }, { "first_epoch_ms", 3000 },
                        { "last_epoch_ms", 4000 }, { "last_message", "Login.Name=KnownAccountName" },
                        { "context_before", nlohmann::json::array() }
                    }
                }) }
            }).dump() }
        };
    });
    ASSERT_EQ(panel.GatherErrorsOnce(), 2u);

    int64 ownerId = 0;
    ASSERT_EQ(panel.Users().Create("report-owner", "a good long password", true, false, &ownerId, error), PanelUserResult::Ok) << error;
    std::optional<PanelSessionOpened> const ownerSession = panel.Sessions().Open(ownerId, 1, "127.0.0.1", "test", PanelStore::NowEpochMs(), error);
    ASSERT_TRUE(ownerSession.has_value()) << error;
    std::string const cookie = panel.Routes().GetBrowserAccess().CookieName + "=" + ownerSession->Secret;
    std::string const csrf = ownerSession->Csrf;
    AdminClient const client("127.0.0.1", panel.GetPort(), "");
    auto const sendAsOwner = [&client, &cookie, &csrf](std::string method, std::string path, std::string body)
    {
        AdminClientRequest request{ std::move(method), std::move(path), std::move(body), "application/json", "" };
        request.Headers.emplace_back("Cookie", cookie);
        request.Headers.emplace_back("Origin", fmt::format("http://127.0.0.1:{}", client.GetPort()));
        request.Headers.emplace_back("X-CSRF-Token", csrf);
        return client.Send(request, std::chrono::seconds(10));
    };

    AdminClientResponse const listed = sendAsOwner("GET", "/api/panel/errors", "");
    ASSERT_TRUE(listed.Answered) << listed.Error;
    ASSERT_EQ(listed.Status, 200) << listed.Body;
    nlohmann::json const listing = nlohmann::json::parse(listed.Body);
    ASSERT_EQ(listing["groups"].size(), 2u);
    std::vector<int64> ids;
    for (nlohmann::json const& group : listing["groups"])
        ids.push_back(group["id"].get<int64>());
    ASSERT_EQ(ids.size(), 2u);
    AdminClientResponse const cleared = sendAsOwner("POST", "/api/panel/errors/clear", nlohmann::json({ { "id", ids.front() } }).dump());
    ASSERT_TRUE(cleared.Answered) << cleared.Error;
    ASSERT_EQ(cleared.Status, 200) << cleared.Body;
    AdminClientResponse const afterClear = sendAsOwner("GET", "/api/panel/errors", "");
    ASSERT_TRUE(afterClear.Answered) << afterClear.Error;
    ASSERT_EQ(afterClear.Status, 200) << afterClear.Body;
    bool foundCleared = false;
    nlohmann::json const afterClearListing = nlohmann::json::parse(afterClear.Body);
    for (nlohmann::json const& group : afterClearListing["groups"])
        if (group["id"] == ids.front())
        {
            EXPECT_FALSE(group["new_since_cleared"].get<bool>());
            foundCleared = true;
        }
    EXPECT_TRUE(foundCleared);

    std::string const payload = nlohmann::json({ { "groups", ids }, { "include_rendered", false } }).dump();
    AdminClientResponse const preview = sendAsOwner("POST", "/api/panel/errors/report/preview", payload);
    ASSERT_TRUE(preview.Answered) << preview.Error;
    ASSERT_EQ(preview.Status, 200) << preview.Body;
    nlohmann::json const previewDocument = nlohmann::json::parse(preview.Body);
    nlohmann::json const safe = previewDocument["report"];
    ASSERT_EQ(safe["groups"].size(), 2u);
    EXPECT_EQ(safe["apps"]["loginserver"], "login-revision");
    EXPECT_EQ(safe["apps"]["gameserver"], "game-revision");
    bool foundLoginLocation = false;
    bool foundGameLocation = false;
    for (nlohmann::json const& group : safe["groups"])
    {
        if (group["app"] == "loginserver")
        {
            EXPECT_EQ(group["source"]["file"], "src/server/database/Pool.cpp");
            EXPECT_EQ(group["source"]["line"], 42);
            foundLoginLocation = true;
        }
        else if (group["app"] == "gameserver")
        {
            EXPECT_EQ(group["source"]["file"], "src/server/game/World.cpp");
            EXPECT_EQ(group["source"]["line"], 84);
            foundGameLocation = true;
        }
    }
    EXPECT_TRUE(foundLoginLocation);
    EXPECT_TRUE(foundGameLocation);
    EXPECT_EQ(safe.dump().find(verifier), std::string::npos);
    EXPECT_EQ(safe.dump().find(secondVerifier), std::string::npos);
    EXPECT_EQ(safe.dump().find("KnownSecret"), std::string::npos);
    EXPECT_EQ(safe.dump().find("KnownAccountName"), std::string::npos);
    EXPECT_EQ(safe.dump().find("rendered_message"), std::string::npos);
    EXPECT_EQ(safe.dump().find("log_lines_before"), std::string::npos);

    std::string const renderedPayload = nlohmann::json({ { "groups", ids }, { "include_rendered", true } }).dump();
    AdminClientResponse const rendered = sendAsOwner("POST", "/api/panel/errors/report/preview", renderedPayload);
    ASSERT_TRUE(rendered.Answered) << rendered.Error;
    ASSERT_EQ(rendered.Status, 200) << rendered.Body;
    nlohmann::json const renderedDocument = nlohmann::json::parse(rendered.Body);
    nlohmann::json const renderedReport = renderedDocument["report"];
    EXPECT_NE(renderedReport.dump().find("rendered_message"), std::string::npos);
    EXPECT_NE(renderedReport.dump().find("log_lines_before"), std::string::npos);
    EXPECT_EQ(renderedReport.dump().find(verifier), std::string::npos) << "verifier values remain redacted even when rendered text is selected";
    EXPECT_EQ(renderedReport.dump().find(secondVerifier), std::string::npos);
    EXPECT_EQ(renderedReport.dump().find("KnownSecret"), std::string::npos) << "setting tokens remain redacted when rendered text is selected";

    AdminClientResponse const created = sendAsOwner("POST", "/api/panel/errors/report", renderedPayload);
    ASSERT_TRUE(created.Answered) << created.Error;
    ASSERT_EQ(created.Status, 200) << created.Body;
    nlohmann::json const createdDocument = nlohmann::json::parse(created.Body);
    EXPECT_EQ(createdDocument["report"], renderedReport);
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "errors:report.created"), 1);
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "errors:group.cleared"), 1);
    panel.Stop();
}

TEST_F(PanelTest, APanelWillNotServeARouteThatSaysNothingAboutWhatItNeeds)
{
    ConfigMgr& config = Configured("Panel.Enable = 1\nPanel.Port = 0\n");
    Panel panel = Make();
    panel.Routes().Add("GET", "/api/panel/quiet", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });

    std::string error;
    EXPECT_FALSE(panel.Start(config, error));
    EXPECT_NE(error.find("/api/panel/quiet"), std::string::npos) << error;
}

TEST_F(PanelTest, ARouteNamingAPermissionTheCatalogDoesNotHoldIsNeverRegistered)
{
    ConfigMgr& config = Configured("Panel.Enable = 1\nPanel.Port = 0\n");
    Panel panel = Make();
    panel.Routes().AddGuarded("GET", "/api/panel/invented", "console.everything", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    EXPECT_FALSE(panel.Routes().Has("GET", "/api/panel/invented"));

    panel.Routes().AddGuarded("GET", "/api/panel/real", "status.read", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });
    EXPECT_TRUE(panel.Routes().Has("GET", "/api/panel/real"));

    std::string error;
    EXPECT_TRUE(panel.Start(config, error)) << error;
    panel.Stop();
}

TEST_F(PanelTest, RelayedSettingsChangesReloadsAndRevealsAreRecordedWithoutTheirValues)
{
    Panel panel = Make();
    std::string error;
    ASSERT_TRUE(panel.Start(Configured("Panel.Enable = 1\nPanel.Port = 0\n"), error)) << error;

    AdminRequest request;
    request.Principal = "token";
    request.RemoteAddress = "127.0.0.1";
    request.Id = "relayed-request";
    request.Body = R"({"value":"100","reason":"faster ticks"})";
    panel.RecordRelayed(request, "gameserver", "PUT", "/api/settings/World.UpdateInterval", 200, R"({"changed":true,"value":"100"})");
    request.Body = R"({"value":"0","reason":"too fast"})";
    panel.RecordRelayed(request, "gameserver", "PUT", "/api/settings/World.UpdateInterval", 422, R"({"error":"invalid","message":"World.UpdateInterval was not changed"})");
    request.Body = R"({"reason":"tuning","entries":[{"key":"Rate.XP.Quest","value":2},{"key":"Rate.XP.Kill","value":3}]})";
    panel.RecordRelayed(request, "gameserver", "POST", "/api/settings/batch", 200, "{}");
    request.Body = R"({"dry_run":true,"entries":[{"key":"Rate.XP.Quest","value":2}]})";
    panel.RecordRelayed(request, "gameserver", "POST", "/api/settings/batch", 200, R"({"dry_run":true})");
    request.Body = R"({"dry_run":"yes","reason":"x","entries":[{"key":"Rate.XP.Quest","value":2}]})";
    EXPECT_NO_THROW(panel.RecordRelayed(request, "gameserver", "POST", "/api/settings/batch", 422, R"({"error":"invalid","message":"Give dry_run as true or false"})"));
    request.Body = R"({"reason":"back to the file"})";
    panel.RecordRelayed(request, "gameserver", "DELETE", "/api/settings/World.UpdateInterval", 200, R"({"changed":true})");
    request.Body.clear();
    panel.RecordRelayed(request, "gameserver", "POST", "/api/reload/messages", 409, R"({"ok":false})");
    panel.RecordRelayed(request, "loginserver", "GET", "/api/settings?reveal=1", 200,
        R"({"revealed":true,"settings":[{"key":"Account.VerifierKeys","secret":true,"value":"1:aaaaaaaa"},{"key":"LoginDatabaseInfo","secret":true,"value":""},{"key":"Login.Name","secret":false,"value":"Ambrose"}]})");
    panel.RecordRelayed(request, "loginserver", "GET", "/api/settings", 200, R"({"revealed":false,"settings":[]})");
    panel.RecordRelayed(request, "loginserver", "GET", "/api/settings?reveal=Account.VerifierKeys", 200,
        R"({"revealed":true,"revealed_keys":["Account.VerifierKeys"],"settings":[{"key":"Account.VerifierKeys","secret":true,"value":"1:bbbbbbbb"},{"key":"LoginDatabaseInfo","secret":true,"value":"127.0.0.1;3306;ambrose;***;x"}]})");
    panel.RecordRelayed(request, "loginserver", "GET", "/api/settings/Login.Name/history", 200, "{}");

    EXPECT_EQ(PanelAudit::Count(panel.Store(), "settings:setting.changed"), 2) << "a refused change is recorded as well as one that landed";
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "settings:batch.changed"), 2) << "a dry run changes nothing, so it is not recorded, but a batch refused for an unclear dry_run is";
    std::optional<PanelStore::Statement> batches = panel.Store().Prepare("SELECT result, error FROM audit_event WHERE name = 'settings:batch.changed' AND result = ?1", error);
    ASSERT_TRUE(batches.has_value()) << error;
    batches->Bind(1, std::string(PanelAudit::ToString(AuditResult::Refused)));
    ASSERT_TRUE(batches->Step(error)) << error;
    EXPECT_EQ(batches->Text(1), "Give dry_run as true or false");
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "settings:setting.reset"), 1);
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "reload:target.run"), 1);
    EXPECT_EQ(PanelAudit::Count(panel.Store(), "settings:secret.revealed"), 2) << "only a read that showed a secret is a reveal";

    std::optional<PanelStore::Statement> rows = panel.Store().Prepare("SELECT result, reason, node, properties, error FROM audit_event WHERE name = 'settings:setting.changed' ORDER BY created_epoch_ms, event_id", error);
    ASSERT_TRUE(rows.has_value()) << error;
    std::vector<std::string> results;
    while (rows->Step(error))
    {
        results.push_back(rows->Text(0));
        EXPECT_EQ(rows->Text(2), "gameserver");
        EXPECT_EQ(rows->Text(3).find("100"), std::string::npos) << "no value is kept: " << rows->Text(3);
        EXPECT_NE(rows->Text(3).find("World.UpdateInterval"), std::string::npos) << rows->Text(3);
    }
    std::sort(results.begin(), results.end());
    EXPECT_EQ(results, (std::vector<std::string>{ std::string(PanelAudit::ToString(AuditResult::Refused)), std::string(PanelAudit::ToString(AuditResult::Succeeded)) }));

    std::optional<PanelStore::Statement> reveal = panel.Store().Prepare("SELECT properties FROM audit_event WHERE name = 'settings:secret.revealed'", error);
    ASSERT_TRUE(reveal.has_value()) << error;
    std::size_t reveals = 0;
    while (reveal->Step(error))
    {
        std::string const properties = reveal->Text(0);
        EXPECT_NE(properties.find("Account.VerifierKeys"), std::string::npos) << properties;
        EXPECT_EQ(properties.find("LoginDatabaseInfo"), std::string::npos) << "an empty secret and one not named showed nothing: " << properties;
        EXPECT_EQ(properties.find("aaaaaaaa"), std::string::npos) << properties;
        EXPECT_EQ(properties.find("bbbbbbbb"), std::string::npos) << properties;
        ++reveals;
    }
    EXPECT_EQ(reveals, 2u);
}
