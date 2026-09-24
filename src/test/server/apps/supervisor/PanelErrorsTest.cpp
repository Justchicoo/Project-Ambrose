/*
 * Project Ambrose by Imjustchico
 * Tests what the panel keeps about the errors its apps raised: a group is found by the place it was raised at rather than by anything the app chose, so the same place reported twice is one row; a count that grows adds only what grew; a count that went backwards is read as the app having restarted and adds the whole of the new run rather than losing it or counting it twice; the first time stays the earliest ever seen; clearing a group is remembered as a time, so a group raised again afterwards reads as new again; and forgetting an app takes its groups with it.
 */

#include "LogTestDirectory.h"
#include "PanelErrors.h"
#include "PanelStore.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    class PanelErrorsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_store.Open(_directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error)) << error;
            _errors = std::make_unique<PanelErrors>(_store);
        }

        static PanelErrorGroup Raised(uint64 count, int64 first, int64 last, std::string message = "it failed")
        {
            PanelErrorGroup group;
            group.Category = "server.database";
            group.File = "src/server/database/Pool.cpp";
            group.Line = 42;
            group.Function = "Open";
            group.Template = "could not reach {}";
            group.Level = "error";
            group.Revision = "abc1234";
            group.Count = count;
            group.FirstEpochMs = first;
            group.LastEpochMs = last;
            group.LastMessage = std::move(message);
            return group;
        }

        PanelErrorGroup Only()
        {
            std::string error;
            std::vector<PanelErrorGroup> const groups = _errors->List(error);
            EXPECT_TRUE(error.empty()) << error;
            EXPECT_EQ(groups.size(), 1u);
            return groups.empty() ? PanelErrorGroup{} : groups.front();
        }

        LogTestDirectory _directory;
        PanelStore _store;
        std::unique_ptr<PanelErrors> _errors;
    };
}

TEST_F(PanelErrorsTest, ThePlaceItWasRaisedAtIsWhatFindsItAgain)
{
    std::string error;
    ASSERT_TRUE(_errors->Record("gameserver", { Raised(3, 1000, 2000) }, error)) << error;
    ASSERT_TRUE(_errors->Record("gameserver", { Raised(5, 1000, 3000, "it failed again") }, error)) << error;

    PanelErrorGroup const group = Only();
    EXPECT_EQ(group.App, "gameserver");
    EXPECT_EQ(group.Line, 42u);
    EXPECT_EQ(group.Count, 5u);
    EXPECT_EQ(group.TotalCount, 5u) << "three then five in one run is five, not eight";
    EXPECT_EQ(group.FirstEpochMs, 1000);
    EXPECT_EQ(group.LastEpochMs, 3000);
    EXPECT_EQ(group.LastMessage, "it failed again");
}

TEST_F(PanelErrorsTest, ACountThatWentBackwardsIsAnAppThatRestarted)
{
    std::string error;
    ASSERT_TRUE(_errors->Record("gameserver", { Raised(9, 1000, 2000) }, error)) << error;
    ASSERT_TRUE(_errors->Record("gameserver", { Raised(2, 5000, 6000) }, error)) << error;

    PanelErrorGroup const group = Only();
    EXPECT_EQ(group.Count, 2u) << "this run has raised it twice";
    EXPECT_EQ(group.TotalCount, 11u) << "nine before the restart and two after";
    EXPECT_EQ(group.FirstEpochMs, 1000) << "the first time ever seen, not the first time this run";
    EXPECT_EQ(group.LastEpochMs, 6000);
}

TEST_F(PanelErrorsTest, TwoAppsRaisingTheSameLineAreTwoGroupsAndForgettingOneLeavesTheOther)
{
    std::string error;
    ASSERT_TRUE(_errors->Record("gameserver", { Raised(1, 1000, 1000) }, error)) << error;
    ASSERT_TRUE(_errors->Record("loginserver", { Raised(1, 1000, 1000) }, error)) << error;
    EXPECT_EQ(_errors->List(error).size(), 2u);

    ASSERT_TRUE(_errors->Forget("gameserver", error)) << error;
    std::vector<PanelErrorGroup> const left = _errors->List(error);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left.front().App, "loginserver");
}

TEST_F(PanelErrorsTest, AGroupRaisedAgainAfterItWasClearedIsNewAgain)
{
    std::string error;
    ASSERT_TRUE(_errors->Record("gameserver", { Raised(1, 1000, 2000) }, error)) << error;
    PanelErrorGroup before = Only();
    EXPECT_FALSE(before.IsNewSinceCleared()) << "nothing has been cleared yet";

    ASSERT_TRUE(_errors->Clear(before.Id, 3000, error)) << error;
    EXPECT_FALSE(Only().IsNewSinceCleared()) << "cleared and not raised since";

    ASSERT_TRUE(_errors->Record("gameserver", { Raised(2, 1000, 4000) }, error)) << error;
    PanelErrorGroup const after = Only();
    EXPECT_TRUE(after.IsNewSinceCleared()) << "raised again after it was cleared";
    EXPECT_EQ(after.ClearedEpochMs.value_or(0), 3000) << "clearing is remembered rather than undone";
}
