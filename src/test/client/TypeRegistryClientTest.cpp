/*
 * Project Ambrose by Imjustchico
 * Loads the user's own r806919 type dump into the type registry when AMBROSE_TYPE_DUMP_PATH names it, and checks the class kind counts for the shape the dump's own header names, the reference dump's or the larger one this project's extractor writes, that every property type is classified, the load time and size, WizClientObject's first properties in id order, that pointer aliases of unprefixed templates join their class, and that integer defaults are not taken for enum options; and, for any dump it names, that the binary cache written from it builds a registry equal to the JSON one class by class, every property's attributes and enum options included, and that an optimized build loads that cache in a median under 200 ms.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "TypeDumpCache.h"
#include "TypeDumpLoader.h"
#include "TypeRegistry.h"
#include "TypeRegistryBinary.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
    constexpr std::string_view PinnedRevision = "r806919.Wizard_1_610";
    constexpr std::string_view OurExtractor = "typeextract";
}

TEST(TypeRegistryClientTest, TheR806919DumpLoadsAndClassifiesEveryType)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump (format v2) from your own client to run this test";

    std::optional<TypeDumpHeader> const header = TypeDumpCache::ReadHeader(LogConfig::Utf8Path(*path));
    if (header && !header->Revision.empty() && header->Revision != PinnedRevision)
        GTEST_SKIP() << "these counts are r806919.Wizard_1_610's, and AMBROSE_TYPE_DUMP_PATH names " << header->Revision;
    bool const ours = header && header->Extractor.starts_with(OurExtractor);

    TypeRegistry registry;
    auto const start = std::chrono::steady_clock::now();
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path))) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    auto const elapsed = std::chrono::steady_clock::now() - start;
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_TRUE(catalog);

    EXPECT_EQ(catalog->GetClassCount(ClassKind::PropertyClass), ours ? 2198u : 2197u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Enum), ours ? 141u : 140u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Primitive), 37u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Container), 168u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::ValueType), 13u);
    EXPECT_EQ(catalog->GetClassCount(ClassKind::Opaque), ours ? 39u : 37u);
    EXPECT_EQ(catalog->GetClasses().size(), ours ? 2596u : 2592u);
    EXPECT_EQ(catalog->GetAliasCount(), ours ? 4398u : 4397u);
    EXPECT_EQ(catalog->GetPropertyCount(), ours ? 16495u : 16493u);
    EXPECT_LT(elapsed, std::chrono::seconds(10));
    EXPECT_LT(catalog->GetApproximateBytes(), std::size_t{ 150 } << 20);
    std::cout << "Loaded the type dump in " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms, about " << ((catalog->GetApproximateBytes() + (std::size_t{ 1 } << 19)) >> 20) << " MiB\n";

    ClassInfo const* const wizClientObject = catalog->FindClass("class WizClientObject");
    ASSERT_NE(wizClientObject, nullptr);
    ASSERT_EQ(wizClientObject->Properties.size(), 14u);
    EXPECT_EQ(wizClientObject->Properties[0].Name, "m_inactiveBehaviors");
    EXPECT_EQ(wizClientObject->Properties[1].Name, "m_globalID.m_full");
    EXPECT_EQ(wizClientObject->Properties[2].Name, "m_permID");
    EXPECT_EQ(wizClientObject->Properties[0].Kind, ValueKind::Object);
    EXPECT_EQ(wizClientObject->Properties[0].Container, ContainerKind::Vector);
    ClassInfo const* const clientObject = catalog->FindClass("class ClientObject");
    ASSERT_NE(clientObject, nullptr);
    EXPECT_TRUE(wizClientObject->IsA(*clientObject));
    EXPECT_EQ(catalog->FindClass("class SharedPointer<class WizClientObject>"), wizClientObject);

    ClassInfo const* const madlibFloat = catalog->FindClass("MadlibArgT<float>");
    ASSERT_NE(madlibFloat, nullptr);
    EXPECT_EQ(catalog->FindClass("class MadlibArgT<float>*"), madlibFloat);
    EXPECT_EQ(catalog->FindClass("class MadlibArgT<float>"), nullptr);

    std::size_t integerDefaults = 0;
    for (ClassInfo const* info : catalog->GetClasses())
        for (PropertyInfo const& property : info->Properties)
        {
            EXPECT_FALSE(property.FindOptionValue("__DEFAULT")) << info->Name << "::" << property.Name;
            if (property.Default && std::holds_alternative<int64>(*property.Default))
                ++integerDefaults;
        }
    EXPECT_GT(integerDefaults, 0u);
}

namespace
{
    std::string Describe(ClassInfo const& info)
    {
        std::ostringstream text;
        text << TypeKinds::GetName(info.Kind) << " " << info.Hash << " bytes " << info.DefaultBytes << " objects " << info.DefaultObjects << " depth " << info.DefaultDepth << " bases";
        for (ClassInfo const* base : info.Bases)
            text << " " << (base ? base->Name : std::string("?"));
        for (PropertyInfo const& property : info.Properties)
        {
            text << "\n  " << property.Name << " hash " << property.Hash << " id " << property.Id << " offset " << property.Offset << " flags " << property.Flags << " "
                 << TypeKinds::GetName(property.Container) << " dynamic " << property.Dynamic << " singleton " << property.Singleton << " pointer " << property.Pointer << " type "
                 << property.TypeName << " " << TypeKinds::GetName(property.Kind) << " bits " << static_cast<int>(property.BitWidth) << " class "
                 << (property.Type ? property.Type->Name : std::string("-")) << " base " << property.OptionBaseClass << " default bytes " << property.DefaultBytes << " default ";
            if (!property.Default)
                text << "none";
            else if (std::holds_alternative<int64>(*property.Default))
                text << std::get<int64>(*property.Default);
            else
                text << "'" << std::get<std::string>(*property.Default) << "'";
            text << " options";
            for (EnumOption const& option : property.Options)
                text << " " << option.Name << "=" << option.Value;
            text << " text";
            for (TextOption const& option : property.TextOptions)
                text << " " << option.Name << "='" << option.Text << "'";
        }
        return text.str();
    }

    std::map<std::string, std::string> Tables(TypeCatalog const& catalog)
    {
        std::map<std::string, std::string> tables;
        for (ClassInfo const* info : catalog.GetClasses())
            tables.emplace(info->Name, Describe(*info));
        return tables;
    }

    struct BinaryCache
    {
        std::filesystem::path Json;
        std::filesystem::path Binary;
        std::string Revision;
    };

    std::optional<BinaryCache> WriteBinaryCache(std::string& skip, std::string& error)
    {
        std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
        if (!path || path->empty())
        {
            skip = "set AMBROSE_TYPE_DUMP_PATH to a type dump from your own client to run this test";
            return std::nullopt;
        }
        BinaryCache cache;
        cache.Json = LogConfig::Utf8Path(*path);
        std::optional<TypeDumpHeader> const header = TypeDumpCache::ReadHeader(cache.Json);
        cache.Revision = header && !header->Revision.empty() ? header->Revision : std::string("unknown");
        std::ifstream stream(cache.Json, std::ios::binary);
        std::string const text{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        TypeDumpLoader::RawDump dump;
        std::vector<std::string> parseErrors;
        if (!TypeDumpLoader::Parse(text, dump, parseErrors))
        {
            error = parseErrors.empty() ? std::string("the dump does not parse") : parseErrors.front();
            return std::nullopt;
        }
        cache.Binary = std::filesystem::temp_directory_path() / "ambrose-typeregistry-client-cache.bin";
        if (!TypeRegistryBinary::Write(cache.Binary, dump, cache.Revision, error))
            return std::nullopt;
        return cache;
    }
}

TEST(TypeRegistryClientTest, TheBinaryCacheBuildsTheSameRegistryAsTheJsonDumpClassByClass)
{
    std::string skip;
    std::string error;
    std::optional<BinaryCache> const cache = WriteBinaryCache(skip, error);
    if (!skip.empty())
        GTEST_SKIP() << skip;
    ASSERT_TRUE(cache) << error;

    TypeRegistry fromJson;
    ASSERT_TRUE(fromJson.LoadFromFile(cache->Json)) << (fromJson.GetErrors().empty() ? std::string() : fromJson.GetErrors().front());
    TypeRegistry fromBinary;
    ASSERT_TRUE(fromBinary.LoadBinary(cache->Binary, cache->Revision)) << (fromBinary.GetErrors().empty() ? std::string() : fromBinary.GetErrors().front());

    TypeCatalogPtr const json = fromJson.GetCatalog();
    TypeCatalogPtr const binary = fromBinary.GetCatalog();
    ASSERT_TRUE(json && binary);
    EXPECT_EQ(binary->GetPropertyCount(), json->GetPropertyCount());
    EXPECT_EQ(binary->GetAliasCount(), json->GetAliasCount());
    for (ClassKind const kind : { ClassKind::PropertyClass, ClassKind::Enum, ClassKind::Primitive, ClassKind::Container, ClassKind::ValueType, ClassKind::Opaque })
        EXPECT_EQ(binary->GetClassCount(kind), json->GetClassCount(kind)) << TypeKinds::GetName(kind);

    std::map<std::string, std::string> const expected = Tables(*json);
    std::map<std::string, std::string> const actual = Tables(*binary);
    ASSERT_EQ(actual.size(), expected.size());
    std::size_t differences = 0;
    for (auto const& [name, table] : expected)
    {
        auto const found = actual.find(name);
        if (found == actual.end())
        {
            ADD_FAILURE() << name << " is missing from the registry the binary cache built";
            ++differences;
        }
        else if (found->second != table)
        {
            if (differences < 5)
                ADD_FAILURE() << name << " differs.\nFrom the JSON dump:\n" << table << "\nFrom the binary cache:\n" << found->second;
            ++differences;
        }
    }
    EXPECT_EQ(differences, 0u);
    std::cout << "Compared " << expected.size() << " classes and " << json->GetPropertyCount() << " properties, with every enum option, between the JSON dump and the binary cache\n";

    std::error_code ignored;
    std::filesystem::remove(cache->Binary, ignored);
}

TEST(TypeRegistryClientTest, TheBinaryCacheOfTheDumpLoadsInUnder200Milliseconds)
{
#ifndef NDEBUG
    GTEST_SKIP() << "the load time is measured in an optimized build, since a debug build times the checks rather than the load";
#else
    std::string skip;
    std::string error;
    std::optional<BinaryCache> const cache = WriteBinaryCache(skip, error);
    if (!skip.empty())
        GTEST_SKIP() << skip;
    ASSERT_TRUE(cache) << error;

    std::vector<std::chrono::microseconds> rounds;
    for (int round = 0; round != 7; ++round)
    {
        TypeRegistry registry;
        auto const start = std::chrono::steady_clock::now();
        ASSERT_TRUE(registry.LoadBinary(cache->Binary, cache->Revision)) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        rounds.push_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start));
    }
    std::sort(rounds.begin(), rounds.end());
    std::chrono::microseconds const median = rounds[rounds.size() / 2];
    std::cout << "The binary cache loaded in a median of " << median.count() / 1000.0 << " ms over " << rounds.size() << " loads\n";
    EXPECT_LT(median, std::chrono::milliseconds(200));

    std::error_code ignored;
    std::filesystem::remove(cache->Binary, ignored);
#endif
}
