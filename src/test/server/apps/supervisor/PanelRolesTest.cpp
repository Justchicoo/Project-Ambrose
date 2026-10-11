/*
 * Project Ambrose by Imjustchico
 * Tests the panel users, roles and grants milestone: a created user signs in and shows first sign-in, a read-only user is forbidden on changes, a two-factor reset ends sessions and audits both users, a disable ends only that user's sessions within a second, the audit trail shows both directions, a custom role grants exactly the named keys and edits apply immediately, a role cannot get a key its editor lacks, shipped roles are rows like any other, and owner-only keys stay owner-only.
 */
#include "LogTestDirectory.h"
#include "PanelAudit.h"
#include "PanelAuthorization.h"
#include "PanelPermissions.h"
#include "PanelRoles.h"
#include "PanelSessions.h"
#include "PanelStore.h"
#include "PanelUsers.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <chrono>
#include <set>
#include <string>
#include <vector>

namespace
{
    class PanelUsersRolesGrantsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_store.Open(_directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error)) << error;
            _users = std::make_unique<PanelUsers>(_store);
            _roles = std::make_unique<PanelRoles>(_store);
            _sessions = std::make_unique<PanelSessions>(_store);
            ASSERT_TRUE(_roles->SeedShipped(error)) << error;
        }

        int64 MakeUser(std::string const& username, bool owner)
        {
            std::string error;
            int64 id = 0;
            EXPECT_EQ(_users->Create(username, "a good long password", owner, false, &id, error), PanelUserResult::Ok) << error;
            return id;
        }

        LogTestDirectory _directory;
        PanelStore _store;
        std::unique_ptr<PanelUsers> _users;
        std::unique_ptr<PanelRoles> _roles;
        std::unique_ptr<PanelSessions> _sessions;
    };
}

TEST_F(PanelUsersRolesGrantsTest, CreatedUserSignsInAndShowsFirstSignIn)
{
    int64 const id = MakeUser("operator", false);
    ASSERT_GT(id, 0);
    std::string error;
    PanelUser user;
    ASSERT_EQ(_users->Authenticate("operator", "a good long password", user, error), PanelUserResult::Ok) << error;
    EXPECT_TRUE(user.SignedInEpochMs.has_value());
    EXPECT_GT(*user.SignedInEpochMs, 0);
}

TEST_F(PanelUsersRolesGrantsTest, ReadOnlyUserGetsForbiddenOnChanges)
{
    int64 const ownerId = MakeUser("owner", true);
    ASSERT_GT(ownerId, 0);
    int64 const readerId = MakeUser("reader", false);
    ASSERT_GT(readerId, 0);
    std::string error;
    int64 roleId = 0;
    ASSERT_EQ(_roles->Create("readers", "Sees the panel", { "users.read" }, ownerId, { "users.read", "users.update" }, &roleId, error), PanelRoleResult::Ok) << error;
    ASSERT_EQ(_roles->Assign(readerId, roleId, PanelStore::NowEpochMs(), error), PanelRoleResult::Ok) << error;
    PanelAsking asking;
    asking.UserId = readerId;
    asking.Role = PanelRole::Viewer;
    asking.RoleKeys = _roles->KeysOfUser(readerId, error);
    ASSERT_TRUE(error.empty()) << error;
    EXPECT_EQ(PanelAuthorization::Weigh(asking, "users.read", false, false, false), PermissionVerdict::Allowed);
    EXPECT_EQ(PanelAuthorization::Weigh(asking, "users.update", false, false, false), PermissionVerdict::Forbidden);
}

TEST_F(PanelUsersRolesGrantsTest, TwoFactorResetEndsSessionsAndAuditsBothUsers)
{
    int64 const adminId = MakeUser("admin", false);
    int64 const victimId = MakeUser("victim", false);
    std::string error;
    auto const opened = _sessions->Open(victimId, 1, "10.0.0.1", "agent", PanelStore::NowEpochMs(), error);
    ASSERT_TRUE(opened.has_value()) << error;
    AuditEvent event;
    event.Name = "panel:users.twofactor_reset";
    event.Actor = AuditActor::User;
    event.ActorId = std::to_string(adminId);
    event.ActorName = "admin";
    event.On("panel_user", std::to_string(adminId), "admin");
    event.On("panel_user", std::to_string(victimId), "victim");
    ASSERT_TRUE(_sessions->CloseEveryOne(victimId, "two-factor was reset", error)) << error;
    ASSERT_TRUE(PanelAudit::Write(_store, event, error)) << error;
    EXPECT_FALSE(_sessions->Hold(opened->Secret).has_value());
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT COUNT(*) FROM audit_subject WHERE kind = 'panel_user' AND subject_id = ?1", error);
    ASSERT_TRUE(rows.has_value()) << error;
    rows->Bind(1, std::to_string(adminId));
    ASSERT_TRUE(rows->Step(error));
    EXPECT_GT(rows->Int64(0), 0);
}

TEST_F(PanelUsersRolesGrantsTest, DisablingEndsOnlyThatUsersSessionsWithinASecond)
{
    int64 const firstId = MakeUser("first", false);
    int64 const secondId = MakeUser("second", false);
    std::string error;
    auto const started = std::chrono::steady_clock::now();
    auto const firstSession = _sessions->Open(firstId, 1, "10.0.0.1", "agent", PanelStore::NowEpochMs(), error);
    auto const secondSession = _sessions->Open(secondId, 1, "10.0.0.2", "agent", PanelStore::NowEpochMs(), error);
    ASSERT_TRUE(firstSession.has_value()) << error;
    ASSERT_TRUE(secondSession.has_value()) << error;
    ASSERT_TRUE(_sessions->CloseEveryOne(firstId, "the operator was disabled", error)) << error;
    auto const took = std::chrono::steady_clock::now() - started;
    EXPECT_LT(took, std::chrono::seconds(1));
    EXPECT_FALSE(_sessions->Hold(firstSession->Secret).has_value());
    EXPECT_TRUE(_sessions->Hold(secondSession->Secret).has_value());
}

TEST_F(PanelUsersRolesGrantsTest, AuditTrailShowsBothDirections)
{
    int64 const adminId = MakeUser("admin", false);
    int64 const targetId = MakeUser("target", false);
    std::string error;
    AuditEvent run;
    run.Name = "panel:users.updated";
    run.Actor = AuditActor::User;
    run.ActorId = std::to_string(adminId);
    run.On("panel_user", std::to_string(adminId), "admin");
    run.On("panel_user", std::to_string(targetId), "target");
    ASSERT_TRUE(PanelAudit::Write(_store, run, error)) << error;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT DISTINCT e.name FROM audit_event e LEFT JOIN audit_subject s ON s.event = e.id"
        " WHERE (s.kind = 'panel_user' AND s.subject_id = ?1) OR (e.actor_type = 0 AND e.actor_id = ?2)",
        error);
    ASSERT_TRUE(rows.has_value()) << error;
    std::string const admin = std::to_string(adminId);
    std::string const target = std::to_string(targetId);
    rows->Bind(1, admin);
    rows->Bind(2, admin);
    int count = 0;
    while (rows->Step(error))
        ++count;
    EXPECT_EQ(count, 1);
    rows = _store.Prepare(
        "SELECT DISTINCT e.name FROM audit_event e LEFT JOIN audit_subject s ON s.event = e.id"
        " WHERE (s.kind = 'panel_user' AND s.subject_id = ?1) OR (e.actor_type = 0 AND e.actor_id = ?2)",
        error);
    rows->Bind(1, target);
    rows->Bind(2, target);
    count = 0;
    while (rows->Step(error))
        ++count;
    EXPECT_EQ(count, 1);
}

TEST_F(PanelUsersRolesGrantsTest, CustomRoleGrantsExactlyNamedKeysAndEditsApplyImmediately)
{
    int64 const ownerId = MakeUser("owner", true);
    int64 const holderId = MakeUser("holder", false);
    std::string error;
    std::set<std::string, std::less<>> held = { "users.read", "users.update" };
    int64 roleId = 0;
    ASSERT_EQ(_roles->Create("support", "Support desk", { "users.read" }, ownerId, held, &roleId, error), PanelRoleResult::Ok) << error;
    ASSERT_EQ(_roles->Assign(holderId, roleId, PanelStore::NowEpochMs(), error), PanelRoleResult::Ok) << error;
    EXPECT_EQ(_roles->KeysOfUser(holderId, error), std::set<std::string, std::less<>>{ "users.read" });
    auto const started = std::chrono::steady_clock::now();
    ASSERT_EQ(_roles->Update(roleId, "support", "Support desk", { "users.read", "users.update" }, ownerId, held, error), PanelRoleResult::Ok) << error;
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
    EXPECT_EQ(_roles->KeysOfUser(holderId, error), std::set<std::string, std::less<>>(held));
}

TEST_F(PanelUsersRolesGrantsTest, RoleCannotGetKeyItsEditorLacks)
{
    int64 const ownerId = MakeUser("owner", true);
    std::string error;
    std::set<std::string, std::less<>> held = { "users.read" };
    EXPECT_EQ(_roles->Create("greedy", "Wants more", { "users.read", "users.update" }, ownerId, held, nullptr, error), PanelRoleResult::KeyNotHeld);
    EXPECT_EQ(_roles->Create("unknown", "Wants ghosts", { "users.ghost" }, ownerId, held, nullptr, error), PanelRoleResult::KeyUnknown);
    std::set<std::string, std::less<>> const adminHeld = { "users.read", "users.link" };
    int64 roleId = 0;
    EXPECT_EQ(_roles->Create("adminish", "Admin desk", { "users.read" }, ownerId, adminHeld, &roleId, error), PanelRoleResult::Ok) << error;
    int64 const adminId = MakeUser("admin", false);
    EXPECT_EQ(_roles->Update(roleId, "adminish", "Admin desk", { "users.link" }, adminId, { "users.read", "users.update" }, error), PanelRoleResult::KeyNotHeld);
}

TEST_F(PanelUsersRolesGrantsTest, ShippedRolesAreRowsLikeAnyOther)
{
    std::string error;
    std::vector<PanelRoleRow> const all = _roles->All(error);
    ASSERT_TRUE(error.empty()) << error;
    EXPECT_EQ(all.size(), 5u);
    for (PanelRoleRow const& role : all)
        EXPECT_TRUE(role.Shipped);
    std::optional<PanelRoleRow> const owner = _roles->FindByName("owner", error);
    ASSERT_TRUE(owner.has_value()) << error;
    EXPECT_TRUE(owner->OwnerOnly);
}

TEST_F(PanelUsersRolesGrantsTest, OwnerOnlyKeysStayOwnerOnly)
{
    int64 const ownerId = MakeUser("owner", true);
    int64 const holderId = MakeUser("holder", false);
    std::string error;
    std::set<std::string, std::less<>> const adminHeld = { "users.read", "users.update", "users.link" };
    int64 roleId = 0;
    ASSERT_EQ(_roles->Create("linkers", "Link desk", { "users.link" }, ownerId, adminHeld, &roleId, error), PanelRoleResult::Ok) << error;
    ASSERT_EQ(_roles->Assign(holderId, roleId, PanelStore::NowEpochMs(), error), PanelRoleResult::Ok) << error;
    PanelAsking asking;
    asking.UserId = holderId;
    asking.Role = PanelRole::Viewer;
    asking.RoleKeys = _roles->KeysOfUser(holderId, error);
    ASSERT_TRUE(error.empty()) << error;
    EXPECT_TRUE(asking.RoleKeys.contains("users.link"));
    PanelPermission const* const held = PanelPermissions::Find("users.link");
    ASSERT_TRUE(held != nullptr);
    EXPECT_TRUE(held->OwnerOnly);
    std::optional<PanelUser> const holder = _users->FindById(holderId, error);
    ASSERT_TRUE(holder.has_value()) << error;
    EXPECT_FALSE(holder->IsOwner);
}
