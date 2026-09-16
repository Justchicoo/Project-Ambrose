/*
 * Project Ambrose by Imjustchico
 * Tests the name extractor on a Root.wad the test builds with its own names, .lang files, a BINd disallowed list encoded through a type dump it writes, and a creation config: every table lands in each locale with its keys and texts, schools and options come out in key order with string ids, the SQL script replaces the four tables with hex text, text split by comments or CDATA is joined, each kind of broken input is reported with file values escaped and the report capped, a script file is replaced only when written whole, and with AMBROSE_TEST_DB set the script applies to a new world database, loads in the name manager, applies again without duplicating rows, and refuses a database without the world tables while naming dbimport.
 */

#include "CharacterNameExtractor.h"
#include "CharacterNameMgr.h"
#include "BindFile.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "NameViews.h"
#include "StringHash.h"
#include "TypedView.h"
#include "Utf.h"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;
    constexpr uint32 Male = 1;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static")
    {
        bool const pointer = type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    std::string NameDump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        Json name = Json::object();
        uint32 id = 0;
        for (char const* field : { "m_locale", "m_gender", "m_first", "m_middle", "m_last" })
            name[field] = Property("unsigned int", field, id++);
        AddClass(classes, "class DisallowedName", Json::array({ "PropertyClass" }), name);
        Json list = Json::object();
        list["m_disallowedNameList"] = Property("class SharedPointer<class DisallowedName>", "m_disallowedNameList", 0, "List");
        AddClass(classes, "class DisallowedNameList", Json::array({ "PropertyClass" }), list);
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    std::vector<uint8> Lang(std::string_view stem, std::vector<std::pair<std::string, std::string>> const& entries)
    {
        std::string text = fmt::format("1:{}\r\n", stem);
        for (auto const& [key, value] : entries)
            text += fmt::format("{}\r\n\r\n{}\r\n", key, value);
        std::vector<uint8> bytes{ 0xFF, 0xFE };
        std::vector<uint8> const encoded = Utf::StringToUtf16LEBytes(*Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::Reject));
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
        return bytes;
    }

    std::string HumanXml(std::string const& locale)
    {
        return fmt::format(
            "  <Table Name=\"FirstName_HumanMale\" Locale=\"{0}\">\n    <Section>CharacterNames-{0}</Section>\n    <CharacterName>First_Boy_0</CharacterName>\n    <CharacterName>First_Boy_2</CharacterName>\n  </Table>\n"
            "  <Table Name=\"FirstName_HumanFemale\" Locale=\"{0}\">\n    <Section>CharacterNames-{0}</Section>\n    <CharacterName> First_Girl_0 </CharacterName>\n  </Table>\n"
            "  <Table Name=\"MiddleName_Human\" Locale=\"{0}\">\n    <Section>CharacterNames-{0}</Section>\n    <CharacterName></CharacterName>\n    <CharacterName>Middle_0</CharacterName>\n  </Table>\n"
            "  <Table Name=\"LastName_Human\" Locale=\"{0}\">\n    <Section>CharacterNames-{0}</Section>\n    <CharacterName/>\n    <CharacterName>Last_0</CharacterName>\n    <CharacterName>Last_1</CharacterName>\n  </Table>\n", locale);
    }

    std::string NamesXml()
    {
        return "\xEF\xBB\xBF<CharacterNameTable>\n" + HumanXml("en-US") + HumanXml("de")
            + "  <Table Name=\"FirstName\">\n    <Section>PetNames</Section>\n    <CharacterName>First_0</CharacterName>\n    <CharacterName>First_1</CharacterName>\n  </Table>\n</CharacterNameTable>\n";
    }

    std::string ConfigXml()
    {
        return "<Objects>\n  <Class Name=\"class WizCharacterCreationConfig\">\n"
            "    <m_creationOptions key=\"0\">\n      <Class Name=\"class AllowedCreationOption\">\n        <m_templateID>1</m_templateID>\n      </Class>\n    </m_creationOptions>\n"
            "    <m_schoolOptions key=\"1\">\n      <Class Name=\"class AllowedSchoolOption\">\n        <m_schoolName>Ice</m_schoolName>\n      </Class>\n    </m_schoolOptions>\n"
            "    <m_schoolOptions key=\"0\">\n      <Class Name=\"class AllowedSchoolOption\">\n        <m_schoolName> Fire </m_schoolName>\n      </Class>\n    </m_schoolOptions>\n"
            "  </Class>\n</Objects>\n";
    }

    class CharacterNameExtractorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            NameViews::RegisterAll(_views);
            _registry = std::make_unique<TypeRegistry>(&_views);
            ASSERT_TRUE(_registry->LoadFromText(NameDump(), "names.json")) << _registry->GetErrors().front();
            _catalog = _registry->GetCatalog();

            _files["CharacterNames.xml"] = Bytes(NamesXml());
            _files["Locale/en-US/CharacterNames.lang"] = Lang("CharacterNames", { { "00000421", "Pygmy Whale" }, { "First_Boy_0", "Aaron" }, { "First_Boy_1", "Retired" }, { "First_Boy_2", "Cody" },
                { "First_Girl_0", "Abby" }, { "Middle_0", "Storm" }, { "Last_0", "Blade" }, { "Last_1", "Rider" } });
            _files["Locale/de/CharacterNames.lang"] = Lang("CharacterNames", { { "First_Boy_0", "Aaron" }, { "First_Boy_2", "J\xC3\xBCrgen" }, { "First_Girl_0", "Anja" },
                { "Middle_0", "Sturm" }, { "Last_0", "Klinge" }, { "Last_1", "Reiter" } });
            _files["Locale/en-US/PetNames.lang"] = Lang("PetNames", { { "First_0", "Baby" }, { "First_1", "" } });
            _files["Locale/fr/PetNames.lang"] = Lang("PetNames", { { "First_0", "B\xC3\xA9" "b\xC3\xA9" }, { "First_1", "Petit" } });
            _files["CharacterCreation/CharacterCreationConfig.xml"] = Bytes(ConfigXml());
            _files["CharacterNamesDisallowedList.xml"] = DisallowedList({ { 1, 1, 1, 1, 2 }, { 2, 0, 0, 1, 999 } });
        }

        static std::vector<uint8> Bytes(std::string_view text)
        {
            return { text.begin(), text.end() };
        }

        std::vector<uint8> DisallowedList(std::vector<std::array<uint32, 5>> const& names)
        {
            PropertyObjectPtr list = PropertyObject::Create(_catalog, "class DisallowedNameList");
            EXPECT_TRUE(list);
            PropertyValue::List entries;
            for (std::array<uint32, 5> const& values : names)
            {
                PropertyObjectPtr name = PropertyObject::Create(_catalog, "class DisallowedName");
                std::size_t index = 0;
                for (char const* field : { "m_locale", "m_gender", "m_first", "m_middle", "m_last" })
                    EXPECT_EQ(name->Set(field, values[index++]), PropertySetResult::Ok);
                entries.emplace_back(std::move(name));
            }
            EXPECT_EQ(list->Set("m_disallowedNameList", std::move(entries)), PropertySetResult::Ok);
            EncodeResult encoded = BindFile::Write(list.get());
            EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
            return std::move(encoded.Bytes);
        }

        NameExtraction Extract()
        {
            KiwadBuilder builder(2);
            for (auto const& [name, bytes] : _files)
                builder.Add(name, bytes, name.ends_with(".lang"));
            std::filesystem::path const path = _directory.Path() / fmt::format("Root{}.wad", _archives++);
            std::vector<uint8> const archive = builder.Build();
            std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
            std::string error;
            std::unique_ptr<KiwadArchive> const opened = KiwadArchive::Open(path, error);
            EXPECT_TRUE(opened) << error;
            return opened ? CharacterNameExtractor::Extract(*opened, _catalog) : NameExtraction{};
        }

        static bool Mentions(NameExtraction const& extraction, std::string_view text)
        {
            return std::any_of(extraction.Errors.begin(), extraction.Errors.end(), [text](std::string const& error) { return error.find(text) != std::string::npos; });
        }

        static CharacterNameTable const* Find(NameExtraction const& extraction, std::string_view name, std::string_view locale)
        {
            auto const found = std::find_if(extraction.Tables.begin(), extraction.Tables.end(), [&](CharacterNameTable const& table) { return table.Name == name && table.Locale == locale; });
            return found == extraction.Tables.end() ? nullptr : &*found;
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        std::unique_ptr<TypeRegistry> _registry;
        TypeCatalogPtr _catalog;
        std::map<std::string, std::vector<uint8>> _files;
        int _archives = 0;
    };
}

TEST_F(CharacterNameExtractorTest, EveryTableSchoolAndDisallowedNameIsExtracted)
{
    NameExtraction const extraction = Extract();
    ASSERT_TRUE(extraction.Ok()) << extraction.Errors.front();
    EXPECT_EQ(extraction.Tables.size(), 10u);
    EXPECT_EQ(extraction.GetPartCount(), 20u);

    CharacterNameTable const* const male = Find(extraction, "FirstName_HumanMale", "de");
    ASSERT_TRUE(male);
    EXPECT_EQ(male->Parts, (std::vector<CharacterNamePart>{ { "First_Boy_0", "Aaron" }, { "First_Boy_2", "J\xC3\xBCrgen" } }));
    CharacterNameTable const* const female = Find(extraction, "FirstName_HumanFemale", "en-US");
    ASSERT_TRUE(female);
    EXPECT_EQ(female->Parts, (std::vector<CharacterNamePart>{ { "First_Girl_0", "Abby" } }));
    CharacterNameTable const* const last = Find(extraction, "LastName_Human", "en-US");
    ASSERT_TRUE(last);
    EXPECT_EQ(last->Parts, (std::vector<CharacterNamePart>{ { "", "" }, { "Last_0", "Blade" }, { "Last_1", "Rider" } }));
    ASSERT_TRUE(Find(extraction, "FirstName", "en-US"));
    EXPECT_EQ(Find(extraction, "FirstName", "en-US")->Parts, (std::vector<CharacterNamePart>{ { "First_0", "Baby" }, { "First_1", "" } }));
    ASSERT_TRUE(Find(extraction, "FirstName", "fr"));
    EXPECT_EQ(Find(extraction, "FirstName", "fr")->Parts[0].Text, "B\xC3\xA9" "b\xC3\xA9");
    EXPECT_FALSE(Find(extraction, "FirstName", "de"));

    EXPECT_EQ(extraction.Disallowed, (std::vector<DisallowedName>{ { 0, 1, 1, 1, 1, 2 }, { 1, 2, 0, 0, 1, 999 } }));
    EXPECT_EQ(extraction.ErrorCount, 0u);
    EXPECT_EQ(extraction.Schools, (std::vector<CreationSchool>{ { 0, "Fire", 2343174 }, { 1, "Ice", 72777 } }));
    EXPECT_EQ(extraction.Options, (std::vector<CreationOption>{ { 0, 1 } }));
}

TEST_F(CharacterNameExtractorTest, TheScriptReplacesTheFourTablesWithHexText)
{
    NameExtraction const extraction = Extract();
    ASSERT_TRUE(extraction.Ok()) << extraction.Errors.front();
    WorldSqlScript const script = CharacterNameExtractor::BuildScript(extraction);
    std::vector<std::string> const& statements = script.GetStatements();
    ASSERT_EQ(statements.size(), 8u);
    EXPECT_EQ(statements[0], "DELETE FROM `character_name_part`");
    EXPECT_TRUE(statements[1].starts_with("INSERT INTO `character_name_part` (`table_name`, `locale`, `idx`, `locale_key`, `text`) VALUES (X'"));
    EXPECT_NE(statements[1].find(fmt::format("{}, {}, 1, X'4C6173745F30', X'426C616465')", WorldSqlScript::Literal(std::string("LastName_Human")), WorldSqlScript::Literal(std::string("en-US")))), std::string::npos);
    EXPECT_NE(statements[1].find("0, '', '')"), std::string::npos);
    EXPECT_EQ(statements[4], "DELETE FROM `character_create_school`");
    EXPECT_EQ(statements[5], "INSERT INTO `character_create_school` (`school_id`, `school_name`, `sort_order`) VALUES (2343174, X'46697265', 0), (72777, X'496365', 1)");
    EXPECT_EQ(statements[7], "INSERT INTO `character_create_option` (`sort_order`, `template_id`) VALUES (0, 1)");

    EXPECT_EQ(WorldSqlScript::Literal(uint64{ 18446744073709551615u }), "18446744073709551615");
    EXPECT_EQ(WorldSqlScript::Literal(std::string()), "''");
    EXPECT_EQ(WorldSqlScript::Literal(std::string("a'\\\n\xC3\xA9")), "X'61275C0AC3A9'");

    WorldSqlScript many;
    std::vector<WorldSqlScript::Row> rows(WorldSqlScript::RowsPerStatement * 2 + 1, WorldSqlScript::Row{ uint64{ 1 } });
    many.ReplaceTable("t", { "c" }, rows);
    EXPECT_EQ(many.GetStatements().size(), 4u);

    std::string const text = script.ToText();
    EXPECT_TRUE(text.starts_with("-- Written by the Project Ambrose extractor"));
    EXPECT_NE(text.find("\nSTART TRANSACTION;\nDELETE FROM `character_name_part`;\n"), std::string::npos);
    EXPECT_TRUE(text.ends_with(";\nCOMMIT;\n"));
    std::filesystem::path const file = _directory.Path() / "names.sql";
    std::ofstream(file, std::ios::binary) << "old script";
    std::string error;
    ASSERT_TRUE(script.WriteFile(file, error)) << error;
    {
        std::ifstream stream(file, std::ios::binary);
        EXPECT_EQ(std::string(std::istreambuf_iterator<char>(stream), {}), text);
    }
    EXPECT_FALSE(std::filesystem::exists(_directory.Path() / "names.sql.partial"));
    EXPECT_FALSE(script.WriteFile(_directory.Path() / "missing" / "names.sql", error));
    EXPECT_NE(error.find("cannot create the temporary file"), std::string::npos) << error;
    std::filesystem::create_directory(_directory.Path() / "taken.sql");
    std::ofstream(_directory.Path() / "taken.sql" / "inside", std::ios::binary) << "x";
    EXPECT_FALSE(script.WriteFile(_directory.Path() / "taken.sql", error));
    EXPECT_FALSE(std::filesystem::exists(_directory.Path() / "taken.sql.partial"));
    EXPECT_EQ(CharacterNameExtractor::GetTables(), (std::vector<std::string_view>{ "character_name_part", "character_name_disallowed", "character_create_school", "character_create_option" }));
}

TEST_F(CharacterNameExtractorTest, BrokenInputsAreReported)
{
    auto const expectProblem = [this](std::string const& file, std::optional<std::vector<uint8>> bytes, std::string_view expected)
    {
        std::map<std::string, std::vector<uint8>> const saved = _files;
        if (bytes)
            _files[file] = std::move(*bytes);
        else
            _files.erase(file);
        NameExtraction const extraction = Extract();
        EXPECT_FALSE(extraction.Ok()) << file << ": expected " << expected;
        EXPECT_TRUE(Mentions(extraction, expected)) << file << ": expected '" << expected << "' in " << (extraction.Errors.empty() ? std::string("no errors") : extraction.Errors.front());
        _files = saved;
    };
    std::string const names = NamesXml();
    auto const replaced = [](std::string text, std::string_view from, std::string_view to)
    {
        std::size_t const at = text.find(from);
        EXPECT_NE(at, std::string::npos) << from;
        return Bytes(text.replace(at, from.size(), to));
    };

    expectProblem("CharacterNames.xml", std::nullopt, "CharacterNames.xml: ");
    expectProblem("CharacterNames.xml", Bytes("<CharacterNameTable><Table>"), "CharacterNames.xml is not well-formed XML");
    expectProblem("CharacterNames.xml", Bytes("<Names/>"), "the root element is <Names>, not <CharacterNameTable>");
    expectProblem("CharacterNames.xml", Bytes(names + "<CharacterNameTable/>"), "CharacterNames.xml must hold exactly one root element and no text outside it");
    expectProblem("CharacterNames.xml", Bytes(names + "trailing"), "CharacterNames.xml must hold exactly one root element and no text outside it");
    expectProblem("CharacterNames.xml", Bytes(""), "CharacterNames.xml must hold exactly one root element and no text outside it");
    expectProblem("CharacterNames.xml", replaced(names, "<CharacterName>First_Boy_2</CharacterName>", "<CharacterName>First_Boy_2<Bogus/></CharacterName>"), "CharacterNames.xml: <CharacterName> holds an element <Bogus> where only text belongs");
    expectProblem("CharacterNames.xml", replaced(names, "<CharacterName>First_Boy_2</CharacterName>", "<CharacterName>First_<!-- note -->Boy_9</CharacterName>"), "has no key First_Boy_9");
    expectProblem("CharacterNames.xml", replaced(names, "<CharacterName>Middle_0</CharacterName>", "<Bogus/>"), "table MiddleName_Human holds an unknown element <Bogus>");
    expectProblem("CharacterNames.xml", replaced(names, "First_Boy_2", "First_Boy_9"), "table FirstName_HumanMale (en-US) position 1: Locale/en-US/CharacterNames.lang has no key First_Boy_9");
    expectProblem("CharacterNames.xml", replaced(names, "<Section>PetNames</Section>", "<Section>MountNames</Section>"), "table FirstName uses section MountNames, but no locale holds MountNames.lang");
    expectProblem("CharacterNames.xml", replaced(names, "<Section>PetNames</Section>", ""), "table FirstName needs exactly one non-empty <Section>, not 0");
    expectProblem("CharacterNames.xml", replaced(names, "<CharacterName></CharacterName>", "<CharacterName>Middle_0</CharacterName>"), "MiddleName_Human (en-US) position 0 must be empty");
    expectProblem("CharacterNames.xml", replaced(names, "Locale=\"de\">\n    <Section>CharacterNames-de</Section>\n    <CharacterName>First_Boy_0", "Locale=\"pl\">\n    <Section>CharacterNames-pl</Section>\n    <CharacterName>First_Boy_0"), "Locale/pl/CharacterNames.lang: ");
    expectProblem("Locale/de/CharacterNames.lang", Lang("Names", { { "First_Boy_0", "Aaron" } }), "Locale/de/CharacterNames.lang: its header names the stem Names, not CharacterNames");
    expectProblem("Locale/de/CharacterNames.lang", Bytes("not utf-16"), "Locale/de/CharacterNames.lang: does not start with a UTF-16LE byte order mark");
    expectProblem("CharacterNamesDisallowedList.xml", std::nullopt, "CharacterNamesDisallowedList.xml: ");
    expectProblem("CharacterNamesDisallowedList.xml", Bytes("<xml/>"), "CharacterNamesDisallowedList.xml: ");
    expectProblem("CharacterNamesDisallowedList.xml", DisallowedList({ { 1, 3, 0, 0, 0 } }), "disallowed name 0 has gender 3");
    std::string const config = ConfigXml();
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", std::nullopt, "CharacterCreation/CharacterCreationConfig.xml: ");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "class WizCharacterCreationConfig", "class Other"), "must be <Objects> holding one <Class Name=\"class WizCharacterCreationConfig\">");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "<m_creationOptions key=\"0\">\n      <Class Name=\"class AllowedCreationOption\">\n        <m_templateID>1</m_templateID>\n      </Class>\n    </m_creationOptions>", "<m_extra/>"), "holds an unknown property <m_extra>");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "key=\"1\"", "key=\"0\""), "<m_schoolOptions> key 0 is used more than once");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "key=\"1\"", "key=\"one\""), "<m_schoolOptions> needs a key attribute of 0-65535, not 'one'");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "<m_templateID>1</m_templateID>", "<m_templateID>first</m_templateID>"), "has the template id 'first', which is not a number");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "<m_schoolName>Ice</m_schoolName>", "<m_schoolName>Fire</m_schoolName>"), "the school Fire is listed twice");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "<m_schoolName>Ice</m_schoolName>", "<m_schoolName>Ice Cold</m_schoolName>"), "has the school name 'Ice Cold'");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "<m_schoolName>Ice</m_schoolName>", "<m_schoolName>I&#x1B;[31mce</m_schoolName>"), "has the school name 'I\\x1B[31mce'");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "<m_schoolName>Ice</m_schoolName>", "<m_schoolName>Fi<!-- split -->re</m_schoolName>"), "the school Fire is listed twice");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", replaced(config, "class AllowedSchoolOption\">\n        <m_schoolName>Ice", "class AllowedCreationOption\">\n        <m_schoolName>Ice"), "must hold exactly one <Class Name=\"class AllowedSchoolOption\">");
    expectProblem("CharacterCreation/CharacterCreationConfig.xml", Bytes("<Objects>\n  <Class Name=\"class WizCharacterCreationConfig\"/>\n</Objects>\n"), "CharacterCreation/CharacterCreationConfig.xml offers no school");
    expectProblem("CharacterNames.xml", Bytes("<CharacterNameTable/>"), "CharacterNames.xml holds no locale with all four human name tables");

    std::vector<uint8> const joined = replaced(names, "<CharacterName>First_Boy_2</CharacterName>", "<CharacterName>First_<![CDATA[Boy_]]><!-- joined -->2</CharacterName>");
    _files["CharacterNames.xml"] = joined;
    NameExtraction const split = Extract();
    ASSERT_TRUE(split.Ok()) << split.Errors.front();
    ASSERT_TRUE(Find(split, "FirstName_HumanMale", "en-US"));
    EXPECT_EQ(Find(split, "FirstName_HumanMale", "en-US")->Parts[1], (CharacterNamePart{ "First_Boy_2", "Cody" }));

    std::string noisy = "<CharacterNameTable>\n  <Table Name=\"Noise\" Locale=\"en-US\">\n";
    for (int index = 0; index < 150; ++index)
        noisy += "    <Section>CharacterNames-en-US</Section>\n    <Bogus/>\n";
    noisy += "  </Table>\n</CharacterNameTable>\n";
    _files["CharacterNames.xml"] = Bytes(noisy);
    NameExtraction const capped = Extract();
    EXPECT_EQ(capped.ErrorCount, 150u);
    ASSERT_EQ(capped.Errors.size(), NameExtraction::MaxReportedErrors + 1);
    EXPECT_EQ(capped.Errors.back(), "and 50 more problems");
}

TEST_F(CharacterNameExtractorTest, TheScriptAppliesToAWorldDatabaseAndLoadsInTheManager)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> server = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(server);
    std::string const suffix = fmt::format("{:08x}", std::random_device()());
    MySQLConnectionInfo world = *server;
    world.Database = "ambrose_extract_world_" + suffix;
    MySQLConnectionInfo bare = *server;
    bare.Database = "ambrose_extract_bare_" + suffix;
    server->Database.clear();
    struct Cleanup
    {
        MySQLConnectionInfo Server;
        std::vector<std::string> Names;
        ~Cleanup()
        {
            WorldDatabase.Close();
            MySQLConnection connection(Server);
            if (connection.Open() == 0)
                for (std::string const& name : Names)
                    connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(name)));
        }
    } const cleanup{ *server, { world.Database, bare.Database } };

    NameExtraction const extraction = Extract();
    ASSERT_TRUE(extraction.Ok()) << extraction.Errors.front();
    WorldSqlScript const script = CharacterNameExtractor::BuildScript(extraction);

    MySQLConnection creator(*server);
    ASSERT_EQ(creator.Open(), 0u) << creator.GetLastErrorText();
    ASSERT_TRUE(creator.Execute(fmt::format("CREATE DATABASE {}", DBUpdater::QuoteIdentifier(bare.Database))));
    std::string error;
    EXPECT_FALSE(script.Apply(bare, error));
    EXPECT_NE(error.find("nothing was changed"), std::string::npos) << error;
    EXPECT_NE(error.find("run dbimport to create the world tables first"), std::string::npos) << error;

    ASSERT_TRUE(DBUpdater::Run(world, "world", UpdaterSettings{}));
    for (int round = 0; round < 2; ++round)
    {
        ASSERT_TRUE(script.Apply(world, error)) << error;
        ASSERT_TRUE(WorldDatabase.SetConnectionInfo(world.ToConnectionString(), 1, 1));
        ASSERT_EQ(WorldDatabase.Open(), 0u);
        QueryResult const counts = WorldDatabase.Query("SELECT (SELECT COUNT(*) FROM `character_name_part`), (SELECT COUNT(*) FROM `character_name_disallowed`), (SELECT COUNT(*) FROM `character_create_school`), (SELECT COUNT(*) FROM `character_create_option`)");
        ASSERT_TRUE(counts);
        EXPECT_EQ((*counts)[0].Get<uint64>(), 20u);
        EXPECT_EQ((*counts)[1].Get<uint64>(), 2u);
        EXPECT_EQ((*counts)[2].Get<uint64>(), 2u);
        EXPECT_EQ((*counts)[3].Get<uint64>(), 1u);

        CharacterNameMgr manager;
        CharacterNameLoadResult const loaded = manager.Load();
        ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
        EXPECT_EQ(loaded.HumanLocales, 2u);
        EXPECT_EQ(manager.FormatName(NameIndices{ 1, 1, 2 }.Pack(), Male, "de"), "J\xC3\xBC" "rgen SturmReiter");
        EXPECT_EQ(manager.FormatName(NameIndices{ 0, 0, 1 }.Pack(), Male), "Aaron Blade");
        EXPECT_EQ(manager.Check(NameIndices{ 1, 1, 2 }.Pack(), Male, "en-US"), NameCheck::Disallowed);
        EXPECT_EQ(*manager.GetNames()->FindTable("FirstName", "fr"), *Find(extraction, "FirstName", "fr"));
        WorldDatabase.Close();
    }
}
