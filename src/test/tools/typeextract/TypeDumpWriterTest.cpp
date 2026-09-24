/*
 * Project Ambrose by Imjustchico
 * Tests the type dump writer on hand-built dump models: JSON that the loader reads back field for field, metadata first and classes sorted by name then key, absent fields and empty options left out, duplicates and option value types kept, invalid UTF-8 replaced instead of thrown; saves that create folders, overwrite, write through a temporary file named for the process and a random number, leave other runs' temporary files alone, succeed when many run at once on one target, clean up their own temporary file and name the path when it cannot be written; and comparisons that report each kind of difference exactly once, in name order, with nothing for equal dumps.
 */

#include "TypeDumpWriter.h"
#include "ConfigMgr.h"
#include "StringHash.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <regex>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

namespace
{
    using OrderedJson = nlohmann::ordered_json;
    using RawClass = TypeDumpLoader::RawClass;
    using RawDump = TypeDumpLoader::RawDump;
    using RawProperty = TypeDumpLoader::RawProperty;
    using OptionValue = std::variant<int64, std::string>;

    TypeDumpMetadata const Metadata{ "r806919", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "typeextract 1" };

    RawProperty MakeProperty(std::string const& type, std::string const& name, uint64 id)
    {
        RawProperty property;
        property.Name = name;
        property.Type = type;
        property.Id = id;
        property.Offset = 8 * (id + 1);
        property.Flags = 31;
        property.Container = "Static";
        property.Dynamic = false;
        property.Singleton = false;
        property.Pointer = false;
        property.Hash = StringHash::PropertyHash(type, name);
        return property;
    }

    RawClass MakeClass(std::string const& name, std::vector<std::string> bases, std::vector<RawProperty> properties)
    {
        RawClass rawClass;
        rawClass.Key = std::to_string(StringHash::KiStringHash(name));
        rawClass.Name = name;
        rawClass.Hash = StringHash::KiStringHash(name);
        rawClass.Bases = std::move(bases);
        rawClass.Properties = std::move(properties);
        return rawClass;
    }

    RawDump MakeDump(std::vector<RawClass> classes)
    {
        RawDump dump;
        dump.Version = 2;
        dump.HasClasses = true;
        dump.Classes = std::move(classes);
        return dump;
    }

    RawDump SampleDump()
    {
        RawProperty mood = MakeProperty("enum TestMood", "m_mood", 2);
        mood.Flags = 2097183;
        mood.Options = {
            { "kCalm", OptionValue{ int64{ 0 } } },
            { "kNegative", OptionValue{ int64{ -2 } } },
            { "kLargest", OptionValue{ std::numeric_limits<int64>::max() } },
            { "kSmallest", OptionValue{ std::numeric_limits<int64>::min() } },
            { "__DEFAULT", OptionValue{ std::string("kCalm") } },
            { "kDigits", OptionValue{ std::string("5") } },
            { "kEmpty", OptionValue{ std::string() } }
        };
        RawProperty children = MakeProperty("class SharedPointer<class TestBase>", "m_children", 1);
        children.Container = "Vector";
        children.Dynamic = true;
        children.Pointer = true;
        children.Offset = std::numeric_limits<uint64>::max();
        RawProperty single = MakeProperty("unsigned __int64", "m_zeta", 0);
        single.Singleton = true;
        single.Flags = std::numeric_limits<uint64>::max();
        return MakeDump({
            MakeClass("class TestDerived", { "TestBase", "PropertyClass" }, { single, children, mood }),
            MakeClass("class PropertyClass", {}, {}),
            MakeClass("class TestBase", { "PropertyClass" }, { MakeProperty("std::string", "m_name", 0) })
        });
    }

    RawDump ParseBack(std::string const& text)
    {
        RawDump dump;
        std::vector<std::string> errors;
        EXPECT_TRUE(TypeDumpLoader::Parse(text, dump, errors)) << (errors.empty() ? std::string() : errors.front());
        return dump;
    }

    void ExpectSameProperty(RawProperty const& expected, RawProperty const& actual)
    {
        EXPECT_EQ(actual.Name, expected.Name);
        EXPECT_EQ(actual.Type, expected.Type) << expected.Name;
        EXPECT_EQ(actual.Container, expected.Container) << expected.Name;
        EXPECT_EQ(actual.Id, expected.Id) << expected.Name;
        EXPECT_EQ(actual.Offset, expected.Offset) << expected.Name;
        EXPECT_EQ(actual.Flags, expected.Flags) << expected.Name;
        EXPECT_EQ(actual.Hash, expected.Hash) << expected.Name;
        EXPECT_EQ(actual.Dynamic, expected.Dynamic) << expected.Name;
        EXPECT_EQ(actual.Singleton, expected.Singleton) << expected.Name;
        EXPECT_EQ(actual.Pointer, expected.Pointer) << expected.Name;
        EXPECT_EQ(actual.Options, expected.Options) << expected.Name;
    }

    void ExpectSameClass(RawClass const& expected, RawClass const& actual)
    {
        EXPECT_EQ(actual.Key, expected.Key);
        EXPECT_EQ(actual.Name, expected.Name);
        EXPECT_EQ(actual.Hash, expected.Hash);
        EXPECT_EQ(actual.Bases, expected.Bases);
        ASSERT_EQ(actual.Properties.size(), expected.Properties.size()) << expected.Key;
        for (std::size_t index = 0; index < expected.Properties.size(); ++index)
            ExpectSameProperty(expected.Properties[index], actual.Properties[index]);
    }

    std::vector<std::string> Keys(OrderedJson const& object)
    {
        std::vector<std::string> keys;
        for (auto const& item : object.items())
            keys.push_back(item.key());
        return keys;
    }

    std::vector<std::array<std::string, 5>> Rows(std::vector<TypeDumpDifference> const& differences)
    {
        std::vector<std::array<std::string, 5>> rows;
        for (TypeDumpDifference const& difference : differences)
            rows.push_back({ difference.Class, difference.Property, difference.Field, difference.Ours, difference.Theirs });
        return rows;
    }

    std::string ReadFile(std::filesystem::path const& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> PartialFiles(std::filesystem::path const& directory)
    {
        std::vector<std::string> names;
        for (std::filesystem::recursive_directory_iterator it(directory), end; it != end; ++it)
            if (it->path().extension() == ".partial")
                names.push_back(ConfigMgr::PathToUtf8(it->path().filename()));
        std::sort(names.begin(), names.end());
        return names;
    }

    class TypeDumpWriterTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::random_device device;
            _directory = std::filesystem::path(testing::TempDir()) / ("ambrose-typedumpwriter-" + std::to_string(device()) + "-" + std::to_string(device()));
            std::filesystem::create_directories(_directory);
        }

        void TearDown() override
        {
            std::error_code error;
            std::filesystem::remove_all(_directory, error);
        }

        std::filesystem::path _directory;
    };
}

TEST_F(TypeDumpWriterTest, JsonReadsBackFieldForField)
{
    RawDump const dump = SampleDump();
    std::string const text = TypeDumpWriter::ToJson(dump, Metadata);
    RawDump const parsed = ParseBack(text);
    EXPECT_EQ(parsed.Version, 2);
    EXPECT_TRUE(parsed.HasClasses);
    ASSERT_EQ(parsed.Classes.size(), 3u);
    ExpectSameClass(dump.Classes[1], parsed.Classes[0]);
    ExpectSameClass(dump.Classes[2], parsed.Classes[1]);
    ExpectSameClass(dump.Classes[0], parsed.Classes[2]);
    EXPECT_TRUE(TypeDumpWriter::Compare(dump, parsed).empty());
    EXPECT_EQ(TypeDumpWriter::ToJson(parsed, Metadata), text);
}

TEST_F(TypeDumpWriterTest, JsonPutsMetadataFirstAndSortsClasses)
{
    RawDump dump = SampleDump();
    std::string const baseKey = std::to_string(StringHash::KiStringHash("class TestBase"));
    dump.Classes[2].Key = "8";
    RawClass sameNameLaterKey = MakeClass("class TestBase", {}, {});
    sameNameLaterKey.Key = "9";
    RawClass sameNameEarlierKey = MakeClass("class TestBase", {}, {});
    sameNameEarlierKey.Key = "10";
    dump.Classes.push_back(sameNameLaterKey);
    dump.Classes.push_back(sameNameEarlierKey);
    std::string const text = TypeDumpWriter::ToJson(dump, Metadata);

    EXPECT_TRUE(text.starts_with("{\n \"version\": 2,\n \"revision\": \"r806919\",\n \"executable_sha256\": \"0123456789abcdef")) << text.substr(0, 120);
    EXPECT_TRUE(text.ends_with("\n}\n"));

    OrderedJson const root = OrderedJson::parse(text);
    EXPECT_EQ(Keys(root), (std::vector<std::string>{ "version", "revision", "executable_sha256", "extractor", "classes" }));
    EXPECT_TRUE(root.at("version").is_number_integer());
    EXPECT_EQ(root.at("version").get<int64>(), 2);
    EXPECT_EQ(root.at("revision"), Metadata.Revision);
    EXPECT_EQ(root.at("executable_sha256"), Metadata.ExecutableSha256);
    EXPECT_EQ(root.at("extractor"), Metadata.Extractor);

    OrderedJson const classes = root.at("classes");
    std::vector<std::string> names;
    for (auto const& item : classes.items())
        names.push_back(item.value().at("name").get<std::string>());
    EXPECT_EQ(names, (std::vector<std::string>{ "class PropertyClass", "class TestBase", "class TestBase", "class TestBase", "class TestDerived" }));
    EXPECT_EQ(Keys(classes), (std::vector<std::string>{ std::to_string(StringHash::KiStringHash("class PropertyClass")), "10", "8", "9", std::to_string(StringHash::KiStringHash("class TestDerived")) }));
    EXPECT_EQ(classes.at("8").at("properties").size(), 1u);
    EXPECT_FALSE(classes.contains(baseKey));

    OrderedJson const derived = classes.at(std::to_string(StringHash::KiStringHash("class TestDerived")));
    EXPECT_EQ(Keys(derived), (std::vector<std::string>{ "name", "bases", "hash", "properties" }));
    EXPECT_EQ(derived.at("bases"), OrderedJson::array({ "TestBase", "PropertyClass" }));
    EXPECT_EQ(derived.at("hash").get<uint64>(), StringHash::KiStringHash("class TestDerived"));
    OrderedJson const properties = derived.at("properties");
    EXPECT_EQ(Keys(properties), (std::vector<std::string>{ "m_zeta", "m_children", "m_mood" }));
    EXPECT_EQ(Keys(properties.at("m_zeta")), (std::vector<std::string>{ "type", "id", "offset", "flags", "container", "dynamic", "singleton", "pointer", "hash" }));
    EXPECT_EQ(Keys(properties.at("m_mood")), (std::vector<std::string>{ "type", "id", "offset", "flags", "container", "dynamic", "singleton", "pointer", "hash", "enum_options" }));
    EXPECT_EQ(properties.at("m_zeta").at("flags").get<uint64>(), std::numeric_limits<uint64>::max());
    EXPECT_TRUE(properties.at("m_zeta").at("singleton").get<bool>());
    EXPECT_TRUE(properties.at("m_children").at("dynamic").is_boolean());
    EXPECT_EQ(properties.at("m_children").at("container"), "Vector");

    OrderedJson const options = properties.at("m_mood").at("enum_options");
    EXPECT_EQ(Keys(options), (std::vector<std::string>{ "kCalm", "kNegative", "kLargest", "kSmallest", "__DEFAULT", "kDigits", "kEmpty" }));
    EXPECT_TRUE(options.at("kCalm").is_number_integer());
    EXPECT_EQ(options.at("kNegative").get<int64>(), -2);
    EXPECT_EQ(options.at("kLargest").get<int64>(), std::numeric_limits<int64>::max());
    EXPECT_EQ(options.at("kSmallest").get<int64>(), std::numeric_limits<int64>::min());
    EXPECT_TRUE(options.at("kDigits").is_string());
    EXPECT_EQ(options.at("kDigits"), "5");
    EXPECT_EQ(options.at("kEmpty"), "");
    EXPECT_NE(text.find("\"kNegative\": -2,"), std::string::npos);
    EXPECT_NE(text.find("\"kDigits\": \"5\","), std::string::npos);
    EXPECT_NE(text.find("\"kEmpty\": \"\"\n"), std::string::npos);

    RawDump const parsed = ParseBack(text);
    ASSERT_EQ(parsed.Classes.size(), 5u);
    RawProperty const& mood = parsed.Classes[4].Properties[2];
    ASSERT_EQ(mood.Options.size(), 7u);
    EXPECT_TRUE(std::holds_alternative<int64>(mood.Options[0].second));
    EXPECT_EQ(std::get<int64>(mood.Options[1].second), -2);
    EXPECT_EQ(std::get<int64>(mood.Options[3].second), std::numeric_limits<int64>::min());
    EXPECT_TRUE(std::holds_alternative<std::string>(mood.Options[4].second));
    EXPECT_EQ(std::get<std::string>(mood.Options[5].second), "5");
    EXPECT_EQ(std::get<std::string>(mood.Options[6].second), "");
}

TEST_F(TypeDumpWriterTest, JsonOmitsAbsentFieldsAndEmptyOptions)
{
    RawClass unnamed;
    unnamed.Key = "77";
    RawProperty bare;
    bare.Name = "m_bare";
    unnamed.Properties.push_back(bare);
    RawProperty partial;
    partial.Name = "m_partial";
    partial.Id = 0;
    partial.Pointer = false;
    unnamed.Properties.push_back(partial);
    RawDump const dump = MakeDump({ MakeClass("class PropertyClass", {}, {}), unnamed });

    std::string const text = TypeDumpWriter::ToJson(dump, TypeDumpMetadata{});
    OrderedJson const root = OrderedJson::parse(text);
    EXPECT_EQ(Keys(root), (std::vector<std::string>{ "version", "revision", "executable_sha256", "extractor", "classes" }));
    EXPECT_EQ(root.at("revision"), "");
    std::string const propertyClassKey = std::to_string(StringHash::KiStringHash("class PropertyClass"));
    EXPECT_EQ(Keys(root.at("classes")), (std::vector<std::string>{ "77", propertyClassKey }));
    OrderedJson const written = root.at("classes").at("77");
    EXPECT_EQ(Keys(written), (std::vector<std::string>{ "bases", "properties" }));
    EXPECT_TRUE(written.at("bases").is_array());
    EXPECT_TRUE(written.at("bases").empty());
    EXPECT_TRUE(written.at("properties").at("m_bare").is_object());
    EXPECT_TRUE(written.at("properties").at("m_bare").empty());
    EXPECT_EQ(Keys(written.at("properties").at("m_partial")), (std::vector<std::string>{ "id", "pointer" }));
    OrderedJson const propertyClass = root.at("classes").at(propertyClassKey);
    EXPECT_TRUE(propertyClass.at("properties").is_object());
    EXPECT_TRUE(propertyClass.at("properties").empty());
    EXPECT_EQ(text.find("enum_options"), std::string::npos);

    RawDump const parsed = ParseBack(text);
    ASSERT_EQ(parsed.Classes.size(), 2u);
    ExpectSameClass(unnamed, parsed.Classes[0]);
    EXPECT_FALSE(parsed.Classes[0].Name.has_value());
    EXPECT_FALSE(parsed.Classes[0].Hash.has_value());
    EXPECT_FALSE(parsed.Classes[0].Properties[0].Type.has_value());
    EXPECT_FALSE(parsed.Classes[0].Properties[1].Dynamic.has_value());
    EXPECT_EQ(parsed.Classes[0].Properties[1].Pointer, std::optional<bool>{ false });

    std::string const empty = TypeDumpWriter::ToJson(RawDump{}, Metadata);
    EXPECT_TRUE(empty.ends_with(" \"classes\": {}\n}\n")) << empty;
    RawDump const parsedEmpty = ParseBack(empty);
    EXPECT_TRUE(parsedEmpty.HasClasses);
    EXPECT_TRUE(parsedEmpty.Classes.empty());
}

TEST_F(TypeDumpWriterTest, JsonKeepsDuplicatesAndReplacesInvalidText)
{
    RawClass first = MakeClass("class TestTwin", {}, { MakeProperty("int", "m_value", 0), MakeProperty("float", "m_value", 1) });
    first.Properties[0].Options = { { "kSame", OptionValue{ int64{ 1 } } }, { "kSame", OptionValue{ std::string("1") } } };
    RawClass second = MakeClass("class TestTwin", { "PropertyClass" }, {});
    RawDump const dump = MakeDump({ first, second });
    std::string const text = TypeDumpWriter::ToJson(dump, Metadata);
    RawDump const parsed = ParseBack(text);
    ASSERT_EQ(parsed.Classes.size(), 2u);
    EXPECT_EQ(parsed.Classes[0].Key, parsed.Classes[1].Key);
    ExpectSameClass(first, parsed.Classes[0]);
    ExpectSameClass(second, parsed.Classes[1]);

    RawDump invalid = MakeDump({ MakeClass("class Bad", {}, { MakeProperty("int", "m_bad", 0) }) });
    invalid.Classes[0].Name = std::string("class Bad") + "\xFF" + "Name";
    invalid.Classes[0].Properties[0].Options = { { std::string("k") + "\xC3", OptionValue{ std::string("\xE2\x82") } } };
    std::string written;
    EXPECT_NO_THROW(written = TypeDumpWriter::ToJson(invalid, TypeDumpMetadata{ "r1", "sha", std::string("tool") + "\xFE" }));
    RawDump const replaced = ParseBack(written);
    ASSERT_EQ(replaced.Classes.size(), 1u);
    EXPECT_EQ(replaced.Classes[0].Name, std::string("class Bad") + "\xEF\xBF\xBD" + "Name");
    ASSERT_EQ(replaced.Classes[0].Properties[0].Options.size(), 1u);
    EXPECT_EQ(replaced.Classes[0].Properties[0].Options[0].first, std::string("k") + "\xEF\xBF\xBD");
    EXPECT_NE(written.find(std::string("\"extractor\": \"tool") + "\xEF\xBF\xBD" + "\""), std::string::npos);
}

TEST_F(TypeDumpWriterTest, SaveWritesOverwritesAndLeavesNoPartialFile)
{
    std::filesystem::path const target = _directory / "types" / "nested" / "r806919.json";
    std::string error;
    std::string const text = TypeDumpWriter::ToJson(SampleDump(), Metadata);
    ASSERT_TRUE(TypeDumpWriter::Save(target, text, error)) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(ReadFile(target), text);
    EXPECT_TRUE(PartialFiles(_directory).empty());

    std::filesystem::path oldRun = target;
    oldRun += ".partial";
    std::filesystem::path otherRun = target;
    otherRun += ".4242.00000000deadbeef.partial";
    std::ofstream(oldRun, std::ios::binary) << "left by a crashed run";
    std::ofstream(otherRun, std::ios::binary) << "still being written by another run";
    ASSERT_TRUE(TypeDumpWriter::Save(target, "{}\n", error)) << error;
    EXPECT_EQ(ReadFile(target), "{}\n");
    EXPECT_EQ(ReadFile(oldRun), "left by a crashed run");
    EXPECT_EQ(ReadFile(otherRun), "still being written by another run");
    EXPECT_EQ(PartialFiles(_directory), (std::vector<std::string>{ "r806919.json.4242.00000000deadbeef.partial", "r806919.json.partial" }));
    std::filesystem::remove(oldRun);
    std::filesystem::remove(otherRun);

    std::string binary("a\r\n\0b\n", 6);
    ASSERT_TRUE(TypeDumpWriter::Save(target, binary, error)) << error;
    EXPECT_EQ(ReadFile(target), binary);

    std::string const large(3u << 20, 'x');
    ASSERT_TRUE(TypeDumpWriter::Save(target, large, error)) << error;
    EXPECT_EQ(ReadFile(target), large);

    std::filesystem::path const plain = _directory / "plain.json";
    ASSERT_TRUE(TypeDumpWriter::Save(plain, std::string(), error)) << error;
    EXPECT_TRUE(std::filesystem::exists(plain));
    EXPECT_EQ(std::filesystem::file_size(plain), 0u);

    EXPECT_TRUE(PartialFiles(_directory).empty());
}

TEST_F(TypeDumpWriterTest, ConcurrentSavesOfOneTargetAllSucceed)
{
    std::filesystem::path const target = _directory / "shared" / "r806919.json";
    constexpr std::size_t Threads = 8;
    constexpr std::size_t SavesPerThread = 25;
    std::vector<std::string> texts;
    for (std::size_t index = 0; index < Threads; ++index)
        texts.push_back(std::string(4096 + index * 512, static_cast<char>('a' + index)));
    std::vector<std::vector<std::string>> errors(Threads);
    std::vector<std::thread> workers;
    for (std::size_t index = 0; index < Threads; ++index)
    {
        workers.emplace_back([&, index]
        {
            for (std::size_t save = 0; save < SavesPerThread; ++save)
            {
                std::string error;
                if (!TypeDumpWriter::Save(target, texts[index], error))
                    errors[index].push_back(error);
            }
        });
    }
    for (std::thread& worker : workers)
        worker.join();
    for (std::vector<std::string> const& failures : errors)
    {
        for (std::string const& failure : failures)
            ADD_FAILURE() << failure;
    }
    EXPECT_NE(std::find(texts.begin(), texts.end(), ReadFile(target)), texts.end());
    EXPECT_TRUE(PartialFiles(_directory).empty());
}

TEST_F(TypeDumpWriterTest, SaveReportsPathsItCannotWrite)
{
    std::string error;
    std::filesystem::path const blocker = _directory / "blocker";
    std::ofstream(blocker, std::ios::binary) << "a file where a folder must be";
    std::filesystem::path const underFile = blocker / "types" / "dump.json";
    EXPECT_FALSE(TypeDumpWriter::Save(underFile, "{}", error));
    EXPECT_NE(error.find(ConfigMgr::PathToUtf8(underFile)), std::string::npos) << error;
    EXPECT_GT(error.size(), ConfigMgr::PathToUtf8(underFile).size() + 20) << error;
    EXPECT_EQ(ReadFile(blocker), "a file where a folder must be");

    std::filesystem::path const directParent = blocker / "dump.json";
    error.clear();
    EXPECT_FALSE(TypeDumpWriter::Save(directParent, "{}", error));
    EXPECT_NE(error.find(ConfigMgr::PathToUtf8(directParent)), std::string::npos) << error;

    std::filesystem::path const folder = _directory / "taken.json";
    std::filesystem::create_directories(folder / "inside");
    error.clear();
    EXPECT_FALSE(TypeDumpWriter::Save(folder, "{}", error));
    EXPECT_NE(error.find(ConfigMgr::PathToUtf8(folder)), std::string::npos) << error;
    EXPECT_NE(error.find("temporary file"), std::string::npos) << error;
    std::regex const temporaryName("taken\\.json\\.([0-9]+)\\.([0-9a-f]{16})\\.partial");
    std::smatch first;
    EXPECT_TRUE(std::regex_search(error, first, temporaryName)) << error;
    std::string secondError;
    EXPECT_FALSE(TypeDumpWriter::Save(folder, "{}", secondError));
    std::smatch second;
    EXPECT_TRUE(std::regex_search(secondError, second, temporaryName)) << secondError;
    if (!first.empty() && !second.empty())
    {
        EXPECT_EQ(first[1].str(), second[1].str());
        EXPECT_NE(first[2].str(), second[2].str());
    }
    EXPECT_TRUE(PartialFiles(_directory).empty());
    EXPECT_TRUE(std::filesystem::is_directory(folder / "inside"));

    std::filesystem::path const partialFolder = _directory / "blocked.json.partial";
    std::filesystem::create_directories(partialFolder);
    error.clear();
    EXPECT_TRUE(TypeDumpWriter::Save(_directory / "blocked.json", "{}", error)) << error;
    EXPECT_TRUE(std::filesystem::is_directory(partialFolder));
    EXPECT_EQ(ReadFile(_directory / "blocked.json"), "{}");

    error.clear();
    EXPECT_FALSE(TypeDumpWriter::Save(_directory / "", "{}", error));
    EXPECT_NE(error.find("names a folder"), std::string::npos) << error;
}

TEST_F(TypeDumpWriterTest, CompareFindsNothingForEqualDumps)
{
    RawDump const dump = SampleDump();
    EXPECT_TRUE(TypeDumpWriter::Compare(dump, dump).empty());
    RawDump reordered = dump;
    std::reverse(reordered.Classes.begin(), reordered.Classes.end());
    EXPECT_TRUE(TypeDumpWriter::Compare(dump, reordered).empty());
    EXPECT_TRUE(TypeDumpWriter::Compare(dump, ParseBack(TypeDumpWriter::ToJson(dump, Metadata))).empty());
    EXPECT_TRUE(TypeDumpWriter::Compare(RawDump{}, RawDump{}).empty());
}

TEST_F(TypeDumpWriterTest, CompareReportsEachDifferenceOnce)
{
    RawProperty ourPhase = MakeProperty("enum DuelPhase", "m_duelPhase", 3);
    ourPhase.Options = {
        { "kA", OptionValue{ int64{ 0 } } },
        { "kB", OptionValue{ int64{ 1 } } },
        { "kOurs", OptionValue{ int64{ 2 } } },
        { "kText", OptionValue{ std::string() } }
    };
    RawProperty theirPhase = ourPhase;
    theirPhase.Options = {
        { "kB", OptionValue{ int64{ 1 } } },
        { "kA", OptionValue{ int64{ 0 } } },
        { "kTheirs", OptionValue{ int64{ 3 } } },
        { "kText", OptionValue{ int64{ 0 } } }
    };
    RawProperty const ourC = MakeProperty("int", "m_c", 2);
    RawProperty theirC = ourC;
    theirC.Type = "float";
    theirC.Id = 7;
    theirC.Offset = 99;
    theirC.Flags = 30;
    theirC.Container = "Vector";
    theirC.Dynamic = true;
    theirC.Singleton = true;
    theirC.Pointer = true;
    theirC.Hash = 12345;
    RawProperty const a = MakeProperty("int", "m_a", 0);
    RawProperty const b = MakeProperty("int", "m_b", 1);

    RawClass const ourDuel = MakeClass("class Duel", { "TestBase", "PropertyClass" }, { a, b, ourC, ourPhase, MakeProperty("int", "m_onlyOurs", 4) });
    RawClass theirDuel = MakeClass("class Duel", { "PropertyClass" }, { b, a, theirC, theirPhase, MakeProperty("int", "m_onlyTheirs", 4) });
    theirDuel.Hash = *ourDuel.Hash + 1;
    RawClass const same = MakeClass("class Same", { "PropertyClass" }, { MakeProperty("int", "m_value", 0) });

    RawDump const ours = MakeDump({ same, ourDuel, MakeClass("class Alpha", {}, {}) });
    RawDump const theirs = MakeDump({ MakeClass("class Omega", {}, {}), theirDuel, same });
    std::string const duelHash = std::to_string(*ourDuel.Hash);
    std::string const theirDuelHash = std::to_string(*theirDuel.Hash);
    std::string const cHash = std::to_string(*ourC.Hash);

    std::vector<std::array<std::string, 5>> const expected{
        { "class Alpha", "", "class", "present", "missing" },
        { "class Duel", "", "hash", duelHash, theirDuelHash },
        { "class Duel", "", "bases", "[TestBase, PropertyClass]", "[PropertyClass]" },
        { "class Duel", "", "order", "[m_a, m_b, m_c, m_duelPhase]", "[m_b, m_a, m_c, m_duelPhase]" },
        { "class Duel", "m_c", "type", "int", "float" },
        { "class Duel", "m_c", "id", "2", "7" },
        { "class Duel", "m_c", "offset", "24", "99" },
        { "class Duel", "m_c", "flags", "31", "30" },
        { "class Duel", "m_c", "container", "Static", "Vector" },
        { "class Duel", "m_c", "dynamic", "false", "true" },
        { "class Duel", "m_c", "singleton", "false", "true" },
        { "class Duel", "m_c", "pointer", "false", "true" },
        { "class Duel", "m_c", "hash", cHash, "12345" },
        { "class Duel", "m_duelPhase", "option order", "[kA, kB, kText]", "[kB, kA, kText]" },
        { "class Duel", "m_duelPhase", "option kOurs", "present", "missing" },
        { "class Duel", "m_duelPhase", "option kText", "\"\"", "0" },
        { "class Duel", "m_duelPhase", "option kTheirs", "missing", "present" },
        { "class Duel", "m_onlyOurs", "property", "present", "missing" },
        { "class Duel", "m_onlyTheirs", "property", "missing", "present" },
        { "class Omega", "", "class", "missing", "present" }
    };
    std::vector<TypeDumpDifference> const differences = TypeDumpWriter::Compare(ours, theirs);
    EXPECT_EQ(Rows(differences), expected);

    std::vector<std::string> described;
    for (TypeDumpDifference const& difference : differences)
        described.push_back(TypeDumpWriter::Describe(difference));
    std::vector<std::string> const expectedDescriptions{
        "class Alpha: only in ours",
        "class Duel: hash ours " + duelHash + ", theirs " + theirDuelHash,
        "class Duel: bases ours [TestBase, PropertyClass], theirs [PropertyClass]",
        "class Duel: order ours [m_a, m_b, m_c, m_duelPhase], theirs [m_b, m_a, m_c, m_duelPhase]",
        "class Duel property m_c: type ours int, theirs float",
        "class Duel property m_c: id ours 2, theirs 7",
        "class Duel property m_c: offset ours 24, theirs 99",
        "class Duel property m_c: flags ours 31, theirs 30",
        "class Duel property m_c: container ours Static, theirs Vector",
        "class Duel property m_c: dynamic ours false, theirs true",
        "class Duel property m_c: singleton ours false, theirs true",
        "class Duel property m_c: pointer ours false, theirs true",
        "class Duel property m_c: hash ours " + cHash + ", theirs 12345",
        "class Duel property m_duelPhase: option order ours [kA, kB, kText], theirs [kB, kA, kText]",
        "class Duel property m_duelPhase: option kOurs only in ours",
        "class Duel property m_duelPhase: option kText ours \"\", theirs 0",
        "class Duel property m_duelPhase: option kTheirs only in theirs",
        "class Duel property m_onlyOurs: only in ours",
        "class Duel property m_onlyTheirs: only in theirs",
        "class Omega: only in theirs"
    };
    EXPECT_EQ(described, expectedDescriptions);

    std::vector<std::array<std::string, 5>> swapped;
    for (TypeDumpDifference const& difference : TypeDumpWriter::Compare(theirs, ours))
        swapped.push_back({ difference.Class, difference.Property, difference.Field, difference.Theirs, difference.Ours });
    EXPECT_EQ(swapped, expected);
}

TEST_F(TypeDumpWriterTest, CompareHandlesMissingFieldsUnnamedAndDuplicateClasses)
{
    RawClass ourPartial = MakeClass("class Partial", {}, { MakeProperty("int", "m_x", 0) });
    ourPartial.Hash.reset();
    ourPartial.Properties[0].Id.reset();
    RawClass theirPartial = MakeClass("class Partial", {}, { MakeProperty("int", "m_x", 3) });
    theirPartial.Hash = 5;
    theirPartial.Properties[0].Offset = 8;
    theirPartial.Properties[0].Options = { { "kOnly", OptionValue{ std::string("text") } } };

    RawClass unnamed;
    unnamed.Key = "77";
    RawClass twinOne = MakeClass("class Twin", {}, { MakeProperty("int", "m_p", 0), MakeProperty("int", "m_p", 0) });
    twinOne.Key = "1";
    RawClass twinTwo = MakeClass("class Twin", {}, {});
    twinTwo.Key = "2";
    RawClass twinTheirs = MakeClass("class Twin", {}, { MakeProperty("int", "m_p", 0) });
    twinTheirs.Key = "1";

    RawDump const ours = MakeDump({ unnamed, twinTwo, ourPartial, twinOne });
    RawDump const theirs = MakeDump({ twinTheirs, theirPartial, MakeClass("unsigned int", {}, {}) });
    std::vector<std::array<std::string, 5>> const expected{
        { "class Partial", "", "hash", "missing", "5" },
        { "class Partial", "m_x", "id", "missing", "3" },
        { "class Partial", "m_x", "option kOnly", "missing", "present" },
        { "class Twin", "m_p", "property", "present", "missing" },
        { "class Twin", "", "class", "present", "missing" },
        { "unsigned int", "", "class", "missing", "present" },
        { "the class under key 77", "", "class", "present", "missing" }
    };
    std::vector<TypeDumpDifference> const differences = TypeDumpWriter::Compare(ours, theirs);
    EXPECT_EQ(Rows(differences), expected);
    ASSERT_EQ(differences.size(), expected.size());
    EXPECT_EQ(TypeDumpWriter::Describe(differences[0]), "class Partial: hash ours missing, theirs 5");
    EXPECT_EQ(TypeDumpWriter::Describe(differences[2]), "class Partial property m_x: option kOnly only in theirs");
    EXPECT_EQ(TypeDumpWriter::Describe(differences[5]), "unsigned int: only in theirs");
    EXPECT_EQ(TypeDumpWriter::Describe(differences[6]), "the class under key 77: only in ours");
}

TEST_F(TypeDumpWriterTest, DescribeNamesTheClassPropertyAndSides)
{
    EXPECT_EQ(TypeDumpWriter::Describe(TypeDumpDifference{ "class Duel", "m_duelPhase", "flags", "31", "30" }), "class Duel property m_duelPhase: flags ours 31, theirs 30");
    EXPECT_EQ(TypeDumpWriter::Describe(TypeDumpDifference{ "class ZoneStats", "", "class", "present", "missing" }), "class ZoneStats: only in ours");
    EXPECT_EQ(TypeDumpWriter::Describe(TypeDumpDifference{ "class ZoneStats", "", "class", "missing", "present" }), "class ZoneStats: only in theirs");
    EXPECT_EQ(TypeDumpWriter::Describe(TypeDumpDifference{ "class ZoneStats", "m_zone", "property", "missing", "present" }), "class ZoneStats property m_zone: only in theirs");
    EXPECT_EQ(TypeDumpWriter::Describe(TypeDumpDifference{ "class Duel", "m_duelPhase", "type", "enum A", "missing" }), "class Duel property m_duelPhase: type ours enum A, theirs missing");
    EXPECT_EQ(TypeDumpWriter::Describe(TypeDumpDifference{ "class Bad\nName", "m_\x1B[31m", "option \x7F", "\"a\\b\"", "0" }),
        "class Bad\\x0AName property m_\\x1B[31m: option \\x7F ours \"a\\\\b\", theirs 0");
}
