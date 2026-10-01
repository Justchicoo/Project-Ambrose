/*
 * Project Ambrose by Imjustchico
 * Tests UTF-16 chat-filter lists read from synthetic Root.wad files: blacklist and whitelist decisions, replacement characters, malformed input, and reloads that preserve the serving generation when one required file is missing.
 */

#include "ChatFilter.h"

#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "ReloadMgr.h"
#include "Utf.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::array<std::string_view, 5> Entries{
        "ChatFilter/WhiteListBase.txt",
        "ChatFilter/WhiteListPhrasesBase.txt",
        "ChatFilter/BlackListBase.txt",
        "ChatFilter/ExceptionListBase.txt",
        "ChatFilter/CharacterReplacementMap.txt"
    };

    struct FixtureLists
    {
        std::u16string Whitelist;
        std::u16string WhitelistPhrases;
        std::u16string Blacklist;
        std::u16string Exceptions;
        std::u16string Replacements;
    };

    class ChatFilterTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            _filter.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            _filter.Clear();
            _filter.SetInstall({});
        }

        void Write(FixtureLists const& lists, bool includeWhitelist = true)
        {
            std::filesystem::path const folder = _directory.Path() / "Data" / "GameData";
            std::filesystem::create_directories(folder);
            KiwadBuilder builder(2);
            std::array<std::u16string_view, Entries.size()> contents{
                lists.Whitelist, lists.WhitelistPhrases, lists.Blacklist, lists.Exceptions, lists.Replacements
            };
            for (std::size_t index = 0; index < Entries.size(); ++index)
            {
                if (index == 0 && !includeWhitelist)
                    continue;
                std::vector<uint8> const bytes = Utf::StringToUtf16LEBytes(contents[index]);
                builder.Add(std::string(Entries[index]), bytes, true);
            }
            std::vector<uint8> const archive = builder.Build();
            std::ofstream stream(folder / "Root.wad", std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
        }

        std::shared_ptr<ChatFilterLists const> ReadWritten(std::vector<std::string>& errors)
        {
            std::string error;
            std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(_directory.Path() / "Data" / "GameData" / "Root.wad", error);
            EXPECT_TRUE(root) << error;
            return root ? ChatFilterLists::Read(*root, errors) : nullptr;
        }

        LogTestDirectory _directory;
        ChatFilterMgr _filter;
    };
}

TEST_F(ChatFilterTest, FindsBlacklistedWordsAndLetsWhitelistEntriesPass)
{
    Write({ u"allowed\n", u"safe badword phrase\n", u"badword\n", u"not badword\n", u"0=o\n4=a\n" });
    std::vector<std::string> errors;
    std::shared_ptr<ChatFilterLists const> const lists = ReadWritten(errors);
    ASSERT_TRUE(lists) << errors.front();
    EXPECT_EQ(lists->Inspect(u"that is a badword"), ChatFilterResult::Blacklisted);
    EXPECT_EQ(lists->Inspect(u"that is an allowed word"), ChatFilterResult::Clear);
    EXPECT_EQ(lists->Inspect(u"safe badword phrase"), ChatFilterResult::Whitelisted);
    EXPECT_EQ(lists->Inspect(u"not badword"), ChatFilterResult::Clear);
    EXPECT_EQ(lists->Inspect(u"b4dw0rd"), ChatFilterResult::Blacklisted);
    EXPECT_EQ(lists->Inspect(u"badwording"), ChatFilterResult::Clear);
}

TEST_F(ChatFilterTest, RejectsMalformedReplacementMapLines)
{
    FixtureLists malformed{ {}, {}, u"badword\n", {}, u"source without separator\n" };
    Write(malformed);
    std::vector<std::string> errors;
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.back().find("has no '=' separator"), std::string::npos) << errors.back();
}

TEST_F(ChatFilterTest, RejectsMalformedUtf16Text)
{
    std::filesystem::path const folder = _directory.Path() / "Data" / "GameData";
    std::filesystem::create_directories(folder);
    KiwadBuilder builder(2);
    builder.Add(std::string(Entries[0]), std::vector<uint8>{ 0x41 }, true);
    std::vector<uint8> const archive = builder.Build();
    std::ofstream stream(folder / "Root.wad", std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
    stream.close();

    std::vector<std::string> errors;
    EXPECT_FALSE(ReadWritten(errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.front().find("is not valid UTF-16LE"), std::string::npos) << errors.front();
}

TEST_F(ChatFilterTest, FailedReloadKeepsOldListsAndValidReloadSwapsTheWholeSnapshot)
{
    _filter.RegisterReloadTarget();
    Write({ {}, {}, u"oldword\n", {}, {} });
    ReloadOutcome const first = sReloadMgr.Reload(ChatFilterMgr::Target);
    ASSERT_TRUE(first.Ok) << first.Errors.front();
    std::shared_ptr<ChatFilterLists const> const old = _filter.GetLists();
    ASSERT_EQ(old->Inspect(u"oldword"), ChatFilterResult::Blacklisted);

    Write({ {}, {}, u"newword\n", {}, {} }, false);
    ReloadOutcome const failed = sReloadMgr.Reload(ChatFilterMgr::Target);
    EXPECT_FALSE(failed.Ok);
    ASSERT_FALSE(failed.Errors.empty());
    EXPECT_NE(failed.Errors.front().find("WhiteListBase.txt cannot be read"), std::string::npos);
    EXPECT_EQ(_filter.GetLists(), old);
    EXPECT_EQ(_filter.GetLists()->Inspect(u"oldword"), ChatFilterResult::Blacklisted);
    EXPECT_EQ(_filter.GetLists()->Inspect(u"newword"), ChatFilterResult::Clear);

    Write({ {}, {}, u"newword\n", {}, {} });
    ReloadOutcome const succeeded = sReloadMgr.Reload(ChatFilterMgr::Target);
    ASSERT_TRUE(succeeded.Ok) << succeeded.Errors.front();
    EXPECT_EQ(_filter.GetLists()->Inspect(u"oldword"), ChatFilterResult::Clear);
    EXPECT_EQ(_filter.GetLists()->Inspect(u"newword"), ChatFilterResult::Blacklisted);
}
