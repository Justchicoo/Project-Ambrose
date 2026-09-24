/*
 * Project Ambrose by Imjustchico
 * Tests the order a request is decided in, which matters more than any single rule: a role answers first, a grant only ever adds and never takes away, somebody holding nothing on an app is told there is no such app rather than that they may not touch it, so a refusal is never a list of what exists, and a permission the catalog does not hold is refused rather than quietly allowed. Also that the app a request is about is read from its path, since everything above depends on knowing which app is being asked for, and that a sub-user holding one grant on one app reaches that one thing there, is refused the rest of that app, and is told no other app exists.
 */

#include "LogTestDirectory.h"
#include "PanelAuthorization.h"
#include "PanelGrants.h"
#include "PanelPermissions.h"
#include "PanelStore.h"
#include "PanelUsers.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    PanelAsking As(PanelRole role)
    {
        PanelAsking asking;
        asking.UserId = 7;
        asking.Role = role;
        asking.IsOwner = role == PanelRole::Owner;
        return asking;
    }
}

TEST(PanelAuthorizationTest, TheAppBeingAskedAboutIsReadFromThePath)
{
    EXPECT_EQ(PanelAuthorization::AppInPath("/api/apps/gameserver/api/command"), "gameserver");
    EXPECT_EQ(PanelAuthorization::AppInPath("/api/apps/loginserver"), "loginserver");
    EXPECT_EQ(PanelAuthorization::AppInPath("/api/apps/"), "");
    EXPECT_EQ(PanelAuthorization::AppInPath("/api/status"), "") << "a request about no app in particular names none";
    EXPECT_EQ(PanelAuthorization::AppInPath("/api/panel/users"), "");
}

TEST(PanelAuthorizationTest, ARoleAnswersFirst)
{
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Owner), "accounts.delete", false, false, true), PermissionVerdict::Allowed);
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Operator), "console.write", false, false, true), PermissionVerdict::Allowed);
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Viewer), "console.read", false, false, true), PermissionVerdict::Allowed);
}

TEST(PanelAuthorizationTest, AGrantAddsAndNeverTakesAway)
{
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Viewer), "power.restart", false, true, true), PermissionVerdict::Forbidden)
        << "holding something else on the app is not holding this";
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Viewer), "power.restart", true, true, true), PermissionVerdict::Allowed)
        << "the grant is what allows it";

    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Operator), "console.write", false, false, true), PermissionVerdict::Allowed)
        << "a role that already allows it does not need a grant, and the absence of one takes nothing away";
}

TEST(PanelAuthorizationTest, HoldingNothingOnAnAppMeansThereIsNoSuchApp)
{
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Viewer), "power.restart", false, false, true), PermissionVerdict::OutOfScope)
        << "a refusal must not double as a list of the apps that exist";
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Viewer), "power.restart", false, true, true), PermissionVerdict::Forbidden)
        << "somebody who holds something here already knows the app is there";
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Viewer), "panel.settings", false, false, false), PermissionVerdict::Forbidden)
        << "a request about no app in particular cannot be out of scope";
}

TEST(PanelAuthorizationTest, AnOwnerOnlyPermissionIsNotAnAdministratorsAndAnUnknownOneIsNobodys)
{
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Admin), "files.roots", false, false, false), PermissionVerdict::Forbidden);
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Owner), "files.roots", false, false, false), PermissionVerdict::Allowed);
    EXPECT_FALSE(PanelPermissions::Holds("console.everything"));
    EXPECT_EQ(PanelAuthorization::Weigh(As(PanelRole::Owner), "console.everything", false, false, false), PermissionVerdict::Forbidden)
        << "even the owner cannot hold what the catalog does not name";
}

namespace
{
    class PanelSubUserTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_store.Open(_directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error)) << error;
            _users = std::make_unique<PanelUsers>(_store);
            _grants = std::make_unique<PanelGrants>(_store);
            ASSERT_EQ(_users->Create("chico", "a good long password", true, false, &_owner, error), PanelUserResult::Ok) << error;
            ASSERT_EQ(_users->Create("helper", "a good long password", false, false, &_helper, error), PanelUserResult::Ok) << error;
            ASSERT_TRUE(_grants->Give(_helper, "gameserver", "power.restart", _owner, 1, error)) << error;
            _decide = std::make_unique<PanelAuthorization>(*_grants,
                [this](AdminRequest const&)
                {
                    std::string error;
                    return _users->FindById(_helper, error);
                },
                [this](AdminRequest const&, std::string_view permission, std::string_view app, bool allowed)
                {
                    _recorded.push_back(std::string(permission) + " on " + std::string(app) + (allowed ? " allowed" : " refused"));
                });
        }

        PermissionVerdict Asked(std::string path, std::string_view permission)
        {
            AdminRequest request;
            request.Method = "POST";
            request.Path = std::move(path);
            return _decide->Decide(request, permission);
        }

        LogTestDirectory _directory;
        PanelStore _store;
        std::unique_ptr<PanelUsers> _users;
        std::unique_ptr<PanelGrants> _grants;
        std::unique_ptr<PanelAuthorization> _decide;
        int64 _owner = 0;
        int64 _helper = 0;
        std::vector<std::string> _recorded;
    };
}

TEST_F(PanelSubUserTest, OneGrantOnOneAppReachesThatOneThingAndNothingElse)
{
    EXPECT_EQ(Asked("/api/apps/gameserver/api/power", "power.restart"), PermissionVerdict::Allowed);
    EXPECT_EQ(Asked("/api/apps/gameserver/api/power", "power.stop"), PermissionVerdict::Forbidden)
        << "the app is theirs to see, so being refused the rest of it tells them nothing new";
    EXPECT_EQ(Asked("/api/apps/gameserver/api/command", "console.write"), PermissionVerdict::Forbidden);
    EXPECT_EQ(Asked("/api/apps/loginserver/api/power", "power.restart"), PermissionVerdict::OutOfScope)
        << "an app they hold nothing on is an app they are not told about";
    EXPECT_EQ(Asked("/api/apps/gameserver/api/status", "status.read"), PermissionVerdict::Allowed)
        << "a viewer reads an app's status wherever they can see the app";
    EXPECT_EQ(Asked("/api/panel/users", "users.update"), PermissionVerdict::Forbidden)
        << "a request about no app in particular falls back to what the role allows";
}

TEST_F(PanelSubUserTest, ADangerousPermissionIsWrittenDownWhetherOrNotItWasAllowed)
{
    EXPECT_EQ(Asked("/api/apps/gameserver/api/settings", "settings.secrets.read"), PermissionVerdict::Forbidden);
    ASSERT_EQ(_recorded.size(), 1u);
    EXPECT_EQ(_recorded[0], "settings.secrets.read on gameserver refused") << "an attempt is the thing worth knowing about";

    EXPECT_EQ(Asked("/api/apps/gameserver/api/status", "status.read"), PermissionVerdict::Allowed);
    EXPECT_EQ(_recorded.size(), 1u) << "a permission the catalog does not call dangerous is not worth a row every time it is used";

    std::string error;
    ASSERT_TRUE(_grants->Give(_helper, "gameserver", "settings.secrets.read", _owner, 1, error)) << error;
    EXPECT_EQ(Asked("/api/apps/gameserver/api/settings", "settings.secrets.read"), PermissionVerdict::Allowed);
    ASSERT_EQ(_recorded.size(), 2u);
    EXPECT_EQ(_recorded[1], "settings.secrets.read on gameserver allowed");
}
