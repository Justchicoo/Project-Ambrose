/*
 * Project Ambrose by Imjustchico
 * Builds a format v2 type dump from the user's own install without launching the game: derives Type and std::string layout from values passed to the client's constructor, refuses strict extraction while any field remains assumed, runs initializers and lazy getters, adds the races from Root.wad through the client's race adder, validates the dump and reports per-field evidence and extraction results.
 */

#ifndef AMBROSE_TYPEEXTRACTION_H
#define AMBROSE_TYPEEXTRACTION_H

#include "ClientLayout.h"
#include "TypeDumpLoader.h"
#include "TypeDumpWriter.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct TypeExtractionOptions
{
    std::filesystem::path ClientDir;
    ClientLayout Layout;
    uint64 InitializerBudget = 200000000;
    uint64 GetterBudget = 50000000;
    uint64 RaceBudget = 200000000;
    std::function<void(std::string_view)> Progress;
    bool RequireDerivedLayout = false;
};

struct TypeExtractionStats
{
    uint64 Initializers = 0;
    uint64 LazyGetters = 0;
    uint64 LazyGettersRun = 0;
    uint64 LazyGettersFaulted = 0;
    uint64 Races = 0;
    uint64 Classes = 0;
    uint64 Properties = 0;
    uint64 DuplicateTypes = 0;
    uint64 DuplicateOptions = 0;
    uint64 HeapBytes = 0;
    uint64 LoadMilliseconds = 0;
    uint64 InitializeMilliseconds = 0;
    uint64 DiscoverMilliseconds = 0;
    uint64 WalkMilliseconds = 0;
    uint64 TotalMilliseconds = 0;
};

struct TypeExtractionResult
{
    std::string Error;
    TypeDumpLoader::RawDump Dump;
    TypeDumpMetadata Metadata;
    TypeExtractionStats Stats;
    std::map<std::string, uint64> ProblemCounts;
    std::map<std::string, std::vector<std::string>> ProblemSamples;
    std::map<std::string, uint64> UnhandledApiCalls;
    std::vector<std::string> Discovered;
    std::vector<ClientLayoutEvidence> LayoutEvidence;

    bool Succeeded() const noexcept { return Error.empty(); }
};

namespace TypeExtraction
{
    inline constexpr std::string_view ExtractorName = "typeextract 1";
    inline constexpr std::string_view RaceEnumType = "enum eRace";
    inline constexpr std::size_t MaxListedLoaderErrors = 20;

    TypeExtractionResult Extract(TypeExtractionOptions const& options);
    bool RequireDerivedLayout(ClientLayout const& layout, std::string& error);
    bool SaveDump(TypeExtractionResult const& result, std::filesystem::path const& path, std::string& error);
    bool IsPlainRevision(std::string_view revision);
    std::optional<std::filesystem::path> DefaultOutputPath(std::filesystem::path const& dataFolder, std::string_view revision);
    bool CheckRaces(TypeDumpLoader::RawDump const& dump, std::span<std::string const> races, std::string& error);
    bool CheckCatalog(TypeDumpLoader::RawDump const& dump, std::string const& sha256, std::string& error);
}

#endif
