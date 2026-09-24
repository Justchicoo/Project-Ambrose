/*
 * Project Ambrose by Imjustchico
 * Tests how an operator gets into the panel and how a guesser is kept out: a fresh panel has no password at all and prints a one-time link that makes the owner once, from this machine only; a name and password then open a session that the panel answers by cookie; twenty failed attempts against one account are held back until the window passes; a success against one account does not forgive the failures counted against another from the same address; and changing a password ends the sessions that user already had.
 */

#include "AdminClient.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    class PanelSignInTest : public testing::Test
    {
    protected:
        void Start(std::string const& extra = "")
        {
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            std::string const text = "Panel.Enable = 1\nPanel.Port = 0\n" + extra;
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", text)).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
        }

        AdminClientResponse Post(std::string const& path, nlohmann::json const& body, std::vector<std::pair<std::string, std::string>> headers = {})
        {
            AdminClient const client("127.0.0.1", _panel->GetPort(), "");
            AdminClientRequest request{ "POST", path, body.dump(), "application/json", "" };
            request.Headers = std::move(headers);
            return client.Send(request, std::chrono::seconds(20));
        }

        AdminClientResponse Get(std::string const& path, std::string const& cookie)
        {
            AdminClient const client("127.0.0.1", _panel->GetPort(), "");
            AdminClientRequest request{ "GET", path, "", "application/json", "" };
            if (!cookie.empty())
                request.Headers.push_back({ "Cookie", cookie });
            return client.Send(request, std::chrono::seconds(20));
        }

        std::string TokenFromTheLog()
        {
            for (std::string const& line : _harness.Store().Texts("Capture"))
            {
                std::size_t const at = line.find("token=");
                if (at != std::string::npos)
                    return line.substr(at + 6);
            }
            return {};
        }

        std::string CookieFrom(AdminClientResponse const& answer) const
        {
            std::size_t const at = answer.Head.find("Set-Cookie: ");
            if (at == std::string::npos)
                return {};
            std::string const rest = answer.Head.substr(at + 12);
            return rest.substr(0, rest.find(';'));
        }

        std::string MakeOwner(std::string const& username, std::string const& password)
        {
            std::string const token = TokenFromTheLog();
            EXPECT_FALSE(token.empty());
            AdminClientResponse const claimed = Post("/api/panel/claim", { { "token", token }, { "username", username }, { "password", password } });
            EXPECT_EQ(claimed.Status, 200) << claimed.Body;
            return CookieFrom(claimed);
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
    };
}

TEST_F(PanelSignInTest, AFreshPanelHasNoPasswordAndItsLinkMakesTheOwnerOnce)
{
    _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    Start();

    std::string const token = TokenFromTheLog();
    ASSERT_FALSE(token.empty()) << "the supervisor printed no owner link";

    AdminClientResponse const refused = Post("/api/panel/claim", { { "token", "not the token" }, { "username", "owner" }, { "password", "a good long password" } });
    EXPECT_EQ(refused.Status, 403) << refused.Body;

    AdminClientResponse const weak = Post("/api/panel/claim", { { "token", token }, { "username", "owner" }, { "password", "short" } });
    EXPECT_EQ(weak.Status, 422) << weak.Body;

    AdminClientResponse const claimed = Post("/api/panel/claim", { { "token", token }, { "username", "owner" }, { "password", "a good long password" } });
    ASSERT_EQ(claimed.Status, 200) << claimed.Body;
    nlohmann::json const body = nlohmann::json::parse(claimed.Body);
    EXPECT_EQ(body["user"]["username"], "owner");
    EXPECT_TRUE(body["user"]["owner"]);
    EXPECT_FALSE(body["csrf"].get<std::string>().empty());
    EXPECT_NE(claimed.Head.find("Set-Cookie: ambrose_panel_"), std::string::npos) << claimed.Head;

    AdminClientResponse const again = Post("/api/panel/claim", { { "token", token }, { "username", "second" }, { "password", "a good long password" } });
    EXPECT_EQ(again.Status, 410) << again.Body;

    std::string error;
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:session.opened"), 1);
}

TEST_F(PanelSignInTest, ANameAndPasswordOpenASessionTheCookieCarries)
{
    _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    Start();
    MakeOwner("owner", "a good long password");

    AdminClientResponse const wrong = Post("/api/panel/session", { { "username", "owner" }, { "password", "not the password" } });
    EXPECT_EQ(wrong.Status, 401) << wrong.Body;
    AdminClientResponse const nobody = Post("/api/panel/session", { { "username", "nobody" }, { "password", "a good long password" } });
    EXPECT_EQ(nobody.Status, 401) << nobody.Body;
    EXPECT_EQ(nlohmann::json::parse(wrong.Body)["detail"], nlohmann::json::parse(nobody.Body)["detail"]);

    AdminClientResponse const signedIn = Post("/api/panel/session", { { "username", "OWNER" }, { "password", "a good long password" } });
    ASSERT_EQ(signedIn.Status, 200) << signedIn.Body;
    std::string const cookie = CookieFrom(signedIn);
    ASSERT_FALSE(cookie.empty());

    EXPECT_EQ(Get("/api/panel/me", "").Status, 401);
    AdminClientResponse const me = Get("/api/panel/me", cookie);
    ASSERT_EQ(me.Status, 200) << me.Body;
    EXPECT_EQ(nlohmann::json::parse(me.Body)["username"], "owner");

    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:session.refused"), 2);
}

TEST_F(PanelSignInTest, HoldsBackAGuesserUntilTheWindowPasses)
{
    _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    Start();
    MakeOwner("owner", "a good long password");
    _panel->SignInThrottle().SetLimits(20, std::chrono::seconds(60));

    for (int attempt = 0; attempt < 20; ++attempt)
        EXPECT_EQ(Post("/api/panel/session", { { "username", "owner" }, { "password", "not the password" } }).Status, 401) << attempt;

    AdminClientResponse const held = Post("/api/panel/session", { { "username", "owner" }, { "password", "a good long password" } });
    EXPECT_EQ(held.Status, 429) << held.Body;
    EXPECT_NE(held.Head.find("Retry-After:"), std::string::npos) << held.Head;
    EXPECT_EQ(PanelAudit::Count(_panel->Store(), "panel:session.throttled"), 1);

    _panel->SignInThrottle().Clear();
    EXPECT_EQ(Post("/api/panel/session", { { "username", "owner" }, { "password", "a good long password" } }).Status, 200);
}

TEST_F(PanelSignInTest, ASuccessForOneAccountDoesNotForgiveGuessesAtAnother)
{
    PanelSignInThrottle throttle;
    throttle.SetLimits(3, std::chrono::seconds(60));

    for (int attempt = 0; attempt < 3; ++attempt)
        throttle.Failed("alice", "10.0.0.1");
    for (int attempt = 0; attempt < 3; ++attempt)
        throttle.Failed("bob", "10.0.0.1");

    EXPECT_FALSE(throttle.Check("alice", "10.0.0.1").Allowed);
    EXPECT_FALSE(throttle.Check("bob", "10.0.0.1").Allowed);

    throttle.Succeeded("bob", "10.0.0.1");
    EXPECT_TRUE(throttle.Check("bob", "10.0.0.1").Allowed);
    EXPECT_FALSE(throttle.Check("alice", "10.0.0.1").Allowed) << "signing in to bob forgave the guesses at alice";

    EXPECT_FALSE(throttle.Check("alice", "10.0.0.2").Allowed) << "alice is held back by her account's own count, whatever the address";
    EXPECT_TRUE(throttle.Check("carol", "10.0.0.1").Allowed);
}

TEST_F(PanelSignInTest, AFoldedNameIsTheSameGuessingAndTheWindowEndsOnItsOwn)
{
    auto now = PanelSignInThrottle::Clock::time_point() + std::chrono::hours(1);
    PanelSignInThrottle throttle([&now] { return now; });
    throttle.SetLimits(3, std::chrono::seconds(60));

    throttle.Failed("Alice", "10.0.0.1");
    throttle.Failed("ALICE", "10.0.0.1");
    throttle.Failed("alice", "10.0.0.1");
    PanelSignInVerdict const held = throttle.Check("aLiCe", "10.0.0.1");
    EXPECT_FALSE(held.Allowed);
    EXPECT_EQ(held.RetryAfterSeconds, 60u);
    EXPECT_TRUE(held.FirstThisWindow);
    EXPECT_FALSE(throttle.Check("alice", "10.0.0.1").FirstThisWindow);

    now += std::chrono::seconds(30);
    EXPECT_EQ(throttle.Check("alice", "10.0.0.1").RetryAfterSeconds, 30u);
    now += std::chrono::seconds(31);
    EXPECT_TRUE(throttle.Check("alice", "10.0.0.1").Allowed);
}

TEST_F(PanelSignInTest, APasswordChangeEndsTheSessionsThatUserHad)
{
    _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    Start();
    std::string const cookie = MakeOwner("owner", "a good long password");
    ASSERT_FALSE(cookie.empty());
    ASSERT_EQ(Get("/api/panel/me", cookie).Status, 200);

    std::string error;
    std::optional<PanelUser> const owner = _panel->Users().Find("owner", error);
    ASSERT_TRUE(owner.has_value()) << error;
    ASSERT_EQ(_panel->Users().SetPassword(owner->Id, "another long password", false, error), PanelUserResult::Ok) << error;
    auto const changed = std::chrono::steady_clock::now();

    AdminClientResponse const after = Get("/api/panel/me", cookie);
    EXPECT_EQ(after.Status, 401) << after.Body;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - changed).count(), 1000);

    AdminClientResponse const fresh = Post("/api/panel/session", { { "username", "owner" }, { "password", "another long password" } });
    EXPECT_EQ(fresh.Status, 200) << fresh.Body;
}
