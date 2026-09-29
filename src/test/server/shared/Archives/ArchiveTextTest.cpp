/*
 * Project Ambrose by Imjustchico
 * Tests the property names an archive's text files spell out, over an archive written by the test: plain and compressed XML and text entries give every name that starts a word, once each, while a BINd entry, an entry of another kind and m_ inside a longer word give none.
 */

#include "ArchiveText.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

TEST(ArchiveTextTest, TheNamesTextFilesSpellOutAreFoundOnceEachAndNoneElsewhere)
{
    KiwadBuilder builder(2);
    builder.Add("Objects/Deck.xml", std::string_view("<Objects><Class Name=\"class Deck\"><m_first>1</m_first><m_second/></Class></Objects>"), false);
    builder.Add("Notes.txt", std::string_view("Random_Mob uses m_third and m_first"), true);
    builder.Add("Stats.xml", std::string_view("BINd m_hidden"), false);
    builder.Add("Texture.dds", std::string_view("m_fourth"), false);
    LogTestDirectory directory;
    std::filesystem::path const path = directory.Path() / "Names.wad";
    std::vector<uint8> const bytes = builder.Build();
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error);
    ASSERT_TRUE(archive) << error;
    EXPECT_EQ(ArchiveText::PropertyNames(*archive), (std::vector<std::string>{ "m_first", "m_second", "m_third" }));
}
