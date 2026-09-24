/*
 * Project Ambrose by Imjustchico
 * Extracts the type dump from the user's own install named by AMBROSE_CLIENT_DIR and checks that it validates, round-trips through the dump writer and loader into a type catalog, and matches the dump AMBROSE_TYPE_DUMP_PATH names except for classes that dump missed and option values it wrote as 0 where the client holds empty text, which for r806919.Wizard_1_610 must be exactly its 5 known classes and 60 option values against the reference dump and nothing at all against a dump this project's own extractor wrote; also extracts AMBROSE_SECOND_CLIENT_DIR when set, whose dump the extraction has already built into a catalog.
 */

#include "ConfigMgr.h"
#include "Environment.h"
#include "TypeDumpCache.h"
#include "TypeDumpLoader.h"
#include "TypeDumpWriter.h"
#include "TypeExtraction.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::string_view PinnedReferenceRevision = "r806919.Wizard_1_610";
    constexpr std::string_view OurExtractor = "typeextract";

    std::optional<std::filesystem::path> EnvironmentPath(char const* name)
    {
        std::optional<std::string> const value = Ambrose::GetEnv(name);
        if (!value || value->empty())
            return std::nullopt;
        return ConfigMgr::PathFromUtf8(*value);
    }

    TypeExtractionResult ExtractAndReport(std::filesystem::path const& client)
    {
        TypeExtractionOptions options;
        options.ClientDir = client;
        TypeExtractionResult result = TypeExtraction::Extract(options);
        for (std::string const& line : result.Discovered)
            std::cout << "[ DISCOVER ] " << line << std::endl;
        for (auto const& [call, count] : result.UnhandledApiCalls)
            std::cout << "[ STUBBED  ] " << call << " x" << count << std::endl;
        for (auto const& [kind, samples] : result.ProblemSamples)
            for (std::string const& sample : samples)
                std::cout << "[ PROBLEM  ] " << kind << ": " << sample << std::endl;
        std::cout << "[ EXTRACT  ] " << result.Metadata.Revision << ": " << result.Stats.Classes << " classes, " << result.Stats.Properties << " properties, " << result.Stats.Races << " races, "
                  << result.Stats.LazyGettersRun << " of " << result.Stats.LazyGetters << " lazy getters (" << result.Stats.LazyGettersFaulted << " faulted), " << result.Stats.TotalMilliseconds << " ms (load " << result.Stats.LoadMilliseconds
                  << ", initializers " << result.Stats.InitializeMilliseconds << ", discovery " << result.Stats.DiscoverMilliseconds << ", getters and walk " << result.Stats.WalkMilliseconds
                  << "), " << (result.Stats.HeapBytes >> 20) << " MiB heap, sha256 " << result.Metadata.ExecutableSha256 << std::endl;
        return result;
    }
}

TEST(TypeExtractionClientTest, TheInstallExtractsValidatesAndMatchesTheReferenceDump)
{
    std::optional<std::filesystem::path> const client = EnvironmentPath("AMBROSE_CLIENT_DIR");
    if (!client)
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR is not set";
    TypeExtractionResult const result = ExtractAndReport(*client);
    ASSERT_TRUE(result.Succeeded()) << result.Error;
    EXPECT_GT(result.Stats.Classes, 6000u);
    EXPECT_GT(result.Stats.Properties, 40000u);
    EXPECT_GT(result.Stats.Races, 1000u);
    EXPECT_EQ(result.Metadata.ExecutableSha256.size(), 64u);
    EXPECT_TRUE(result.UnhandledApiCalls.empty() || std::none_of(result.UnhandledApiCalls.begin(), result.UnhandledApiCalls.end(), [](auto const& entry) { return entry.first.starts_with("kernel32.dll!"); }));

    std::string const json = TypeDumpWriter::ToJson(result.Dump, result.Metadata);
    TypeDumpLoader::RawDump reloaded;
    std::vector<std::string> errors;
    ASSERT_TRUE(TypeDumpLoader::Parse(json, reloaded, errors)) << (errors.empty() ? std::string() : errors.front());
    EXPECT_TRUE(TypeDumpWriter::Compare(result.Dump, reloaded).empty());
    TypeCatalogPtr const catalog = TypeCatalogBuilder::Build(reloaded, "extracted", result.Metadata.ExecutableSha256, 1, {}, errors);
    ASSERT_NE(catalog, nullptr) << (errors.empty() ? std::string() : errors.front());

    std::optional<std::filesystem::path> const referencePath = EnvironmentPath("AMBROSE_TYPE_DUMP_PATH");
    if (!referencePath)
        return;
    std::ifstream stream(*referencePath, std::ios::binary);
    ASSERT_TRUE(stream) << ConfigMgr::PathToUtf8(*referencePath);
    std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    TypeDumpLoader::RawDump reference;
    ASSERT_TRUE(TypeDumpLoader::Parse(text, reference, errors)) << (errors.empty() ? std::string() : errors.front());

    std::set<std::string> onlyOurs;
    uint64 emptyOptionValues = 0;
    std::vector<std::string> unexpected;
    for (TypeDumpDifference const& difference : TypeDumpWriter::Compare(result.Dump, reference))
    {
        if (difference.Property.empty() && difference.Field == "class" && difference.Theirs == "missing")
            onlyOurs.insert(difference.Class);
        else if (difference.Field.starts_with("option ") && difference.Ours == "\"\"" && difference.Theirs == "0")
            ++emptyOptionValues;
        else
            unexpected.push_back(TypeDumpWriter::Describe(difference));
    }
    for (std::string const& name : onlyOurs)
        std::cout << "[ EXTRA    ] " << name << std::endl;
    std::cout << "[ COMPARE  ] " << onlyOurs.size() << " classes only in the extraction, " << emptyOptionValues << " empty option values the reference wrote as 0, " << unexpected.size() << " other differences" << std::endl;
    for (std::size_t i = 0; i < unexpected.size() && i < 50; ++i)
        ADD_FAILURE() << unexpected[i];
    EXPECT_TRUE(unexpected.empty());
    if (result.Metadata.Revision == PinnedReferenceRevision)
    {
        std::optional<TypeDumpHeader> const header = TypeDumpCache::ReadHeader(*referencePath);
        if (header && header->Extractor.starts_with(OurExtractor))
        {
            EXPECT_TRUE(onlyOurs.empty()) << "extracting an install twice must discover the same classes both times";
            EXPECT_EQ(emptyOptionValues, 0u) << "extracting an install twice must write the same option values both times";
        }
        else
        {
            std::set<std::string> const knownExtraClasses{
                "MadlibArgT<unsigned __int64>",
                "class MadlibArgT<unsigned __int64>*",
                "class WeakPointer<class PropertyClass>",
                "enum PhysicsSimMass::CylinderDirection",
                "struct CrownShopViews::OnSelectCallbackArg"
            };
            EXPECT_EQ(onlyOurs, knownExtraClasses);
            EXPECT_EQ(emptyOptionValues, 60u);
        }
    }
}

TEST(TypeExtractionClientTest, ASecondRevisionExtractsAndValidates)
{
    std::optional<std::filesystem::path> const client = EnvironmentPath("AMBROSE_SECOND_CLIENT_DIR");
    if (!client)
        GTEST_SKIP() << "AMBROSE_SECOND_CLIENT_DIR is not set";
    TypeExtractionResult const result = ExtractAndReport(*client);
    ASSERT_TRUE(result.Succeeded()) << result.Error;
    EXPECT_GT(result.Stats.Classes, 6000u);
    EXPECT_GT(result.Stats.Races, 1000u);
}
