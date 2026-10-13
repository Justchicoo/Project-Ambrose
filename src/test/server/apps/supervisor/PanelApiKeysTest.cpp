/*
 * Project Ambrose by Imjustchico
 * Tests the panel's personal API keys through a running panel: a minted key reads amb_ plus a twelve-character public id plus a thirty-two-byte secret in base32, its secret is shown once in the answer that made it and never again from any response or the store, which keeps only a hash; demoting a key's owner to viewer makes the key's restart request answer 403 at once; a key used from outside its CIDRs is refused with 403 and audited with the key id; an expired key answers 401 and a rotated key's old secret stops working when its grace ends; a key is refused on an app's own admin API and when it rides with a session cookie; at most the panel's limit of keys per user is made, counted in the same transaction as the insert; a key's subset can never exceed its maker's rights; last use is written at most once a minute; and no expiry is refused unless the owner's policy allows it.
 */

#include "AdminAuth.h"
#include "AdminClient.h"
#include "AdminRouter.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelApiKeys.h"
#include "PanelAudit.h"
#include "SourceFolder.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    struct Credentials
    {
        std::string Cookie;
        std::string Csrf;
        int64 UserId = 0;
    };

    class PanelApiKeysTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\nPanel.TrustedProxies =\n")).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
            _panel->Routes().AddGuarded("POST", "/api/panel/apps/game/restart", "power.restart",
                [](AdminRequest const&) { return AdminResponse::Json(200, R"({"restarted":true})"); });
        }

        void TearDown() override
        {
            if (_panel)
                _panel->Stop();
        }

        AdminClientResponse Send(std::string method, std::string path, nlohmann::json body = {}, Credentials const* credentials = nullptr)
        {
            AdminClient const client("127.0.0.1", _panel->GetPort(), "");
            AdminClientRequest request{ std::move(method), std::move(path), body.is_null() ? "" : body.dump(), "application/json", "" };
            if (credentials)
            {
                request.Headers.emplace_back("Cookie", credentials->Cookie);
                if (request.Method != "GET")
                {
                    request.Headers.emplace_back("Origin", "http://127.0.0.1:" + std::to_string(_panel->GetPort()));
                    request.Headers.emplace_back("X-CSRF-Token", credentials->Csrf);
                }
            }
            return client.Send(request, std::chrono::seconds(20));
        }

        AdminClientResponse SendKey(std::string method, std::string path, std::string const& key, nlohmann::json body = {},
            std::string const& cookie = "")
        {
            AdminClient const client("127.0.0.1", _panel->GetPort(), "");
            AdminClientRequest request{ std::move(method), std::move(path), body.is_null() ? "" : body.dump(), "application/json", "" };
            request.Headers.emplace_back("Authorization", "Bearer " + key);
            if (!cookie.empty())
                request.Headers.emplace_back("Cookie", cookie);
            return client.Send(request, std::chrono::seconds(20));
        }

        std::string CookieFrom(AdminClientResponse const& response) const
        {
            std::size_t const at = response.Head.find("Set-Cookie: ");
            if (at == std::string::npos)
                return {};
            std::string const rest = response.Head.substr(at + 12);
            return rest.substr(0, rest.find(';'));
        }

        bool MakeOwner(Credentials& credentials)
        {
            std::string token;
            for (std::string const& line : _harness.Store().Texts("Capture"))
            {
                std::size_t const at = line.find("token=");
                if (at != std::string::npos)
                    token = line.substr(at + 6);
            }
            if (token.empty())
                return false;
            AdminClientResponse const claimed = Send("POST", "/api/panel/claim",
                { { "token", token }, { "username", "owner" }, { "password", "a good long password" } });
            if (claimed.Status != 200)
                return false;
            nlohmann::json const answer = nlohmann::json::parse(claimed.Body, nullptr, false);
            credentials.Cookie = CookieFrom(claimed);
            credentials.Csrf = answer.value("csrf", "");
            credentials.UserId = answer.value("user", nlohmann::json::object()).value("id", int64{ 0 });
            return !credentials.Cookie.empty() && !credentials.Csrf.empty() && credentials.UserId != 0;
        }

        bool SignInAs(std::string const& username, std::string const& password, Credentials& credentials)
        {
            AdminClientResponse const signedIn = Send("POST", "/api/panel/session",
                { { "username", username }, { "password", password } });
            if (signedIn.Status != 200)
                return false;
            nlohmann::json const answer = nlohmann::json::parse(signedIn.Body, nullptr, false);
            credentials.Cookie = CookieFrom(signedIn);
            credentials.Csrf = answer.value("csrf", "");
            credentials.UserId = answer.value("user", nlohmann::json::object()).value("id", int64{ 0 });
            return !credentials.Cookie.empty() && !credentials.Csrf.empty();
        }

        bool MakeUser(std::string const& username, PanelRole role, Credentials& credentials, int64 byUserId)
        {
            std::string error;
            int64 id = 0;
            if (_panel->Users().Create(username, "a good long password", role == PanelRole::Owner, false, &id, error) != PanelUserResult::Ok)
                return false;
            if (role != PanelRole::Owner && _panel->Users().SetRole(id, role, byUserId, error) != PanelUserResult::Ok)
                return false;
            return SignInAs(username, "a good long password", credentials);
        }

        std::string MakeKey(Credentials const& maker, nlohmann::json draft, std::string& publicId)
        {
            AdminClientResponse const made = Send("POST", "/api/panel/apikeys", std::move(draft), &maker);
            EXPECT_EQ(made.Status, 201) << made.Body;
            nlohmann::json const answer = nlohmann::json::parse(made.Body, nullptr, false);
            publicId = answer.value("public_id", "");
            EXPECT_FALSE(publicId.empty());
            return answer.value("key", "");
        }

        nlohmann::json ManageDraft()
        {
            return { { "name", "manager" }, { "permissions", { "apikeys.manage" } }, { "scopes", { "panel" } } };
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
    };
}

TEST_F(PanelApiKeysTest, KeyTextHasTheMintedShape)
{
    EXPECT_TRUE(PanelApiKeys::IsKeyText("amb_ABCDEFGHIJKL." + std::string(52, 'A')));
    EXPECT_FALSE(PanelApiKeys::IsKeyText("bearer-token"));
    EXPECT_FALSE(PanelApiKeys::IsKeyText("amb_ABCDEFGHIJK." + std::string(52, 'A')));
    EXPECT_FALSE(PanelApiKeys::IsKeyText("amb_ABCDEFGHIJKL." + std::string(51, 'A')));
    EXPECT_FALSE(PanelApiKeys::IsKeyText("amb_abcdefghijkl." + std::string(52, 'A')));
    EXPECT_EQ(PanelApiKeys::PublicIdOf("amb_ABCDEFGHIJKL." + std::string(52, 'A')), "ABCDEFGHIJKL");
    EXPECT_TRUE(PanelApiKeys::PublicIdOf("nope").empty());
    EXPECT_TRUE(PanelApiKeys::IsKeyPrincipal("apikey:ABCDEFGHIJKL"));
    EXPECT_FALSE(PanelApiKeys::IsKeyPrincipal("user:1"));
    EXPECT_EQ(PanelApiKeys::PrincipalOf("ABCDEFGHIJKL"), "apikey:ABCDEFGHIJKL");
    EXPECT_EQ(PanelApiKeys::PublicIdOfPrincipal("apikey:ABCDEFGHIJKL"), "ABCDEFGHIJKL");
}

TEST_F(PanelApiKeysTest, SecretIsShownOnceAndNeverAgain)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);
    ASSERT_TRUE(PanelApiKeys::IsKeyText(secret)) << secret;

    AdminClientResponse const listed = Send("GET", "/api/panel/apikeys", {}, &owner);
    ASSERT_EQ(listed.Status, 200) << listed.Body;
    EXPECT_EQ(listed.Body.find(secret), std::string::npos) << "the list answer must not carry the secret";
    nlohmann::json const keys = nlohmann::json::parse(listed.Body, nullptr, false);
    ASSERT_TRUE(keys.is_array() && !keys.empty());
    EXPECT_FALSE(keys[0].contains("key"));

    std::string error;
    std::optional<PanelStore::Statement> rows = _panel->Store().Prepare("SELECT secret_hash FROM panel_api_key", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    std::string const hash = rows->Text(0);
    rows.reset();
    EXPECT_EQ(hash.find(secret), std::string::npos);
    EXPECT_NE(hash, secret);
    EXPECT_EQ(hash.size(), 64u) << "the store keeps a SHA-256 hex, not the secret";
}

TEST_F(PanelApiKeysTest, DemotingTheOwnerShrinksTheKeyAtOnce)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    Credentials second;
    ASSERT_TRUE(MakeUser("second", PanelRole::Owner, second, owner.UserId));

    std::string publicId;
    std::string const secret = MakeKey(owner,
        { { "name", "restarter" }, { "permissions", { "power.restart" } }, { "scopes", { "panel", "game" } } }, publicId);

    AdminClientResponse const before = SendKey("POST", "/api/panel/apps/game/restart", secret);
    EXPECT_EQ(before.Status, 200) << before.Body;

    std::string error;
    ASSERT_EQ(_panel->Users().SetRole(owner.UserId, PanelRole::Viewer, second.UserId, error), PanelUserResult::Ok) << error;

    AdminClientResponse const after = SendKey("POST", "/api/panel/apps/game/restart", secret);
    EXPECT_EQ(after.Status, 403) << after.Body;
}

TEST_F(PanelApiKeysTest, CidrRefusalIsAuditedWithTheKeyId)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner,
        { { "name", "office" }, { "permissions", { "status.read" } }, { "scopes", { "panel" } }, { "cidrs", { "10.0.0.0/8" } } }, publicId);

    AdminClientResponse const refused = SendKey("GET", "/api/panel/apikeys", secret);
    EXPECT_EQ(refused.Status, 403) << refused.Body;
    EXPECT_NE(refused.Body.find("key_cidr_refused"), std::string::npos) << refused.Body;

    std::string error;
    std::optional<PanelStore::Statement> events = _panel->Store().Prepare(
        "SELECT id FROM audit_event WHERE name = 'panel:apikey.cidr_refused'", error);
    ASSERT_TRUE(events.has_value()) << error;
    ASSERT_TRUE(events->Step(error)) << error << "the refusal must be audited";
    int64 const eventId = events->Int64(0);
    events.reset();
    std::optional<PanelStore::Statement> subjects = _panel->Store().Prepare(
        "SELECT kind, subject_id FROM audit_subject WHERE event = ?", error);
    ASSERT_TRUE(subjects.has_value()) << error;
    subjects->Bind(1, eventId);
    bool named = false;
    while (subjects->Step(error))
        if (subjects->Text(0) == "api_key" && subjects->Text(1) == publicId)
            named = true;
    EXPECT_TRUE(named) << "the audit row must name the key id";
}

TEST_F(PanelApiKeysTest, ExpiredKeyAnswers401)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);

    std::string error;
    ASSERT_TRUE(_panel->Store().Execute(
        "UPDATE panel_api_key SET expires_epoch_ms = 1 WHERE public_id = '" + publicId + "'", error)) << error;

    AdminClientResponse const answered = SendKey("GET", "/api/panel/apikeys", secret);
    EXPECT_EQ(answered.Status, 401) << answered.Body;
    EXPECT_NE(answered.Body.find("key_expired"), std::string::npos) << answered.Body;
}

TEST_F(PanelApiKeysTest, RotatedOldSecretStopsWhenItsGraceEnds)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const oldSecret = MakeKey(owner, ManageDraft(), publicId);

    AdminClientResponse const rotated = Send("POST", "/api/panel/apikeys/" + publicId + "/rotate", { { "grace_seconds", 3600 } }, &owner);
    ASSERT_EQ(rotated.Status, 201) << rotated.Body;
    nlohmann::json const answer = nlohmann::json::parse(rotated.Body, nullptr, false);
    std::string const newSecret = answer.value("key", "");
    ASSERT_TRUE(PanelApiKeys::IsKeyText(newSecret));
    EXPECT_NE(newSecret, oldSecret);

    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", oldSecret).Status, 200) << "the old secret works during its grace";
    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", newSecret).Status, 200);

    std::string error;
    ASSERT_TRUE(_panel->Store().Execute(
        "UPDATE panel_api_key SET grace_expires_epoch_ms = 1 WHERE public_id = '" + publicId + "'", error)) << error;

    AdminClientResponse const ended = SendKey("GET", "/api/panel/apikeys", oldSecret);
    EXPECT_EQ(ended.Status, 401) << ended.Body;
    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", newSecret).Status, 200) << "the replacement keeps working";
}

TEST_F(PanelApiKeysTest, RevokedKeyIsRefusedOnTheNextRequest)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);
    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", secret).Status, 200);

    AdminClientResponse const revoked = Send("DELETE", "/api/panel/apikeys/" + publicId, {}, &owner);
    EXPECT_EQ(revoked.Status, 200) << revoked.Body;

    AdminClientResponse const refused = SendKey("GET", "/api/panel/apikeys", secret);
    EXPECT_EQ(refused.Status, 401) << refused.Body;
    EXPECT_NE(refused.Body.find("key_revoked"), std::string::npos) << refused.Body;
}

TEST_F(PanelApiKeysTest, KeyIsRefusedOnAnAppAdminApi)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);

    AdminAuth auth(10, 1.0);
    auth.SetToken("the-app-token");
    AdminRouter app(auth);
    app.AddGuarded("GET", "/api/status", "status.read", [](AdminRequest const&) { return AdminResponse::Json(200, "{}"); });

    AdminRequest request;
    request.Method = "GET";
    request.Path = "/api/status";
    request.RemoteAddress = "127.0.0.1";
    request.Host = "127.0.0.1";
    request.Authorization = "Bearer " + secret;
    AdminResponse const answered = app.Dispatch(request);
    EXPECT_EQ(answered.Status, 401) << answered.Body;
}

TEST_F(PanelApiKeysTest, KeyWithASessionCookieIsRefused)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);

    AdminClientResponse const refused = SendKey("GET", "/api/panel/apikeys", secret, {}, owner.Cookie);
    EXPECT_EQ(refused.Status, 401) << refused.Body;
    EXPECT_NE(refused.Body.find("key_with_session"), std::string::npos) << refused.Body;

    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", secret).Status, 200) << "the key alone is accepted";
}

TEST_F(PanelApiKeysTest, KeyLimitIsCountedInATransaction)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    AdminClientResponse const limited = Send("PATCH", "/api/panel/settings?group=limits",
        { { "values", { { "Limits.ApiKeysPerUser", "2" } } } }, &owner);
    ASSERT_EQ(limited.Status, 200) << limited.Body;

    std::string first;
    std::string second;
    ASSERT_TRUE(PanelApiKeys::IsKeyText(MakeKey(owner, { { "name", "one" } }, first)));
    ASSERT_TRUE(PanelApiKeys::IsKeyText(MakeKey(owner, { { "name", "two" } }, second)));
    AdminClientResponse const third = Send("POST", "/api/panel/apikeys", { { "name", "three" } }, &owner);
    EXPECT_EQ(third.Status, 409) << third.Body;
    EXPECT_NE(third.Body.find("too_many_keys"), std::string::npos) << third.Body;

    AdminClientResponse const listed = Send("GET", "/api/panel/apikeys", {}, &owner);
    ASSERT_EQ(listed.Status, 200);
    EXPECT_EQ(nlohmann::json::parse(listed.Body, nullptr, false).size(), 2u);
}

TEST_F(PanelApiKeysTest, SubsetCannotExceedTheMaker)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    Credentials viewer;
    ASSERT_TRUE(MakeUser("viewer", PanelRole::Viewer, viewer, owner.UserId));

    AdminClientResponse const over = Send("POST", "/api/panel/apikeys",
        { { "name", "too much" }, { "permissions", { "users.delete" } } }, &viewer);
    EXPECT_EQ(over.Status, 422) << over.Body;

    AdminClientResponse const fine = Send("POST", "/api/panel/apikeys",
        { { "name", "fits" }, { "permissions", { "status.read" } } }, &viewer);
    EXPECT_EQ(fine.Status, 201) << fine.Body;
}

TEST_F(PanelApiKeysTest, NoExpiryNeedsTheOwnerPolicy)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));

    AdminClientResponse const refused = Send("POST", "/api/panel/apikeys",
        { { "name", "forever" }, { "no_expiry", true } }, &owner);
    EXPECT_EQ(refused.Status, 422) << refused.Body;

    AdminClientResponse const allowed = Send("PATCH", "/api/panel/settings?group=limits",
        { { "values", { { "Limits.ApiKeyAllowNoExpiry", "1" } } } }, &owner);
    ASSERT_EQ(allowed.Status, 200) << allowed.Body;

    AdminClientResponse const made = Send("POST", "/api/panel/apikeys",
        { { "name", "forever" }, { "no_expiry", true } }, &owner);
    ASSERT_EQ(made.Status, 201) << made.Body;
    nlohmann::json const answer = nlohmann::json::parse(made.Body, nullptr, false);
    EXPECT_TRUE(answer["expires_epoch_ms"].is_null()) << made.Body;
}

TEST_F(PanelApiKeysTest, LastUsedIsWrittenAtMostOnceAMinute)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);

    auto lastUsed = [&]() -> std::optional<int64>
    {
        std::string error;
        std::optional<PanelStore::Statement> rows = _panel->Store().Prepare(
            "SELECT last_used_epoch_ms, last_used_address FROM panel_api_key WHERE public_id = ?", error);
        if (!rows)
            return std::nullopt;
        rows->Bind(1, publicId);
        if (!rows->Step(error) || rows->IsNull(0))
            return std::nullopt;
        return rows->Int64(0);
    };

    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", secret).Status, 200);
    std::optional<int64> const first = lastUsed();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", secret).Status, 200);
    EXPECT_EQ(lastUsed(), first) << "a second use inside the minute writes nothing";

    std::string error;
    ASSERT_TRUE(_panel->Store().Execute(
        "UPDATE panel_api_key SET last_used_epoch_ms = 1 WHERE public_id = '" + publicId + "'", error)) << error;
    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", secret).Status, 200);
    std::optional<int64> const later = lastUsed();
    ASSERT_TRUE(later.has_value());
    EXPECT_GT(*later, 1) << "an old last use is refreshed";
}

TEST_F(PanelApiKeysTest, WrongSecretAnswers401WithoutSayingWhichHalfFailed)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);

    std::string tampered = secret;
    tampered.back() = tampered.back() == 'A' ? 'B' : 'A';
    AdminClientResponse const wrong = SendKey("GET", "/api/panel/apikeys", tampered);
    EXPECT_EQ(wrong.Status, 401) << wrong.Body;

    std::string unknown = secret;
    unknown.replace(4, 12, "ZZZZZZZZZZZZ");
    AdminClientResponse const missing = SendKey("GET", "/api/panel/apikeys", unknown);
    EXPECT_EQ(missing.Status, 401) << missing.Body;
    EXPECT_EQ(nlohmann::json::parse(wrong.Body, nullptr, false)["error"],
        nlohmann::json::parse(missing.Body, nullptr, false)["error"]) << "unknown id and wrong secret answer alike";
}

TEST_F(PanelApiKeysTest, ManagingOtherKeysNeedsApikeysManage)
{
    Credentials owner;
    ASSERT_TRUE(MakeOwner(owner));
    Credentials viewer;
    ASSERT_TRUE(MakeUser("viewer", PanelRole::Viewer, viewer, owner.UserId));
    std::string publicId;
    std::string const secret = MakeKey(owner, ManageDraft(), publicId);

    EXPECT_EQ(Send("GET", "/api/panel/admin/apikeys", {}, &viewer).Status, 403);
    AdminClientResponse const listed = Send("GET", "/api/panel/admin/apikeys", {}, &owner);
    ASSERT_EQ(listed.Status, 200) << listed.Body;
    EXPECT_EQ(nlohmann::json::parse(listed.Body, nullptr, false).size(), 1u);

    EXPECT_EQ(Send("DELETE", "/api/panel/admin/apikeys/" + publicId, {}, &viewer).Status, 403);
    AdminClientResponse const revoked = Send("DELETE", "/api/panel/admin/apikeys/" + publicId, {}, &owner);
    EXPECT_EQ(revoked.Status, 200) << revoked.Body;
    EXPECT_EQ(SendKey("GET", "/api/panel/apikeys", secret).Status, 401) << "the revoked key is refused at once";
}
