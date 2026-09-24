/*
 * Project Ambrose by Imjustchico
 * Extracts the character names of the user's own r806919 install, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it: every locale with CharacterNames has all four human tables, 250 names each in en-US, de, es and fr and fewer in el, it and pl, the seven schools in their order with their string ids, the four disallowed names and one creation option; the first disallowed name formats in English and German and is refused; and with AMBROSE_TEST_DB set the rows fill a new world database that the name manager loads.
 */

#include "CharacterNameExtractor.h"
#include "CharacterNameScript.h"
#include "CharacterNameMgr.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "NameViews.h"
#include "StringHash.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr uint32 Male = 1;

    class CharacterNameExtractorClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            std::string error;
            std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad", error);
            ASSERT_TRUE(archive) << error;
            s_views = std::make_unique<TypedViewRegistry>();
            NameViews::RegisterAll(*s_views);
            s_registry = std::make_unique<TypeRegistry>(s_views.get());
            ASSERT_TRUE(s_registry->LoadFromFile(LogConfig::Utf8Path(*dump)));
            s_extraction = std::make_unique<NameExtraction>(CharacterNameExtractor::Extract(*archive, s_registry->GetCatalog()));
        }

        static void TearDownTestSuite()
        {
            s_extraction.reset();
            s_registry.reset();
            s_views.reset();
        }

        void SetUp() override
        {
            if (!s_extraction)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            ASSERT_TRUE(s_extraction->Ok()) << s_extraction->Errors.front();
        }

        static CharacterNameTable const* Find(std::string_view name, std::string_view locale)
        {
            for (CharacterNameTable const& table : s_extraction->Tables)
                if (table.Name == name && table.Locale == locale)
                    return &table;
            return nullptr;
        }

        static inline std::unique_ptr<TypedViewRegistry> s_views;
        static inline std::unique_ptr<TypeRegistry> s_registry;
        static inline std::unique_ptr<NameExtraction> s_extraction;
    };
}

TEST_F(CharacterNameExtractorClientTest, EveryLocaleHasItsHumanTablesSchoolsAndDisallowedNames)
{
    std::vector<std::string> errors;
    std::shared_ptr<CharacterNameSet const> const names = CharacterNameSet::Build(s_extraction->Tables, s_extraction->Disallowed, errors);
    ASSERT_TRUE(names) << errors.front();
    EXPECT_EQ(names->GetHumanLocales(), (std::vector<std::string>{ "de", "el", "en-US", "es", "fr", "it", "pl" }));
    std::cout << fmt::format("[ NAMES    ] {} tables holding {} names", s_extraction->Tables.size(), s_extraction->GetPartCount()) << std::endl;
    for (std::string_view const locale : { "de", "en-US", "es", "fr" })
        for (std::string_view const table : { CharacterNameSet::FirstNameMale, CharacterNameSet::FirstNameFemale, CharacterNameSet::MiddleName, CharacterNameSet::LastName })
        {
            ASSERT_TRUE(Find(table, locale)) << table << " " << locale;
            EXPECT_EQ(Find(table, locale)->Parts.size(), 250u) << table << " " << locale;
        }
    for (std::string_view const locale : { "el", "it", "pl" })
    {
        ASSERT_TRUE(Find(CharacterNameSet::FirstNameMale, locale));
        EXPECT_EQ(Find(CharacterNameSet::FirstNameMale, locale)->Parts.size(), 154u) << locale;
        EXPECT_EQ(Find(CharacterNameSet::FirstNameFemale, locale)->Parts.size(), 144u) << locale;
        EXPECT_EQ(Find(CharacterNameSet::MiddleName, locale)->Parts.size(), 85u) << locale;
        EXPECT_EQ(Find(CharacterNameSet::LastName, locale)->Parts.size(), 79u) << locale;
    }
    EXPECT_TRUE(Find("FirstName", "en-US"));
    EXPECT_TRUE(Find("FirstName_AdventureParty", "pl"));

    std::vector<std::string> schools;
    for (CreationSchool const& school : s_extraction->Schools)
    {
        schools.push_back(school.Name);
        EXPECT_EQ(school.Id, StringHash::StringId(school.Name)) << school.Name;
    }
    EXPECT_EQ(schools, (std::vector<std::string>{ "Fire", "Ice", "Storm", "Life", "Myth", "Death", "Balance" }));
    EXPECT_EQ(s_extraction->Schools.back().Id, 1027491821u);
    EXPECT_EQ(s_extraction->Disallowed.size(), 4u);
    EXPECT_EQ(s_extraction->Options, (std::vector<CreationOption>{ { 0, 1 } }));

    DisallowedName const& first = s_extraction->Disallowed.front();
    uint32 const packed = NameIndices{ static_cast<uint8>(first.First), static_cast<uint8>(first.Middle), static_cast<uint8>(first.Last) }.Pack();
    EXPECT_EQ(names->FormatName(packed, first.Gender, "en-US"), "Jeffrey IslandTouch");
    EXPECT_EQ(names->FormatName(packed, first.Gender, "de"), "Linus InselBer\xC3\xBChrung");
    EXPECT_EQ(names->Check(packed, first.Gender, "en-US"), NameCheck::Disallowed);
    EXPECT_EQ(names->Check(packed, first.Gender, "it"), NameCheck::MiddleOutOfRange);
}

TEST_F(CharacterNameExtractorClientTest, TheRowsFillAWorldDatabaseTheManagerLoads)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(info);
    info->Database = fmt::format("ambrose_client_names_{:08x}", std::random_device()());
    struct Cleanup
    {
        MySQLConnectionInfo Info;
        ~Cleanup()
        {
            WorldDatabase.Close();
            MySQLConnectionInfo server = Info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(Info.Database)));
        }
    } const cleanup{ *info };

    ASSERT_TRUE(DBUpdater::Run(*info, "world", UpdaterSettings{}));
    std::string error;
    ASSERT_TRUE(CharacterNameScript::Build(*s_extraction).Apply(*info, error)) << error;
    ASSERT_TRUE(WorldDatabase.SetConnectionInfo(info->ToConnectionString(), 1, 1));
    ASSERT_EQ(WorldDatabase.Open(), 0u);

    QueryResult tables = WorldDatabase.Query("SELECT `table_name`, COUNT(*) FROM `character_name_part` WHERE `locale` = 'en-US' AND `table_name` IN ('FirstName_HumanMale', 'FirstName_HumanFemale', 'MiddleName_Human', 'LastName_Human') GROUP BY `table_name`");
    ASSERT_TRUE(tables);
    EXPECT_EQ(tables->GetRowCount(), 4u);
    do
        EXPECT_GT((*tables)[1].Get<uint64>(), 0u) << (*tables)[0].Get<std::string>();
    while (tables->NextRow());
    QueryResult const schools = WorldDatabase.Query("SELECT COUNT(*) FROM `character_create_school`");
    ASSERT_TRUE(schools);
    EXPECT_EQ((*schools)[0].Get<uint64>(), 7u);

    CharacterNameMgr manager;
    CharacterNameLoadResult const loaded = manager.Load();
    ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
    EXPECT_EQ(loaded.Parts, s_extraction->GetPartCount());
    EXPECT_EQ(loaded.HumanLocales, 7u);
    EXPECT_EQ(manager.FormatName(NameIndices{ 77, 158, 230 }.Pack(), Male), "Jeffrey IslandTouch");
}
