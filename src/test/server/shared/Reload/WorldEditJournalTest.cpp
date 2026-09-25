/*
 * Project Ambrose by Imjustchico
 * Checks what the journal promises: an edit is written down with who made it and where it came from, an edit with nothing in it is not written down at all, a journal that fills keeps the newest rather than the first, the exported file carries the branding header and every statement in the order they were made with a semicolon whether the caller wrote one or not, a line break in who made an edit or where it came from stays inside its comment line, an export names itself the way every other update file is named and counts within the day rather than overwriting, and exporting an empty journal writes no file and says why.
 */

#include "WorldEditJournal.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    class WorldEditJournalTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorldEditJournal.Clear();
            _folder = std::filesystem::temp_directory_path() / ("ambrose_journal_" + std::to_string(std::random_device()()));
        }

        void TearDown() override
        {
            sWorldEditJournal.Clear();
            std::error_code code;
            std::filesystem::remove_all(_folder, code);
        }

        static WorldEdit At(int64 epochMs, std::string statement)
        {
            WorldEdit edit;
            edit.EpochMs = epochMs;
            edit.Who = "Wizard";
            edit.Source = "a command";
            edit.Statement = std::move(statement);
            return edit;
        }

        static std::string Read(std::filesystem::path const& file)
        {
            std::ifstream in(file, std::ios::binary);
            std::ostringstream text;
            text << in.rdbuf();
            return text.str();
        }

        std::filesystem::path _folder;
    };

    constexpr int64 OneDay = 24LL * 60 * 60 * 1000;
}

TEST_F(WorldEditJournalTest, AnEditIsWrittenDownWithWhoMadeItAndWhereItCameFrom)
{
    sWorldEditJournal.Record("Wizard", "a command", "UPDATE `zone_template` SET `far_clip` = 1 WHERE `zone` = 'a'");
    ASSERT_EQ(sWorldEditJournal.Count(), 1u);

    std::vector<WorldEdit> const edits = sWorldEditJournal.Entries();
    EXPECT_EQ(edits.front().Who, "Wizard");
    EXPECT_EQ(edits.front().Source, "a command");
    EXPECT_GT(edits.front().EpochMs, 0) << "an edit must know when it was made";
}

TEST_F(WorldEditJournalTest, AnEditWithNoStatementIsNotWrittenDown)
{
    sWorldEditJournal.Record("Wizard", "a command", "");
    sWorldEditJournal.Record("Wizard", "a command", "   ");
    EXPECT_EQ(sWorldEditJournal.Count(), 0u) << "a blank statement is not an edit";
}

TEST_F(WorldEditJournalTest, AnEditThatNamesNobodyStillSaysSo)
{
    sWorldEditJournal.Record("", "", "DELETE FROM `zone_object` WHERE `id` = 1");
    ASSERT_EQ(sWorldEditJournal.Count(), 1u);
    EXPECT_FALSE(sWorldEditJournal.Entries().front().Who.empty());
    EXPECT_FALSE(sWorldEditJournal.Entries().front().Source.empty());
}

TEST_F(WorldEditJournalTest, AFullJournalKeepsTheNewest)
{
    for (std::size_t made = 0; made < WorldEditJournal::MaxEntries + 50; ++made)
        sWorldEditJournal.Record(At(1, "SELECT " + std::to_string(made)));

    EXPECT_EQ(sWorldEditJournal.Count(), WorldEditJournal::MaxEntries);
    std::vector<WorldEdit> const edits = sWorldEditJournal.Entries();
    EXPECT_NE(edits.back().Statement.find(std::to_string(WorldEditJournal::MaxEntries + 49)), std::string::npos)
        << "the newest edit must survive";
    EXPECT_EQ(edits.front().Statement.find("SELECT 0"), std::string::npos) << "the oldest must be the one dropped";
}

TEST_F(WorldEditJournalTest, TheRenderedFileCarriesTheHeaderAndEveryStatementInOrder)
{
    std::vector<WorldEdit> const edits{ At(1, "UPDATE a SET b = 1"), At(2, "DELETE FROM c WHERE d = 2;") };
    std::string const text = WorldEditJournal::Render(edits);

    EXPECT_TRUE(text.starts_with("-- Project Ambrose by Imjustchico\n")) << "every SQL file carries the branding header";
    EXPECT_NE(text.find("UPDATE a SET b = 1;"), std::string::npos) << "a statement without a semicolon is given one";
    EXPECT_NE(text.find("DELETE FROM c WHERE d = 2;"), std::string::npos);
    EXPECT_EQ(text.find("DELETE FROM c WHERE d = 2;") > text.find("UPDATE a SET b = 1;"), true) << "the order they were made is the order they are written";
    EXPECT_NE(text.find("by Wizard from a command"), std::string::npos) << "a reviewer must see who made each edit";
}

TEST_F(WorldEditJournalTest, ALineBreakInWhoOrWhereCannotEndTheCommentLine)
{
    WorldEdit edit = At(1, "UPDATE a SET b = 1");
    edit.Who = "Wizard\nDROP TABLE zone_object;";
    edit.Source = "the panel\r\nDELETE FROM c";
    std::string const text = WorldEditJournal::Render({ edit });

    EXPECT_EQ(text.find("\nDROP TABLE"), std::string::npos) << "text after a break in a name would otherwise run as a statement";
    EXPECT_EQ(text.find("\nDELETE FROM c"), std::string::npos);
    EXPECT_NE(text.find("by Wizard DROP TABLE zone_object; from the panel  DELETE FROM c\n"), std::string::npos) << text;
    EXPECT_NE(text.find("\nUPDATE a SET b = 1;"), std::string::npos) << "the statement itself still stands on a line of its own";
}

TEST_F(WorldEditJournalTest, AnExportIsNamedTheWayEveryOtherUpdateFileIsAndCountsWithinTheDay)
{
    sWorldEditJournal.Record(At(OneDay * 20000, "UPDATE a SET b = 1"));

    std::string error;
    std::optional<std::filesystem::path> const first = sWorldEditJournal.Export(_folder, error);
    ASSERT_TRUE(first.has_value()) << error;
    EXPECT_TRUE(std::filesystem::exists(*first));
    EXPECT_TRUE(first->filename().string().ends_with("_00.sql")) << first->filename().string();

    std::optional<std::filesystem::path> const second = sWorldEditJournal.Export(_folder, error);
    ASSERT_TRUE(second.has_value()) << error;
    EXPECT_NE(first->filename(), second->filename()) << "a second export must not overwrite the first";
    EXPECT_TRUE(second->filename().string().ends_with("_01.sql")) << second->filename().string();

    EXPECT_NE(Read(*first).find("UPDATE a SET b = 1;"), std::string::npos);
}

TEST_F(WorldEditJournalTest, ExportingAnEmptyJournalWritesNothingAndSaysWhy)
{
    std::string error;
    EXPECT_FALSE(sWorldEditJournal.Export(_folder, error).has_value());
    EXPECT_FALSE(error.empty()) << "an operator must be told why nothing was written";
    EXPECT_FALSE(std::filesystem::exists(_folder / "anything.sql"));
}
