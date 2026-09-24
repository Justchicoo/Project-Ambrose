/*
 * Project Ambrose by Imjustchico
 * Tests the panel's own listener and what it holds: it serves nothing until Panel.Enable is set, it opens its store with the panel tables before it listens, it answers its own routes on a loopback port with its own token, a bind beyond this machine with no certificate is refused with the Panel option names in the message, the plain-HTTP opt-in lifts that refusal, a certificate and key are served over TLS with the fingerprint the files hold, a route that declares a cost is held back with a retry hint while an uncosted route from the same caller still answers, one audit row records the throttling however many requests are refused in that minute, and a change whose audit row cannot be written is not applied, the plain-HTTP opt-in lets it reach beyond this machine with the risk said out loud, a reload that would leave the bind unsafe is refused while the old listener goes on serving, and a replaced certificate is served after a reload on the same port, and it refuses to start at all when a route says neither which permission it needs nor that any signed-in member may call it, or names a permission the catalog does not hold.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelErrors.h"
#include "PanelAudit.h"
#include "TlsCertificate.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
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
    std::optional<PanelStore::Statement> tables = panel.Store().Prepare("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name IN ('audit_event', 'audit_subject', 'panel_session')", error);
    ASSERT_TRUE(tables.has_value()) << error;
    ASSERT_TRUE(tables->Step(error)) << error;
    EXPECT_EQ(tables->Int64(0), 3);
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
