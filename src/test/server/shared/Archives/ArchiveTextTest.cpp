/*
 * Project Ambrose by Imjustchico
 * Tests the property and class names an archive's text files spell out, over an archive written by the test: plain and compressed XML, text and leftover .notxml entries give every property name that starts a word, and every class an element or a quoted name writes, once each, while a BINd entry, an entry of another kind, m_ inside a longer word and a class name that runs on into a template give none.
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
    builder.Add("trig_backup.notxml", std::string_view("<m_all key=\"1\"><class.Trigger><m_fourth>1</m_fourth></class.Trigger><Class Name=\"struct Nested::Info\"/></m_all>"), false);
    builder.Add("Held.xml", std::string_view("<Class Name=\"class SharedPointer<class Held>\"/>"), false);
    builder.Add("Stats.xml", std::string_view("BINd m_hidden"), false);
    builder.Add("Texture.dds", std::string_view("m_fifth <class.Hidden>"), false);
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
    EXPECT_EQ(ArchiveText::PropertyNames(*archive), (std::vector<std::string>{ "m_all", "m_first", "m_fourth", "m_second", "m_third" }));
    EXPECT_EQ(ArchiveText::ClassNames(*archive), (std::vector<std::string>{ "class Deck", "class Trigger", "struct Nested::Info" }));
}
