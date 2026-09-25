/*
 * Project Ambrose by Imjustchico
 * Tests the extraction's checks that need no client: installs whose revision is missing or cannot name a dump file refused before anything loads, plain revisions and the default output path refused for bad revisions and empty or relative data folders, enum eRace properties that lack a race from Races.xml named with the race, and dumps the server's type loader refuses reported with its errors, capped.
 */

#include "ClientLocator.h"
#include "LogTestDirectory.h"
#include "StringHash.h"
#include "TypeExtraction.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace
{
    using RawClass = TypeDumpLoader::RawClass;
    using RawDump = TypeDumpLoader::RawDump;
    using RawProperty = TypeDumpLoader::RawProperty;
    using OptionValue = std::variant<int64, std::string>;

    std::filesystem::path MakeInstall(LogTestDirectory const& directory, std::string const& name, std::optional<std::string> const& revision)
    {
        directory.Write(name + "/Data/GameData/Root.wad", "");
        if (revision)
            directory.Write(name + "/Bin/revision.dat", *revision);
        return directory.Path() / name;
    }

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
        dump.Version = TypeDumpLoader::SupportedVersion;
        dump.HasClasses = true;
        dump.Classes = std::move(classes);
        return dump;
    }

    RawProperty RaceProperty(std::string const& name, uint64 id, std::vector<std::string> const& races)
    {
        RawProperty property = MakeProperty(std::string(TypeExtraction::RaceEnumType), name, id);
        int64 value = 0;
        for (std::string const& race : races)
            property.Options.emplace_back(race, OptionValue{ value++ });
        return property;
    }
}

TEST(TypeExtractionTest, InstallsWhoseRevisionCannotNameADumpAreRefusedBeforeLoading)
{
    LogTestDirectory directory;
    TypeExtractionOptions options;

    options.ClientDir = directory.Path() / "empty";
    std::filesystem::create_directories(options.ClientDir);
    TypeExtractionResult result = TypeExtraction::Extract(options);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_NE(result.Error.find("holds no Wizard101 install"), std::string::npos) << result.Error;

    options.ClientDir = MakeInstall(directory, "unnamed", std::nullopt);
    result = TypeExtraction::Extract(options);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.Error, ClientLocator::PathText(options.ClientDir) + " has no readable Bin/revision.dat, so the dump cannot be named");
    EXPECT_TRUE(result.Discovered.empty());

    options.ClientDir = MakeInstall(directory, "unreadable", std::string("\xEF\xBB\xBFr806919"));
    result = TypeExtraction::Extract(options);
    EXPECT_NE(result.Error.find("has no readable Bin/revision.dat"), std::string::npos) << result.Error;

    std::vector<std::string> const oddRevisions = { "..", ".", "../../evil", "r1:x", "types/r1", "r1\\x", "r1*" };
    for (std::string const& revision : oddRevisions)
    {
        options.ClientDir = MakeInstall(directory, "odd", revision);
        result = TypeExtraction::Extract(options);
        EXPECT_FALSE(result.Succeeded()) << revision;
        EXPECT_NE(result.Error.find("names the revision " + revision + " in Bin/revision.dat, which cannot name a dump file"), std::string::npos) << result.Error;
        EXPECT_TRUE(result.Metadata.Revision.empty()) << revision;
    }

    options.ClientDir = MakeInstall(directory, "plain", std::string("r806919.Wizard_1_610\n"));
    result = TypeExtraction::Extract(options);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.Error.find("revision.dat"), std::string::npos) << result.Error;
    EXPECT_EQ(result.Metadata.Revision, "r806919.Wizard_1_610");
}

TEST(TypeExtractionTest, LayoutEvidenceNamesEveryFieldAndStrictModeDoesNotAssumeAnUnloadedClient)
{
    ClientLayout layout;
    std::vector<ClientLayoutEvidence> const evidence = layout.Evidence();
    EXPECT_EQ(evidence.size(), 33u);
    EXPECT_TRUE(std::all_of(evidence.begin(), evidence.end(), [](ClientLayoutEvidence const& item)
    {
        return !item.Field.empty() && !item.Status.empty() && !item.ConfirmedBy.empty();
    }));
    layout.ConfirmDerived("Type.hash", "matched registered type hashes");
    std::vector<ClientLayoutEvidence> const updated = layout.Evidence();
    auto const derived = std::find_if(updated.begin(), updated.end(), [](ClientLayoutEvidence const& item) { return item.Field == "Type.hash"; });
    ASSERT_NE(derived, updated.end());
    EXPECT_EQ(derived->Status, "derived");
    EXPECT_EQ(derived->ConfirmedBy, "matched registered type hashes");

    LogTestDirectory directory;
    TypeExtractionOptions options;
    options.ClientDir = MakeInstall(directory, "unknown-layout", std::string("r999999"));
    options.RequireDerivedLayout = true;
    TypeExtractionResult result = TypeExtraction::Extract(options);
    EXPECT_FALSE(result.Succeeded());
    EXPECT_NE(result.Error.find("WizardGraphicalClient.exe was not found"), std::string::npos) << result.Error;
    EXPECT_EQ(result.LayoutEvidence.size(), evidence.size());
}

TEST(TypeExtractionTest, DefaultOutputPathNeedsAPlainRevisionAndAnAbsoluteDataFolder)
{
    std::vector<std::string> const plain = { "r806919.Wizard_1_610", "r801440", "V_r806919.Wizard_1_610", "a-b_c.d", "..." };
    for (std::string const& revision : plain)
        EXPECT_TRUE(TypeExtraction::IsPlainRevision(revision)) << revision;
    std::vector<std::string> const odd = { "", ".", "..", "../evil", "a/b", "a\\b", "r1:x", "r 1", "r1\x7F", "caf\xC3\xA9", std::string("r1\0x", 4) };
    for (std::string const& revision : odd)
        EXPECT_FALSE(TypeExtraction::IsPlainRevision(revision)) << revision;

    std::filesystem::path const data = std::filesystem::current_path() / "ProjectAmbrose";
    ASSERT_TRUE(data.is_absolute());
    std::optional<std::filesystem::path> const path = TypeExtraction::DefaultOutputPath(data, "r806919.Wizard_1_610");
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(ClientLocator::PathText(*path), ClientLocator::PathText(data / "types" / "r806919.Wizard_1_610.json"));
    EXPECT_FALSE(TypeExtraction::DefaultOutputPath(data, "").has_value());
    EXPECT_FALSE(TypeExtraction::DefaultOutputPath(data, "..").has_value());
    EXPECT_FALSE(TypeExtraction::DefaultOutputPath(data, "../../../evil").has_value());
    EXPECT_FALSE(TypeExtraction::DefaultOutputPath(data, "r1:x").has_value());
    EXPECT_FALSE(TypeExtraction::DefaultOutputPath(std::filesystem::path(), "r806919").has_value());
    EXPECT_FALSE(TypeExtraction::DefaultOutputPath(std::filesystem::path("ProjectAmbrose"), "r806919").has_value());
}

TEST(TypeExtractionTest, RacePropertiesMustHoldEveryRace)
{
    std::vector<std::string> const races = { "Human", "Elf", "Gnome" };
    RawDump dump = MakeDump({
        MakeClass("class Complete", {}, { RaceProperty("m_race", 0, { "Gnome", "Human", "Elf", "Extra" }) }),
        MakeClass("class Unrelated", {}, { MakeProperty("enum eOther", "m_other", 0), MakeProperty("int", "m_race", 1) }),
        MakeClass("class Partial", {}, { MakeProperty("int", "m_value", 0), RaceProperty("m_targetRace", 1, { "Human" }) })
    });
    std::string error;
    EXPECT_FALSE(TypeExtraction::CheckRaces(dump, races, error));
    EXPECT_EQ(error, "the races did not land: class Partial property m_targetRace of type enum eRace lacks 2 of the 3 races from Races.xml, among them Elf");

    dump.Classes[2].Properties[1] = RaceProperty("m_targetRace", 1, races);
    error.clear();
    EXPECT_TRUE(TypeExtraction::CheckRaces(dump, races, error)) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(TypeExtraction::CheckRaces(MakeDump({ MakeClass("class PropertyClass", {}, {}) }), races, error));

    std::vector<std::string> const more = { "Human", "Elf", "Gnome", "Troll" };
    EXPECT_FALSE(TypeExtraction::CheckRaces(dump, more, error));
    EXPECT_EQ(error, "the races did not land: class Complete property m_race of type enum eRace lacks 1 of the 4 races from Races.xml, among them Troll");
}

TEST(TypeExtractionTest, DumpsTheLoaderRefusesAreReportedWithItsErrors)
{
    RawClass const propertyClass = MakeClass("class PropertyClass", {}, {});
    RawClass const widget = MakeClass("class Widget", { "PropertyClass" }, { MakeProperty("unsigned __int64", "m_id", 0), MakeProperty("std::string", "m_name", 1) });
    std::string error;
    EXPECT_TRUE(TypeExtraction::CheckCatalog(MakeDump({ propertyClass, widget }), std::string(64, '0'), error)) << error;
    EXPECT_TRUE(error.empty());

    RawClass orphan = widget;
    orphan.Bases = { "MissingBase" };
    EXPECT_FALSE(TypeExtraction::CheckCatalog(MakeDump({ propertyClass, orphan }), std::string(64, '0'), error));
    EXPECT_EQ(error, "the server's type loader refuses the extracted dump: class Widget names base MissingBase, which the dump does not list");

    RawClass unlisted = widget;
    unlisted.Properties.push_back(MakeProperty("class Unlisted", "m_unlisted", 2));
    EXPECT_FALSE(TypeExtraction::CheckCatalog(MakeDump({ propertyClass, unlisted }), std::string(64, '0'), error));
    EXPECT_NE(error.find("class Widget property m_unlisted has type class Unlisted, which the dump does not list"), std::string::npos) << error;

    EXPECT_FALSE(TypeExtraction::CheckCatalog(MakeDump({ widget }), std::string(64, '0'), error));
    EXPECT_NE(error.find("the type dump has no class PropertyClass"), std::string::npos) << error;

    std::vector<RawClass> classes = { propertyClass };
    for (int index = 0; index < 30; ++index)
        classes.push_back(MakeClass("class Orphan" + std::to_string(index), { "MissingBase" }, {}));
    EXPECT_FALSE(TypeExtraction::CheckCatalog(MakeDump(classes), std::string(64, '0'), error));
    std::size_t listed = 0;
    for (std::size_t at = error.find("names base MissingBase"); at != std::string::npos; at = error.find("names base MissingBase", at + 1))
        ++listed;
    EXPECT_EQ(listed, TypeExtraction::MaxListedLoaderErrors) << error;
    EXPECT_TRUE(error.ends_with("; and 10 more")) << error;
}
