/*
 * Project Ambrose by Imjustchico
 * Tests the notes kept on the client program's functions: that a note is set, replaced, found and removed by function, found by the words or milestone it holds, written and read back the same, that a missing file reads as no notes, and that a file of another version or with a broken note is refused whole.
 */

#include "FunctionNotes.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    class FunctionNotesTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            _folder = std::filesystem::temp_directory_path() / fmt::format("ambrose-notes-{}", std::chrono::steady_clock::now().time_since_epoch().count());
        }

        void TearDown() override
        {
            std::error_code code;
            std::filesystem::remove_all(_folder, code);
        }

        std::filesystem::path WriteText(std::string const& text) const
        {
            std::filesystem::path const path = FunctionNotes::PathFor(_folder, "r1");
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path, std::ios::binary) << text;
            return path;
        }

        std::filesystem::path _folder;
    };
}

TEST_F(FunctionNotesTest, ANoteIsSetReplacedFoundAndRemovedByFunction)
{
    FunctionNotes notes;
    notes.Set({ 0x140001000, "builds the spellbook", "8.05" });
    notes.Set({ 0x140002000, "looks up a wizbang", "" });
    notes.Set({ 0x140001000, "builds the spellbook from the player's list", "8.05" });
    EXPECT_EQ(notes.Size(), 2u);
    ASSERT_NE(notes.Find(0x140001000), nullptr);
    EXPECT_EQ(notes.Find(0x140001000)->Text, "builds the spellbook from the player's list");
    EXPECT_EQ(notes.Find(0x140003000), nullptr);
    EXPECT_TRUE(notes.Remove(0x140002000));
    EXPECT_FALSE(notes.Remove(0x140002000));
    EXPECT_EQ(notes.Size(), 1u);
}

TEST_F(FunctionNotesTest, NotesAreFoundByTheirWordsOrMilestoneInAddressOrder)
{
    FunctionNotes notes;
    notes.Set({ 0x140002000, "Looks up a WizBang", "6.02" });
    notes.Set({ 0x140001000, "builds the spellbook", "8.05" });
    EXPECT_EQ(notes.Matching("wizbang").size(), 1u);
    EXPECT_EQ(notes.Matching("8.05").front()->Function, 0x140001000u);
    EXPECT_TRUE(notes.Matching("nothing").empty());
    std::vector<FunctionNote const*> const all = notes.Matching("");
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0]->Function, 0x140001000u);
    EXPECT_EQ(all[1]->Function, 0x140002000u);
}

TEST_F(FunctionNotesTest, WrittenNotesReadBackTheSame)
{
    FunctionNotes written;
    written.Set({ 0x140001000, "builds the spellbook", "8.05" });
    written.Set({ 0x140002000, "looks up a wizbang", "" });
    std::filesystem::path const path = FunctionNotes::PathFor(_folder, "r1");
    std::string error;
    ASSERT_TRUE(written.Write(path, error)) << error;

    FunctionNotes read;
    ASSERT_TRUE(read.Read(path, error)) << error;
    EXPECT_EQ(read.Size(), 2u);
    ASSERT_NE(read.Find(0x140001000), nullptr);
    EXPECT_EQ(read.Find(0x140001000)->Milestone, "8.05");
    ASSERT_NE(read.Find(0x140002000), nullptr);
    EXPECT_EQ(read.Find(0x140002000)->Text, "looks up a wizbang");
    EXPECT_TRUE(read.Find(0x140002000)->Milestone.empty());
}

TEST_F(FunctionNotesTest, AMissingFileReadsAsNoNotes)
{
    FunctionNotes notes;
    std::string error;
    EXPECT_TRUE(notes.Read(FunctionNotes::PathFor(_folder, "r1"), error)) << error;
    EXPECT_EQ(notes.Size(), 0u);
}

TEST_F(FunctionNotesTest, AFileOfAnotherVersionOrWithABrokenNoteIsRefusedWhole)
{
    FunctionNotes notes;
    std::string error;
    EXPECT_FALSE(notes.Read(WriteText(R"({"version":2,"notes":[]})"), error));
    EXPECT_FALSE(error.empty());
    error.clear();
    EXPECT_FALSE(notes.Read(WriteText(R"({"version":1,"notes":[{"function":5368713216,"text":"fine"},{"function":"0x1","text":"bad"}]})"), error));
    EXPECT_EQ(notes.Size(), 0u);
    EXPECT_FALSE(notes.Read(WriteText("not json"), error));
}
