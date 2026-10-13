/*
 * Project Ambrose by Imjustchico
 * Tests the one-time invite links against a real panel listener: a link is claimed once and the second claim is refused, an expired or revoked link is refused, an invite naming an operator that already exists adds the grant at once and records both users, claiming with a taken name answers the same generic refusal as any other unusable name so nothing says whether a name exists, invites are held back per inviter and per scope, and two invites for the same operator and scope made at once leave a single grant set behind.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelInvites.h"
#include "PanelStore.h"
#include "PanelTestSession.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace PanelTest;

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class PanelInvitesTest : public testing::Test
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

        void Start()
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            _panel->Invites().SetClock([this] { return _now; });
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
            ASSERT_TRUE(ClaimOwner(*_panel, _harness, _owner)) << "the owner claim link makes the owner";
        }

        AdminClientResponse CreateInvite(nlohmann::json const& body)
        {
            return Send(*_panel, "POST", "/api/panel/invites", body, &_owner);
        }

        AdminClientResponse Claim(std::string const& token, std::string const& username, std::string const& password = std::string(Password))
        {
            return Send(*_panel, "POST", "/api/panel/invites/claim", { { "token", token }, { "username", username }, { "password", password } });
        }

        std::string MakeUser(std::string const& username)
        {
            AdminClientResponse const made = CreateInvite({ { "scope", "gameserver" } });
            EXPECT_EQ(made.Status, 201) << made.Body;
            std::string const token = Json(made).value("token", std::string());
            AdminClientResponse const claimed = Claim(token, username);
            EXPECT_EQ(claimed.Status, 200) << claimed.Body;
            return token;
        }

        nlohmann::json AuditProperties(std::string const& name)
        {
            std::string error;
            std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT properties FROM audit_event WHERE name = ? ORDER BY id DESC LIMIT 1", error);
            EXPECT_TRUE(rows) << error;
            rows->Bind(1, name);
            EXPECT_TRUE(rows->Step(error)) << error;
            return nlohmann::json::parse(rows->Text(0), nullptr, false);
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
        Browser _owner;
        int64 _now = 1700000000000;
    };
}

TEST_F(PanelInvitesTest, AnInviteLinkWorksOnce)
{
    Start();
    AdminClientResponse const made = CreateInvite({ { "scope", "gameserver" }, { "grants", { "power.restart" } } });
    ASSERT_EQ(made.Status, 201) << made.Body;
    std::string const token = Json(made).value("token", std::string());
    ASSERT_FALSE(token.empty());

    AdminClientResponse const peeked = Send(*_panel, "POST", "/api/panel/invites/peek", { { "token", token } });
    EXPECT_EQ(peeked.Status, 200) << peeked.Body;
    EXPECT_EQ(Json(peeked)["scope"], "gameserver");

    AdminClientResponse const claimed = Claim(token, "newop");
    EXPECT_EQ(claimed.Status, 200) << claimed.Body;

    AdminClientResponse const again = Claim(token, "newop2");
    EXPECT_EQ(again.Status, 410) << again.Body;
    EXPECT_EQ(Json(again).value("error", std::string()), "invite_used");

    AdminClientResponse const peekedAgain = Send(*_panel, "POST", "/api/panel/invites/peek", { { "token", token } });
    EXPECT_EQ(peekedAgain.Status, 404) << peekedAgain.Body;
}

TEST_F(PanelInvitesTest, AnInvitePastItsExpiryIsRefused)
{
    Start();
    AdminClientResponse const made = CreateInvite({ { "scope", "gameserver" }, { "ttl_hours", 1 } });
    ASSERT_EQ(made.Status, 201) << made.Body;
    std::string const token = Json(made).value("token", std::string());

    _now += 2 * 3600 * 1000;
    AdminClientResponse const claimed = Claim(token, "lateop");
    EXPECT_EQ(claimed.Status, 410) << claimed.Body;
    EXPECT_EQ(Json(claimed).value("error", std::string()), "invite_expired");

    AdminClientResponse const listed = Send(*_panel, "GET", "/api/panel/invites", nullptr, &_owner);
    ASSERT_EQ(listed.Status, 200) << listed.Body;
    EXPECT_EQ(Json(listed)["invites"][0].value("state", std::string()), "expired");
}

TEST_F(PanelInvitesTest, ARevokedInviteIsRefused)
{
    Start();
    AdminClientResponse const made = CreateInvite({ { "scope", "gameserver" } });
    ASSERT_EQ(made.Status, 201) << made.Body;
    int64 const id = Json(made).value("id", int64{ 0 });
    std::string const token = Json(made).value("token", std::string());

    AdminClientResponse const revoked = Send(*_panel, "DELETE", fmt::format("/api/panel/invites/{}", id), nullptr, &_owner);
    EXPECT_EQ(revoked.Status, 200) << revoked.Body;

    AdminClientResponse const claimed = Claim(token, "revokedop");
    EXPECT_EQ(claimed.Status, 410) << claimed.Body;

    AdminClientResponse const revokedAgain = Send(*_panel, "DELETE", fmt::format("/api/panel/invites/{}", id), nullptr, &_owner);
    EXPECT_EQ(revokedAgain.Status, 404) << revokedAgain.Body;
}

TEST_F(PanelInvitesTest, InvitingAnExistingOperatorAddsTheGrantAtOnce)
{
    Start();
    MakeUser("existing");

    AdminClientResponse const invited = CreateInvite({ { "scope", "gameserver" }, { "grants", { "power.restart" } }, { "target_username", "existing" } });
    ASSERT_EQ(invited.Status, 200) << invited.Body;
    EXPECT_TRUE(Json(invited).value("applied", false)) << invited.Body;

    AdminClientResponse const read = Send(*_panel, "GET", "/api/panel/grant-sets?user_id=2&scope=gameserver", nullptr, &_owner);
    ASSERT_EQ(read.Status, 200) << read.Body;
    EXPECT_EQ(Json(read)["permissions"], nlohmann::json::array({ "power.restart" })) << read.Body;

    nlohmann::json const properties = AuditProperties("users:invite.accepted");
    EXPECT_EQ(properties["scope"], "gameserver");
    EXPECT_EQ(properties["old"], nlohmann::json::array());
    EXPECT_EQ(properties["new"], nlohmann::json::array({ "power.restart" }));
}

TEST_F(PanelInvitesTest, ClaimingSaysNothingAboutWhetherANameExists)
{
    Start();
    AdminClientResponse const made = CreateInvite({ { "scope", "gameserver" } });
    ASSERT_EQ(made.Status, 201) << made.Body;
    std::string const token = Json(made).value("token", std::string());

    AdminClientResponse const taken = Claim(token, "owner");
    EXPECT_EQ(taken.Status, 422) << taken.Body;
    EXPECT_EQ(taken.Body.find("taken"), std::string::npos) << taken.Body;

    AdminClientResponse const unknown = Claim("no-such-token", "somebody");
    EXPECT_EQ(unknown.Status, 404) << unknown.Body;
    EXPECT_EQ(unknown.Body.find("owner"), std::string::npos) << unknown.Body;
}

TEST_F(PanelInvitesTest, InvitesAreHeldBackPerInviter)
{
    Start();
    for (uint32 index = 0; index < PanelInvites::MaxPerInviterPerHour; ++index)
    {
        AdminClientResponse const made = CreateInvite({ { "scope", "gameserver" } });
        ASSERT_EQ(made.Status, 201) << made.Body;
    }
    AdminClientResponse const held = CreateInvite({ { "scope", "gameserver" } });
    EXPECT_EQ(held.Status, 429) << held.Body;
    EXPECT_EQ(Json(held).value("error", std::string()), "invite_rate_limited");
}

TEST_F(PanelInvitesTest, TwoInvitesForTheSameUserAndScopeMakeOneGrant)
{
    Start();
    MakeUser("dual");

    std::vector<int> statuses;
    std::mutex mutex;
    auto invite = [&]
    {
        AdminClientResponse const answer = CreateInvite({ { "scope", "gameserver" }, { "grants", { "power.restart" } }, { "target_username", "dual" } });
        std::lock_guard const lock(mutex);
        statuses.push_back(answer.Status);
    };
    std::thread first(invite);
    std::thread second(invite);
    first.join();
    second.join();
    ASSERT_EQ(statuses.size(), 2u);
    EXPECT_EQ(statuses[0], 200) << statuses[0];
    EXPECT_EQ(statuses[1], 200) << statuses[1];

    std::string error;
    std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT COUNT(*), permissions FROM panel_grant_set WHERE user_id = 2 AND scope = 'gameserver'", error);
    ASSERT_TRUE(rows) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Int64(0), 1) << "two invites for one user and scope leave one grant set";
    EXPECT_EQ(rows->Text(1), "[\"power.restart\"]") << rows->Text(1);
}
