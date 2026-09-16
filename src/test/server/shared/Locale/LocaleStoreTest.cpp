/*
 * Project Ambrose by Imjustchico
 * Tests the locale store on synthetic archives the test writes: the default locale loads at once with its file, entry, key and duplicate counts, keys resolve in it and in another locale that loads on first use, stems with spaces and commas keep their full keys, text finds its keys and a table lists its entries in file order, a malformed file is skipped and named on its table while the rest of the locale loads, a missing locale or one none of whose files load is reported, a reload keeps the previous store when its default locale or a locale already in use cannot be built, a default locale change applies only when the new locale loads, and concurrent first lookups build a locale once.
 */

#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LocaleStore.h"
#include "LogTestDirectory.h"
#include "Utf.h"

#include <gtest/gtest.h>

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    std::vector<uint8> Lang(std::string_view utf8)
    {
        std::vector<uint8> bytes{ 0xFF, 0xFE };
        std::vector<uint8> const text = Utf::StringToUtf16LEBytes(*Utf::Utf8ToUtf16(utf8, Utf::InvalidPolicy::Reject));
        bytes.insert(bytes.end(), text.begin(), text.end());
        return bytes;
    }

    std::filesystem::path Write(LogTestDirectory const& directory, std::string const& name, std::vector<uint8> const& bytes)
    {
        std::filesystem::path const path = directory.Path() / name;
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return path;
    }

    KiwadBuilder MakeArchive()
    {
        KiwadBuilder builder(2);
        builder.Add("Locale/en-US/Items.lang", Lang("1:Items\r\n00028316\r\n\r\nFairy Hat\r\n00000001\r\n\r\nPlain Wand\r\n00028316\r\nlater copy\r\nFairy Crown\r\n"), true);
        builder.Add("Locale/en-US/Persona, First.lang", Lang("1:Persona, First\r\n0001\r\n\r\nAlden\r\n"), false);
        builder.Add("Locale/en-US/QuestTitle.lang", Lang("1:QuestTitle\r\n00001718\r\n\r\nTo the Commons!\r\n00001719\r\n\r\nPlain Wand\r\n"), true);
        builder.Add("Locale/de/Items.lang", Lang("1:Items\r\n00028316\r\n\r\nFeenhut\r\n"), true);
        builder.Add("Locale/fr/Items.lang", Lang("1:Items\r\n00028316\r\n"), true);
        builder.Add("Locale/de/Zones.lang", Lang("Zones\r\n1\r\n\r\nRabenwald\r\n"), false);
        builder.Add("Locale/en-US/Nested/Ignored.lang", Lang("1:Ignored\r\n1\r\n\r\nNo\r\n"), false);
        builder.Add("Locale/StringTable.xml", "not a lang file", false);
        builder.Add("ObjectData/Hat.xml", "BINd", false);
        return builder;
    }
}

TEST(LocaleStoreTest, TheDefaultLocaleLoadsAndOthersLoadOnFirstUse)
{
    LogTestDirectory directory;
    std::filesystem::path const wad = Write(directory, "Root.wad", MakeArchive().Build());
    LocaleStore store;
    EXPECT_FALSE(store.IsLoaded());
    EXPECT_FALSE(store.Resolve("Items_00028316"));
    std::string error;
    ASSERT_TRUE(store.Load(wad, "en-US", error)) << error;
    EXPECT_TRUE(store.IsLoaded());
    EXPECT_EQ(store.GetDefaultLocale(), "en-US");
    EXPECT_EQ(store.GetLocales(), (std::vector<std::string>{ "de", "en-US", "fr" }));

    std::shared_ptr<LocaleTable const> const english = store.GetTable();
    ASSERT_TRUE(english);
    EXPECT_EQ(english->GetFileCount(), 3u);
    EXPECT_EQ(english->GetEntryCount(), 6u);
    EXPECT_EQ(english->GetKeyCount(), 5u);
    EXPECT_EQ(english->GetDuplicateCount(), 1u);
    EXPECT_EQ(store.Resolve("Items_00028316"), "Fairy Crown");
    EXPECT_EQ(store.Resolve("QuestTitle_00001718"), "To the Commons!");
    EXPECT_EQ(store.Resolve("Persona, First_0001"), "Alden");
    EXPECT_TRUE(store.HasKey("Items_00000001"));
    EXPECT_FALSE(store.HasKey("Items_1"));
    EXPECT_FALSE(store.HasKey("Ignored_1"));
    EXPECT_EQ(english->FindKeys("Plain Wand"), (std::vector<std::string>{ "Items_00000001", "QuestTitle_00001719" }));
    std::vector<std::pair<std::string, std::string>> const items = english->GetEntries("Items");
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0], (std::pair<std::string, std::string>{ "Items_00028316", "Fairy Crown" }));
    EXPECT_EQ(items[1], (std::pair<std::string, std::string>{ "Items_00000001", "Plain Wand" }));
    EXPECT_EQ(english->GetStems(), (std::vector<std::string>{ "Items", "Persona, First", "QuestTitle" }));

    EXPECT_EQ(store.Resolve("Items_00028316", "de"), "Feenhut");
    EXPECT_FALSE(store.Resolve("QuestTitle_00001718", "de"));
    std::shared_ptr<LocaleTable const> const german = store.GetTable("de");
    ASSERT_TRUE(german);
    EXPECT_EQ(german->GetFileCount(), 1u);
    EXPECT_EQ(german->GetProblems(), (std::vector<std::string>{ "Locale/de/Zones.lang: does not start with a 1:<Stem> header line" }));
    EXPECT_TRUE(english->GetProblems().empty());
    EXPECT_TRUE(english->HasStem("Items"));
    EXPECT_FALSE(english->HasStem("Zones"));
    std::string missing;
    EXPECT_FALSE(store.GetTable("fr", &missing));
    EXPECT_EQ(missing, "the fr locale cannot be loaded: none of its 1 files load; the first fails with Locale/fr/Items.lang: ends on line 2 in the middle of an entry, which needs a key, a metadata and a text line");
    EXPECT_FALSE(store.GetTable("pl", &missing));
    EXPECT_EQ(missing, "there is no pl locale; the install has de, en-US, fr");
}

TEST(LocaleStoreTest, AFailedReloadOrDefaultChangeKeepsThePreviousStore)
{
    LogTestDirectory directory;
    std::filesystem::path const good = Write(directory, "Good.wad", MakeArchive().Build());
    std::filesystem::path const bad = Write(directory, "Bad.wad", KiwadBuilder(2).Add("Locale/en-US/Zones.lang", Lang("Zones\r\n1\r\n\r\nRavenwood\r\n"), false).Add("Locale/de/Items.lang", Lang("1:Items\r\n1\r\n\r\nHut\r\n"), false).Build());
    std::filesystem::path const noGerman = Write(directory, "NoGerman.wad", KiwadBuilder(2).Add("Locale/en-US/Items.lang", Lang("1:Items\r\n1\r\n\r\nHat\r\n"), false).Build());
    std::filesystem::path const empty = Write(directory, "Empty.wad", KiwadBuilder(2).Add("ObjectData/Hat.xml", "BINd", false).Build());

    LocaleStore store;
    std::string error;
    ASSERT_TRUE(store.Load(good, "en-US", error)) << error;
    uint64 const generation = store.GetGeneration();

    EXPECT_FALSE(store.Load(bad, "en-US", error));
    EXPECT_EQ(error, "the en-US locale cannot be loaded: none of its 1 files load; the first fails with Locale/en-US/Zones.lang: does not start with a 1:<Stem> header line");
    ASSERT_TRUE(store.GetTable("de"));
    EXPECT_FALSE(store.Load(noGerman, "en-US", error));
    EXPECT_EQ(error, "the de locale is in use, and the new data cannot replace it: there is no de locale; the install has en-US");
    EXPECT_FALSE(store.Load(empty, "en-US", error));
    EXPECT_EQ(error, "Empty.wad holds no Locale/<locale>/*.lang files");
    EXPECT_FALSE(store.Load(directory.Path() / "Missing.wad", "en-US", error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(store.Load(good, "pl", error));
    EXPECT_EQ(store.GetGeneration(), generation);
    EXPECT_EQ(store.Resolve("Items_00028316"), "Fairy Crown");

    EXPECT_FALSE(store.SetDefaultLocale("fr", error));
    EXPECT_EQ(store.GetDefaultLocale(), "en-US");
    ASSERT_TRUE(store.SetDefaultLocale("de", error)) << error;
    EXPECT_EQ(store.GetDefaultLocale(), "de");
    EXPECT_EQ(store.Resolve("Items_00028316"), "Feenhut");
    EXPECT_EQ(store.Resolve("Items_00028316", "en-US"), "Fairy Crown");
    EXPECT_GT(store.GetGeneration(), generation);

    store.Clear();
    EXPECT_FALSE(store.IsLoaded());
    EXPECT_FALSE(store.SetDefaultLocale("de", error));
    EXPECT_EQ(error, "no locale data is loaded");
}

TEST(LocaleStoreTest, ConcurrentFirstLookupsBuildALocaleOnce)
{
    LogTestDirectory directory;
    std::filesystem::path const wad = Write(directory, "Root.wad", MakeArchive().Build());
    LocaleStore store;
    std::string error;
    ASSERT_TRUE(store.Load(wad, "en-US", error)) << error;
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<LocaleTable const>> tables(8);
    std::atomic<bool> start{ false };
    for (std::size_t index = 0; index < tables.size(); ++index)
    {
        threads.emplace_back([&store, &tables, &start, index]
        {
            while (!start.load())
                std::this_thread::yield();
            tables[index] = store.GetTable("de");
        });
    }
    start = true;
    for (std::thread& thread : threads)
        thread.join();
    for (std::shared_ptr<LocaleTable const> const& table : tables)
    {
        ASSERT_TRUE(table);
        EXPECT_EQ(table.get(), tables.front().get());
    }
}
