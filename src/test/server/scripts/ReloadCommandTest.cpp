/*
 * Project Ambrose by Imjustchico
 * Checks what an operator sees from the reload command, driven through the command tree the way a typed line reaches it: the bare command lists what this app can rebuild and which generation each is serving, a name nobody registered is said so and the list offered rather than silence, a target that builds answers with its new generation, one that refuses answers with every error it found and says the generation before it goes on serving, and 'all' answers for each target rather than only the last.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "ReloadMgr.h"
#include "WorldEditJournal.h"
#include "ScriptMgr.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

void AddSC_cs_reload();

namespace
{
    bool AnyLineHas(std::vector<std::string> const& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
    }

    class ReloadCommandTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            AddSC_cs_reload();
            sCommandMgr.Load(sScriptMgr.GetCommands());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            sCommandMgr.Clear();
            sScriptMgr.Unload();
        }

        static std::vector<std::string> Run(std::string_view line)
        {
            RecordingCaller caller(SEC_ADMINISTRATOR, true);
            EXPECT_EQ(sCommandMgr.Execute(caller, line), CommandResult::Ran) << line;
            return caller.GetLines();
        }
    };
}

TEST_F(ReloadCommandTest, TheBareCommandSaysWhenThereIsNothingToReload)
{
    EXPECT_TRUE(AnyLineHas(Run("reload"), "nothing registered")) << "an operator must be told the list is empty rather than shown an empty list";
}

TEST_F(ReloadCommandTest, TheBareCommandListsEachTargetAndItsGeneration)
{
    ASSERT_TRUE(sReloadMgr.Register("config", [](std::vector<std::string>&) { return true; }));
    ASSERT_TRUE(sReloadMgr.Register("messages", [](std::vector<std::string>&) { return true; }, { "config" }));

    std::vector<std::string> const lines = Run("reload");
    EXPECT_TRUE(AnyLineHas(lines, "config"));
    EXPECT_TRUE(AnyLineHas(lines, "messages"));
    EXPECT_TRUE(AnyLineHas(lines, "not reloaded since this app started"));
}

TEST_F(ReloadCommandTest, ANameNobodyRegisteredIsSaidSoAndTheListOffered)
{
    ASSERT_TRUE(sReloadMgr.Register("config", [](std::vector<std::string>&) { return true; }));

    std::vector<std::string> const lines = Run("reload invented");
    EXPECT_TRUE(AnyLineHas(lines, "invented"));
    EXPECT_TRUE(AnyLineHas(lines, "config")) << "the list must be offered rather than leaving the operator guessing the name";
    EXPECT_EQ(sReloadMgr.GetGeneration("config"), 0u) << "a mistyped name must not reload anything";
}

TEST_F(ReloadCommandTest, ATargetThatBuildsAnswersWithItsNewGeneration)
{
    ASSERT_TRUE(sReloadMgr.Register("config", [](std::vector<std::string>&) { return true; }));

    EXPECT_TRUE(AnyLineHas(Run("reload config"), "generation 1"));
    EXPECT_EQ(sReloadMgr.GetGeneration("config"), 1u);
}

TEST_F(ReloadCommandTest, ARefusalAnswersWithEveryErrorAndKeepsWhatWasServing)
{
    bool fail = false;
    ASSERT_TRUE(sReloadMgr.Register("config", [&fail](std::vector<std::string>& errors)
    {
        if (!fail)
            return true;
        errors.emplace_back("the first thing wrong");
        errors.emplace_back("the second thing wrong");
        return false;
    }));
    Run("reload config");

    fail = true;
    std::vector<std::string> const lines = Run("reload config");
    EXPECT_TRUE(AnyLineHas(lines, "goes on serving"));
    EXPECT_TRUE(AnyLineHas(lines, "the first thing wrong"));
    EXPECT_TRUE(AnyLineHas(lines, "the second thing wrong")) << "every error must be reported, not only the first";
    EXPECT_EQ(sReloadMgr.GetGeneration("config"), 1u);
}

TEST_F(ReloadCommandTest, TheJournalSaysWhenNothingHasBeenEdited)
{
    sWorldEditJournal.Clear();
    EXPECT_TRUE(AnyLineHas(Run("journal"), "Nothing has been edited"));
}

TEST_F(ReloadCommandTest, TheJournalListsWhatWasEditedAndWhoDidIt)
{
    sWorldEditJournal.Clear();
    sWorldEditJournal.Record("Wizard", "a command", "UPDATE `zone_template` SET `far_clip` = 1");
    std::vector<std::string> const lines = Run("journal");
    EXPECT_TRUE(AnyLineHas(lines, "Wizard"));
    EXPECT_TRUE(AnyLineHas(lines, "far_clip"));
    sWorldEditJournal.Clear();
}

TEST_F(ReloadCommandTest, ExportingAnEmptyJournalSaysWhyRatherThanWritingNothingQuietly)
{
    sWorldEditJournal.Clear();
    EXPECT_TRUE(AnyLineHas(Run("journal export"), "nothing has been edited"));
}

TEST_F(ReloadCommandTest, AllAnswersForEveryTarget)
{
    ASSERT_TRUE(sReloadMgr.Register("config", [](std::vector<std::string>&) { return true; }));
    ASSERT_TRUE(sReloadMgr.Register("messages", [](std::vector<std::string>&) { return true; }, { "config" }));

    std::vector<std::string> const lines = Run("reload all");
    EXPECT_TRUE(AnyLineHas(lines, "config"));
    EXPECT_TRUE(AnyLineHas(lines, "messages"));
    EXPECT_EQ(sReloadMgr.GetGeneration("config"), 1u);
    EXPECT_EQ(sReloadMgr.GetGeneration("messages"), 1u);
}
