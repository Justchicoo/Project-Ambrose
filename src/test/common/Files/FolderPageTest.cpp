/*
 * Project Ambrose by Imjustchico
 * Tests one page of a folder listing: the filter runs before anything is counted, so the total is the size of the filtered set, folders come first and then the chosen order with the name breaking ties, descending reverses the order but keeps folders first, the offset and a limit clamped to 1 to 1000 cut the page, and a folder read short of its end says so.
 */

#include "FolderPage.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    Ambrose::JailListed Entry(std::string name, Ambrose::EntryKind kind, uint64 size, int64 modified)
    {
        Ambrose::JailListed entry;
        entry.Name = std::move(name);
        entry.Kind = kind;
        entry.Size = size;
        entry.ModifiedEpochMs = modified;
        return entry;
    }

    std::vector<Ambrose::JailListed> Sample()
    {
        return {
            Entry("zeta.log", Ambrose::EntryKind::File, 30, 300),
            Entry("Alpha.conf", Ambrose::EntryKind::File, 10, 100),
            Entry("types", Ambrose::EntryKind::Folder, 0, 50),
            Entry("beta.log", Ambrose::EntryKind::File, 20, 200),
            Entry("archive", Ambrose::EntryKind::Folder, 0, 400),
            Entry("gamma.txt", Ambrose::EntryKind::File, 20, 250),
        };
    }

    std::vector<std::string> Names(Ambrose::FolderPageResult const& page)
    {
        std::vector<std::string> names;
        for (Ambrose::JailListed const& entry : page.Entries)
            names.push_back(entry.Name);
        return names;
    }
}

TEST(FolderPageTest, FiltersBeforeItCountsSoTheTotalIsTheFilteredSet)
{
    Ambrose::FolderQuery query;
    query.Filter = "LOG";
    Ambrose::FolderPageResult const page = Ambrose::FolderPage::Build(Sample(), query, false);
    EXPECT_EQ(page.Total, 2u);
    EXPECT_EQ(Names(page), (std::vector<std::string>{ "beta.log", "zeta.log" }));
    EXPECT_TRUE(Ambrose::FolderPage::Matches("Alpha.conf", "alpha"));
    EXPECT_TRUE(Ambrose::FolderPage::Matches("anything", ""));
}

TEST(FolderPageTest, PutsFoldersFirstThenTheChosenOrder)
{
    Ambrose::FolderQuery query;
    EXPECT_EQ(Names(Ambrose::FolderPage::Build(Sample(), query, false)), (std::vector<std::string>{ "archive", "types", "Alpha.conf", "beta.log", "gamma.txt", "zeta.log" }));

    query.Sort = Ambrose::FolderSort::Size;
    EXPECT_EQ(Names(Ambrose::FolderPage::Build(Sample(), query, false)), (std::vector<std::string>{ "archive", "types", "Alpha.conf", "beta.log", "gamma.txt", "zeta.log" }))
        << "equal sizes fall back to the name";

    query.Sort = Ambrose::FolderSort::Modified;
    query.Descending = true;
    EXPECT_EQ(Names(Ambrose::FolderPage::Build(Sample(), query, false)), (std::vector<std::string>{ "archive", "types", "zeta.log", "gamma.txt", "beta.log", "Alpha.conf" }))
        << "descending keeps folders first";

    query.Sort = Ambrose::FolderSort::Kind;
    query.Descending = false;
    EXPECT_EQ(Names(Ambrose::FolderPage::Build(Sample(), query, false)), (std::vector<std::string>{ "archive", "types", "Alpha.conf", "beta.log", "zeta.log", "gamma.txt" }))
        << "kind orders by extension";

    EXPECT_EQ(Ambrose::FolderPage::ParseSort(""), Ambrose::FolderSort::Name);
    EXPECT_EQ(Ambrose::FolderPage::ParseSort("modified"), Ambrose::FolderSort::Modified);
    EXPECT_FALSE(Ambrose::FolderPage::ParseSort("colour").has_value());
    EXPECT_EQ(Ambrose::FolderPage::SortName(Ambrose::FolderSort::Kind), "kind");
}

TEST(FolderPageTest, CutsThePageFromTheOffsetWithAClampedLimit)
{
    Ambrose::FolderQuery query;
    query.Offset = 2;
    query.Limit = 3;
    Ambrose::FolderPageResult const page = Ambrose::FolderPage::Build(Sample(), query, false);
    EXPECT_EQ(page.Total, 6u);
    EXPECT_EQ(page.Offset, 2u);
    EXPECT_EQ(Names(page), (std::vector<std::string>{ "Alpha.conf", "beta.log", "gamma.txt" }));

    query.Offset = 50;
    EXPECT_TRUE(Ambrose::FolderPage::Build(Sample(), query, false).Entries.empty());
    EXPECT_EQ(Ambrose::FolderPage::Build(Sample(), query, false).Offset, 6u);

    query.Offset = 0;
    query.Limit = 0;
    EXPECT_EQ(Ambrose::FolderPage::Build(Sample(), query, false).Limit, 1u);
    query.Limit = 5000;
    EXPECT_EQ(Ambrose::FolderPage::Build(Sample(), query, false).Limit, Ambrose::FolderPage::MaxLimit);
}

TEST(FolderPageTest, SaysWhenTheFolderWasReadShortOfItsEnd)
{
    Ambrose::FolderQuery const query;
    EXPECT_TRUE(Ambrose::FolderPage::Build(Sample(), query, true).Truncated);
    EXPECT_FALSE(Ambrose::FolderPage::Build(Sample(), query, false).Truncated);
}
