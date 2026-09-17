/*
 * Project Ambrose by Imjustchico
 * Builds a format v2 type dump from the user's own install without launching the game: refuses an install whose revision cannot name a dump file, loads WizardGraphicalClient.exe and its runtime into the emulator, runs every C++ initializer, discovers and runs the lazy type and property list getters, adds each race from Root.wad's Races.xml through the client's race adder, walks and validates the type map, checks that every enum eRace property holds every race and that the server's type loader builds a catalog from the dump, and reports progress, timings, counts, faulted getters, problems and any Windows function the client called that the layer lacks.
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

    bool Succeeded() const noexcept { return Error.empty(); }
};

namespace TypeExtraction
{
    inline constexpr std::string_view ExtractorName = "typeextract 1";
    inline constexpr std::string_view RaceEnumType = "enum eRace";
    inline constexpr std::size_t MaxListedLoaderErrors = 20;

    TypeExtractionResult Extract(TypeExtractionOptions const& options);
    bool IsPlainRevision(std::string_view revision);
    std::optional<std::filesystem::path> DefaultOutputPath(std::filesystem::path const& dataFolder, std::string_view revision);
    bool CheckRaces(TypeDumpLoader::RawDump const& dump, std::span<std::string const> races, std::string& error);
    bool CheckCatalog(TypeDumpLoader::RawDump const& dump, std::string const& sha256, std::string& error);
}

#endif
