/*
 * Project Ambrose by Imjustchico
 * Tests the grants that sit beside a role: a grant names one permission on one app and the catalog has to hold that permission, an app has to be named, nobody gives or takes their own, and a change bumps that one operator's session generation and nobody else's so what they may do is re-read at once while everybody else stays signed in. Also that removing an operator takes their grants with them, since a grant that outlives the person it was for would land on whoever is given that id next.
 */

#include "LogTestDirectory.h"
#include "PanelGrants.h"
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
    class PanelGrantsTest : public testing::Test
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
        }

        int64 GenerationOf(int64 id)
        {
            std::string error;
            std::optional<PanelUser> const user = _users->FindById(id, error);
            EXPECT_TRUE(user.has_value()) << error;
            return user ? user->Generation : 0;
        }

        LogTestDirectory _directory;
        PanelStore _store;
        std::unique_ptr<PanelUsers> _users;
        std::unique_ptr<PanelGrants> _grants;
        int64 _owner = 0;
        int64 _helper = 0;
    };
}

TEST_F(PanelGrantsTest, AGrantNamesOnePermissionTheCatalogHoldsOnOneApp)
{
    std::string error;
    EXPECT_FALSE(_grants->Give(_helper, "gameserver", "console.everything", _owner, 1, error));
    EXPECT_NE(error.find("console.everything"), std::string::npos) << error;

    error.clear();
    EXPECT_FALSE(_grants->Give(_helper, "", "power.restart", _owner, 1, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    ASSERT_TRUE(_grants->Give(_helper, "gameserver", "power.restart", _owner, 1, error)) << error;
    EXPECT_TRUE(_grants->Holds(_helper, "gameserver", "power.restart", error)) << error;
    EXPECT_FALSE(_grants->Holds(_helper, "loginserver", "power.restart", error)) << error;
    EXPECT_FALSE(_grants->Holds(_helper, "gameserver", "power.stop", error)) << error;
}

TEST_F(PanelGrantsTest, NobodyGivesOrTakesTheirOwnGrant)
{
    std::string error;
    EXPECT_FALSE(_grants->Give(_owner, "gameserver", "power.restart", _owner, 1, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(_grants->Take(_owner, "gameserver", "power.restart", _owner, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(PanelGrantsTest, AGrantChangeBumpsOneOperatorsGenerationAndNobodyElses)
{
    std::string error;
    int64 const ownerWas = GenerationOf(_owner);
    int64 const helperWas = GenerationOf(_helper);

    ASSERT_TRUE(_grants->Give(_helper, "gameserver", "power.restart", _owner, 1, error)) << error;
    EXPECT_EQ(GenerationOf(_helper), helperWas + 1);
    EXPECT_EQ(GenerationOf(_owner), ownerWas) << "a grant handed to one person ends nobody else's sessions";

    ASSERT_TRUE(_grants->Give(_helper, "gameserver", "power.restart", _owner, 1, error)) << error;
    EXPECT_EQ(GenerationOf(_helper), helperWas + 1) << "giving the same grant twice changes nothing, so it ends nothing";

    ASSERT_TRUE(_grants->Take(_helper, "gameserver", "power.restart", _owner, error)) << error;
    EXPECT_EQ(GenerationOf(_helper), helperWas + 2);
}

TEST_F(PanelGrantsTest, RemovingAnOperatorTakesTheirGrantsWithThem)
{
    std::string error;
    ASSERT_TRUE(_grants->Give(_helper, "gameserver", "power.restart", _owner, 1, error)) << error;
    ASSERT_EQ(_users->Remove(_helper, error), PanelUserResult::Ok) << error;
    EXPECT_TRUE(_grants->Of(_helper, error).empty()) << error;
    EXPECT_FALSE(_grants->Holds(_helper, "gameserver", "power.restart", error)) << error;
}
