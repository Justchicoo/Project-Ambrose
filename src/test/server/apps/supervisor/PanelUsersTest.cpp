/*
 * Project Ambrose by Imjustchico
 * Tests the operators who sign in to the panel: a name is held to the account rules and cannot be taken twice or differ from another only by case, a password is held to one policy on every path that sets one and is never kept except as an Argon2id hash, an unknown user, a disabled user and a wrong password answer the same and take about the same time, a password change or a disable bumps the generation that ends that user's other sessions, the last owner cannot be removed, disabled or given a narrower role and nobody changes their own role, and the store says when it holds no user at all so the supervisor knows to make an owner.
 */

#include "LogTestDirectory.h"
#include "PanelStore.h"
#include "PanelUsers.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <chrono>
#include <string>
#include <vector>

namespace
{
    class PanelUsersTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_store.Open(_directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error)) << error;
            _users = std::make_unique<PanelUsers>(_store);
        }

        std::chrono::microseconds TimeOf(std::string_view username, std::string_view password, PanelUserResult expected)
        {
            PanelUser user;
            std::string error;
            auto const started = std::chrono::steady_clock::now();
            PanelUserResult const result = _users->Authenticate(username, password, user, error);
            auto const took = std::chrono::steady_clock::now() - started;
            EXPECT_EQ(result, expected) << error;
            return std::chrono::duration_cast<std::chrono::microseconds>(took);
        }

        LogTestDirectory _directory;
        PanelStore _store;
        std::unique_ptr<PanelUsers> _users;
    };
}

TEST_F(PanelUsersTest, SaysWhenItHoldsNoUserAndMakesAnOwner)
{
    std::string error;
    EXPECT_TRUE(_users->IsEmpty(error)) << error;

    int64 id = 0;
    ASSERT_EQ(_users->Create("owner", "a good long password", true, true, &id, error), PanelUserResult::Ok) << error;
    EXPECT_GT(id, 0);
    EXPECT_FALSE(_users->IsEmpty(error)) << error;

    std::optional<PanelUser> const owner = _users->Find("OWNER", error);
    ASSERT_TRUE(owner.has_value()) << error;
    EXPECT_EQ(owner->Username, "owner");
    EXPECT_TRUE(owner->IsOwner);
    EXPECT_TRUE(owner->MustChange);
    EXPECT_FALSE(owner->Disabled);
    EXPECT_EQ(owner->Generation, 1);
    EXPECT_FALSE(owner->SignedInEpochMs.has_value());
}

TEST_F(PanelUsersTest, HoldsANameToTheAccountRulesAndTakesItOnlyOnce)
{
    std::string error;
    EXPECT_EQ(_users->Create("ab", "a good long password", false, false, nullptr, error), PanelUserResult::NameTooShort);
    EXPECT_EQ(_users->Create(std::string(33, 'a'), "a good long password", false, false, nullptr, error), PanelUserResult::NameTooLong);
    EXPECT_EQ(_users->Create("has space", "a good long password", false, false, nullptr, error), PanelUserResult::NameInvalid);

    ASSERT_EQ(_users->Create("keeper", "a good long password", false, false, nullptr, error), PanelUserResult::Ok) << error;
    EXPECT_EQ(_users->Create("keeper", "another long password", false, false, nullptr, error), PanelUserResult::NameTaken);
    EXPECT_EQ(_users->Create("KEEPER", "another long password", false, false, nullptr, error), PanelUserResult::NameTaken);
}

TEST_F(PanelUsersTest, HoldsAPasswordToOnePolicyOnEveryPathThatSetsOne)
{
    std::string error;
    EXPECT_EQ(_users->GetPolicy().MinLength, PanelPasswordPolicy::DefaultLength);

    EXPECT_EQ(_users->Create("keeper", "short", false, false, nullptr, error), PanelUserResult::PasswordTooShort);
    EXPECT_EQ(_users->Create("keeper", std::string(129, 'a'), false, false, nullptr, error), PanelUserResult::PasswordTooLong);
    EXPECT_EQ(_users->Create("keeper", "has\ta tab in it", false, false, nullptr, error), PanelUserResult::PasswordInvalid);
    EXPECT_EQ(_users->Create("averylongkeepername", "AVeryLongKeeperName", false, false, nullptr, error), PanelUserResult::PasswordIsTheName);

    int64 id = 0;
    ASSERT_EQ(_users->Create("keeper", "a good long password", false, false, &id, error), PanelUserResult::Ok) << error;
    EXPECT_EQ(_users->SetPassword(id, "short", false, error), PanelUserResult::PasswordTooShort);
    EXPECT_EQ(_users->SetPassword(id, "has\ta tab in it", false, error), PanelUserResult::PasswordInvalid);

    PanelPasswordPolicy loose;
    loose.MinLength = 2;
    _users->SetPolicy(loose);
    EXPECT_EQ(_users->GetPolicy().MinLength, PanelPasswordPolicy::FloorLength);
    EXPECT_EQ(_users->SetPassword(id, "seven77", false, error), PanelUserResult::PasswordTooShort);
    EXPECT_EQ(_users->SetPassword(id, "eight888", false, error), PanelUserResult::Ok) << error;
}

TEST_F(PanelUsersTest, KeepsAPasswordOnlyAsAnArgon2idHash)
{
    std::string error;
    ASSERT_EQ(_users->Create("keeper", "a good long password", false, false, nullptr, error), PanelUserResult::Ok) << error;

    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT password_hash FROM panel_user", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    std::string const hash = rows->Text(0);
    rows.reset();

    EXPECT_EQ(hash.rfind("$argon2id$", 0), 0u) << hash;
    EXPECT_EQ(hash.find("a good long password"), std::string::npos);
    EXPECT_TRUE(PanelUsers::PasswordMatches(hash, "a good long password"));
    EXPECT_FALSE(PanelUsers::PasswordMatches(hash, "a good long passwore"));

    std::string second;
    ASSERT_TRUE(PanelUsers::HashPassword("a good long password", second, error)) << error;
    EXPECT_NE(second, hash);
}

TEST_F(PanelUsersTest, AnswersTheSameAndTakesTheSameTimeForEveryRefusal)
{
    std::string error;
    int64 disabled = 0;
    ASSERT_EQ(_users->Create("keeper", "a good long password", false, false, nullptr, error), PanelUserResult::Ok) << error;
    ASSERT_EQ(_users->Create("closed", "a good long password", false, false, &disabled, error), PanelUserResult::Ok) << error;
    ASSERT_TRUE(_users->SetDisabled(disabled, true, error)) << error;

    PanelUser user;
    EXPECT_EQ(_users->Authenticate("keeper", "a good long password", user, error), PanelUserResult::Ok) << error;
    EXPECT_EQ(user.Username, "keeper");

    EXPECT_EQ(PanelUsers::Explain(PanelUserResult::UnknownUser), PanelUsers::Explain(PanelUserResult::WrongPassword));
    EXPECT_EQ(PanelUsers::Explain(PanelUserResult::Disabled), PanelUsers::Explain(PanelUserResult::WrongPassword));

    std::vector<std::chrono::microseconds> unknown;
    std::vector<std::chrono::microseconds> wrong;
    std::vector<std::chrono::microseconds> closed;
    for (int round = 0; round < 5; ++round)
    {
        unknown.push_back(TimeOf("nobody", "a good long password", PanelUserResult::UnknownUser));
        wrong.push_back(TimeOf("keeper", "not the right password", PanelUserResult::WrongPassword));
        closed.push_back(TimeOf("closed", "a good long password", PanelUserResult::Disabled));
    }
    auto const middle = [](std::vector<std::chrono::microseconds>& taken)
    {
        std::sort(taken.begin(), taken.end());
        return static_cast<double>(taken[taken.size() / 2].count());
    };
    double const slowest = std::max({ middle(unknown), middle(wrong), middle(closed) });
    double const quickest = std::min({ middle(unknown), middle(wrong), middle(closed) });
    EXPECT_LT(slowest, quickest * 3.0) << "unknown " << middle(unknown) << "us, wrong " << middle(wrong) << "us, disabled " << middle(closed) << "us";
}

TEST_F(PanelUsersTest, APasswordChangeOrADisableBumpsTheGeneration)
{
    std::string error;
    int64 id = 0;
    ASSERT_EQ(_users->Create("keeper", "a good long password", false, false, &id, error), PanelUserResult::Ok) << error;
    ASSERT_EQ(_users->FindById(id, error)->Generation, 1);

    ASSERT_EQ(_users->SetPassword(id, "another long password", true, error), PanelUserResult::Ok) << error;
    std::optional<PanelUser> after = _users->FindById(id, error);
    ASSERT_TRUE(after.has_value()) << error;
    EXPECT_EQ(after->Generation, 2);
    EXPECT_TRUE(after->MustChange);

    ASSERT_TRUE(_users->SetDisabled(id, true, error)) << error;
    after = _users->FindById(id, error);
    ASSERT_TRUE(after.has_value()) << error;
    EXPECT_EQ(after->Generation, 3);
    EXPECT_TRUE(after->Disabled);

    EXPECT_FALSE(_users->SetDisabled(id + 100, true, error));

    ASSERT_TRUE(_users->RecordSignIn(id, error)) << error;
    after = _users->FindById(id, error);
    ASSERT_TRUE(after.has_value()) << error;
    ASSERT_TRUE(after->SignedInEpochMs.has_value());
    EXPECT_GT(*after->SignedInEpochMs, 0);
}

TEST_F(PanelUsersTest, ListsEveryUserByName)
{
    std::string error;
    ASSERT_EQ(_users->Create("zara", "a good long password", false, false, nullptr, error), PanelUserResult::Ok) << error;
    ASSERT_EQ(_users->Create("Alice", "a good long password", true, false, nullptr, error), PanelUserResult::Ok) << error;

    std::vector<PanelUser> const users = _users->List(error);
    ASSERT_EQ(users.size(), 2u) << error;
    EXPECT_EQ(users[0].Username, "Alice");
    EXPECT_TRUE(users[0].IsOwner);
    EXPECT_EQ(users[1].Username, "zara");
    EXPECT_FALSE(users[1].IsOwner);
}

TEST_F(PanelUsersTest, TheLastOwnerCannotBeRemovedDisabledOrDemoted)
{
    std::string error;
    int64 owner = 0;
    ASSERT_EQ(_users->Create("chico", "a good long password", true, false, &owner, error), PanelUserResult::Ok) << error;
    EXPECT_EQ(_users->CountOwners(error), 1u) << error;
    EXPECT_TRUE(_users->IsLastOwner(owner, error)) << error;

    EXPECT_FALSE(_users->SetDisabled(owner, true, error));
    EXPECT_EQ(_users->SetRole(owner, PanelRole::Admin, owner + 50, error), PanelUserResult::LastOwner);
    EXPECT_EQ(_users->Remove(owner, error), PanelUserResult::LastOwner);

    std::optional<PanelUser> still = _users->FindById(owner, error);
    ASSERT_TRUE(still.has_value()) << error;
    EXPECT_FALSE(still->Disabled);
    EXPECT_EQ(still->Role, PanelRole::Owner);

    int64 second = 0;
    ASSERT_EQ(_users->Create("mate", "a good long password", false, false, &second, error), PanelUserResult::Ok) << error;
    ASSERT_EQ(_users->SetRole(second, PanelRole::Owner, owner, error), PanelUserResult::Ok) << error;
    EXPECT_EQ(_users->CountOwners(error), 2u) << error;
    EXPECT_FALSE(_users->IsLastOwner(owner, error)) << error;

    EXPECT_EQ(_users->SetRole(owner, PanelRole::Admin, second, error), PanelUserResult::Ok) << error;
    still = _users->FindById(owner, error);
    ASSERT_TRUE(still.has_value()) << error;
    EXPECT_EQ(still->Role, PanelRole::Admin);
    EXPECT_FALSE(still->IsOwner) << "the role is what says who the owner is, so the older column follows it";

    EXPECT_EQ(_users->SetRole(second, PanelRole::Viewer, second, error), PanelUserResult::Themselves)
        << "nobody narrows or widens their own role, whoever they are";
    EXPECT_EQ(_users->Remove(second, error), PanelUserResult::LastOwner);
    EXPECT_EQ(_users->Remove(owner, error), PanelUserResult::Ok) << error;
    EXPECT_FALSE(_users->FindById(owner, error).has_value()) << error;
}

TEST_F(PanelUsersTest, ADisabledOwnerIsNotAnOwnerWhoCouldUndoIt)
{
    std::string error;
    int64 owner = 0;
    int64 spare = 0;
    ASSERT_EQ(_users->Create("chico", "a good long password", true, false, &owner, error), PanelUserResult::Ok) << error;
    ASSERT_EQ(_users->Create("spare", "a good long password", false, false, &spare, error), PanelUserResult::Ok) << error;
    ASSERT_EQ(_users->SetRole(spare, PanelRole::Owner, owner, error), PanelUserResult::Ok) << error;
    ASSERT_TRUE(_users->SetDisabled(spare, true, error)) << error;

    EXPECT_EQ(_users->CountOwners(error), 1u) << error;
    EXPECT_FALSE(_users->SetDisabled(owner, true, error)) << "an owner who cannot sign in cannot undo anything";
    EXPECT_EQ(_users->SetRole(owner, PanelRole::Viewer, spare, error), PanelUserResult::LastOwner);
}

TEST_F(PanelUsersTest, ARoleChangeBumpsTheGenerationThatEndsThatUsersSessions)
{
    std::string error;
    int64 owner = 0;
    int64 other = 0;
    ASSERT_EQ(_users->Create("chico", "a good long password", true, false, &owner, error), PanelUserResult::Ok) << error;
    ASSERT_EQ(_users->Create("other", "a good long password", false, false, &other, error), PanelUserResult::Ok) << error;
    std::optional<PanelUser> const before = _users->FindById(other, error);
    ASSERT_TRUE(before.has_value()) << error;

    ASSERT_EQ(_users->SetRole(other, PanelRole::Operator, owner, error), PanelUserResult::Ok) << error;
    std::optional<PanelUser> const after = _users->FindById(other, error);
    ASSERT_TRUE(after.has_value()) << error;
    EXPECT_EQ(after->Generation, before->Generation + 1);

    std::optional<PanelUser> const untouched = _users->FindById(owner, error);
    ASSERT_TRUE(untouched.has_value()) << error;
    EXPECT_EQ(untouched->Generation, 1) << "changing one person's role ends nobody else's sessions";
}
