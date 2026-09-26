/*
 * Project Ambrose by Imjustchico
 * Runs a whole extraction: refuses an install whose revision cannot name a dump file, loads the install's client program and runtime, derives Type and std::string layout from values passed to the client's constructor, runs its initializers and lazy getters, adds its races, validates the type map and server catalog, and reports per-field evidence, timings, counts and discoveries.
 */

#include "TypeExtraction.h"
#include "ClientDiscovery.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "CodeIndex.h"
#include "GuestProcess.h"
#include "KiwadArchive.h"
#include "SHA256.h"
#include "StringHash.h"
#include "TypeWalker.h"
#include "WindowsApi.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <chrono>
#include <cstddef>
#include <exception>
#include <limits>
#include <set>
#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace
{
    constexpr std::string_view ExecutableName = "WizardGraphicalClient.exe";
    constexpr std::string_view RaceFile = "Races.xml";
    constexpr uint64 TypeConstructorProbeBudget = 50000000;

    using Clock = std::chrono::steady_clock;

    uint64 Milliseconds(Clock::time_point since)
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count());
    }

    std::string Hex(std::span<uint8 const> bytes)
    {
        std::string out;
        out.reserve(bytes.size() * 2);
        for (uint8 const b : bytes)
            out += fmt::format("{:02x}", b);
        return out;
    }

    uint64 StoreStdString(GuestProcess& process, ClientLayout const& layout, std::string_view text)
    {
        Machine& machine = process.GetMachine();
        uint64 const object = process.GetHeap().Allocate(layout.StringObjectSize, true);
        if (text.size() <= layout.StringInlineCapacity)
        {
            std::vector<uint8> inlineBytes(text.begin(), text.end());
            inlineBytes.resize(16, 0);
            machine.Write(object, inlineBytes);
        }
        else
            machine.WriteU64(object, process.StoreCString(text));
        machine.WriteU64(object + layout.StringSize, text.size());
        machine.WriteU64(object + layout.StringCapacity, std::max<uint64>(text.size(), layout.StringInlineCapacity));
        return object;
    }

    bool ProbeTypeLayout(GuestProcess& process, uint64 constructor, ClientLayout& layout, std::string& error)
    {
        std::array<std::string_view, 2> const names = {
            "AmbroseProbe",
            "AmbroseTypeLayoutConstructorProbeWithAllocatedStorage"
        };
        std::vector<ConstructedTypeSample> samples;
        samples.reserve(names.size());
        for (std::string_view const name : names)
        {
            uint32 const hash = StringHash::KiStringHash(name);
            uint64 const object = process.GetHeap().Allocate(0x200, true);
            uint64 const nameAddress = process.StoreCString(name);
            try
            {
                process.Call(constructor, { object, nameAddress, hash }, TypeConstructorProbeBudget);
            }
            catch (EmulationError const& failure)
            {
                error = fmt::format("Type.name could not be derived: the constructor probe failed: {}", failure.what());
                return false;
            }
            samples.push_back({ object, std::string(name), hash });
        }
        ClientLayout candidate = layout;
        if (!ClientDiscovery::DeriveConstructedTypeLayout(process.GetMachine(), process.GetHeap(), samples, candidate, error))
            return false;
        layout = std::move(candidate);
        return true;
    }

    bool UsesReferenceTypeLayout(std::string_view revision)
    {
        return revision == "r801440" || revision == "r806919.Wizard_1_610";
    }

    std::vector<std::string> ReadRaces(std::filesystem::path const& client, std::string& error)
    {
        std::filesystem::path const wadPath = client / "Data" / "GameData" / "Root.wad";
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(wadPath, error);
        if (!archive)
        {
            error = fmt::format("cannot open {}: {}", ClientLocator::PathText(wadPath), error);
            return {};
        }
        KiwadReadResult const read = archive->Read(RaceFile);
        if (!read.Succeeded())
        {
            error = fmt::format("cannot read {} from Root.wad: {}", RaceFile, read.Error);
            return {};
        }
        pugi::xml_document document;
        pugi::xml_parse_result const parsed = document.load_buffer(read.Data.data(), read.Data.size());
        if (!parsed)
        {
            error = fmt::format("{} is not valid XML: {}", RaceFile, parsed.description());
            return {};
        }
        pugi::xml_node const root = document.document_element();
        std::vector<std::string> races;
        for (pugi::xml_node const race : root.children("Race"))
            races.emplace_back(race.child_value());
        if (races.empty())
            error = fmt::format("{} lists no races", RaceFile);
        return races;
    }
}

TypeExtractionResult TypeExtraction::Extract(TypeExtractionOptions const& options)
{
    TypeExtractionResult result;
    Clock::time_point const started = Clock::now();
    auto progress = [&](std::string const& text)
    {
        if (options.Progress)
            options.Progress(text);
    };
    auto fail = [&](std::string message) -> TypeExtractionResult&
    {
        result.Error = std::move(message);
        result.Stats.TotalMilliseconds = Milliseconds(started);
        return result;
    };

    LocalClientSystem const system;
    std::optional<ClientInstall> const install = ClientInstall::Inspect(system, options.ClientDir);
    if (!install)
        return fail(fmt::format("{} holds no Wizard101 install", ClientLocator::PathText(options.ClientDir)));
    if (install->Revision.empty())
        return fail(fmt::format("{} has no readable Bin/revision.dat, so the dump cannot be named", ClientLocator::PathText(options.ClientDir)));
    if (!IsPlainRevision(install->Revision))
        return fail(fmt::format("{} names the revision {} in Bin/revision.dat, which cannot name a dump file: only letters, digits, '.', '_' and '-' are allowed", ClientLocator::PathText(options.ClientDir), install->Revision));
    result.Metadata.Revision = install->Revision;
    result.Metadata.Extractor = std::string(ExtractorName);
    ClientLayout layout = options.Layout;
    result.LayoutEvidence = layout.Evidence();
    auto reportLayout = [&]()
    {
        result.LayoutEvidence = layout.Evidence();
        for (ClientLayoutEvidence const& evidence : result.LayoutEvidence)
            result.Discovered.push_back(fmt::format("layout {} = {:#x} ({}, confirmed by {})", evidence.Field, evidence.Value, evidence.Status, evidence.ConfirmedBy));
    };
    try
    {
        Clock::time_point phase = Clock::now();
        GuestProcess::Options processOptions;
        processOptions.Folder = options.ClientDir / "Bin";
        GuestProcess process(processOptions);
        WindowsApi::Register(process);
        progress(fmt::format("loading {} and its runtime from {}", ExecutableName, ClientLocator::PathText(processOptions.Folder)));
        GuestModule& program = process.LoadMain(ExecutableName);
        result.Metadata.ExecutableSha256 = Hex(SHA256::GetDigestOf(program.Image->GetBytes()));
        process.AttachRuntime();
        CodeIndex const code(*program.Image);
        result.Stats.LoadMilliseconds = Milliseconds(phase);

        std::string error;
        std::optional<InitializerTable> const table = ClientDiscovery::FindInitializerTable(*program.Image, code, error);
        if (!table)
            return fail(fmt::format("the C++ initializer table was not found: {}", error));
        result.Discovered.push_back(fmt::format("C++ initializers at {:#x}-{:#x} ({} slots)", table->Begin, table->End, table->Count()));

        std::optional<InitializerTable> const cTable = ClientDiscovery::FindCInitializerTable(*program.Image, code, error);
        if (!cTable)
            return fail(fmt::format("the C initializer table was not found: {}", error));
        result.Discovered.push_back(fmt::format("C initializers at {:#x}-{:#x} ({} slots)", cTable->Begin, cTable->End, cTable->Count()));

        phase = Clock::now();
        Machine& machine = process.GetMachine();
        for (uint64 slot = cTable->Begin; slot < cTable->End; slot += 8)
        {
            uint64 const function = machine.ReadU64(slot);
            if (!function)
                continue;
            uint64 status = 0;
            try
            {
                status = process.Call(function, std::span<uint64 const>{}, options.InitializerBudget);
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("C initializer {} of {} at {} failed: {}", (slot - cTable->Begin) / 8, cTable->Count(), process.DescribeAddress(function), failure.what()));
            }
            if (static_cast<uint32>(status) != 0)
                return fail(fmt::format("C initializer {} of {} at {} returned {}", (slot - cTable->Begin) / 8, cTable->Count(), process.DescribeAddress(function), static_cast<int32>(status)));
            ++result.Stats.Initializers;
        }
        progress(fmt::format("running {} C++ initializers", table->Count()));
        for (uint64 slot = table->Begin; slot < table->End; slot += 8)
        {
            uint64 const function = machine.ReadU64(slot);
            if (!function)
                continue;
            try
            {
                process.Call(function, std::span<uint64 const>{}, options.InitializerBudget);
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("C++ initializer {} of {} at {} failed: {}", (slot - table->Begin) / 8, table->Count(), process.DescribeAddress(function), failure.what()));
            }
            ++result.Stats.Initializers;
        }
        result.Stats.InitializeMilliseconds = Milliseconds(phase);

        phase = Clock::now();
        std::optional<uint64> const head = ClientDiscovery::FindTypeMapHead(machine, process.GetHeap(), layout, error);
        if (!head)
            return fail(fmt::format("the type map was not found: {}", error));
        std::optional<std::vector<uint64>> types = ClientDiscovery::WalkTypeMap(machine, *head, layout, error);
        if (!types)
            return fail(fmt::format("the type map could not be walked: {}", error));
        result.Discovered.push_back(fmt::format("type map at {:#x} with {} types after the initializers", *head, types->size()));

        std::optional<DiscoveryVote> const constructor = ClientDiscovery::FindTypeConstructor(machine, code, *types, error);
        if (!constructor)
            return fail(fmt::format("the Type constructor was not found: {}", error));
        result.Discovered.push_back(fmt::format("Type constructor at {} ({} votes, runner-up {} votes)", process.DescribeAddress(constructor->Winner), constructor->WinnerVotes, constructor->RunnerUpVotes));
        auto deriveTypeLayout = [&]()
        {
            if (!ProbeTypeLayout(process, constructor->Winner, layout, error))
            {
                result.Discovered.push_back(fmt::format("layout derivation unavailable: {}", error));
                return false;
            }
            for (ClientLayoutEvidence const& evidence : layout.Evidence())
                if (evidence.Status == "derived" && (evidence.Field == "Type.name" || evidence.Field == "Type.hash"
                    || evidence.Field.starts_with("std::string.")))
                    result.Discovered.push_back(fmt::format("layout derivation: {} at {:#x} ({})",
                        evidence.Field, evidence.Value, evidence.ConfirmedBy));
            return true;
        };
        bool const referenceLayout = UsesReferenceTypeLayout(result.Metadata.Revision);
        if (options.RequireDerivedLayout || !referenceLayout)
        {
            if (!deriveTypeLayout())
                return fail(fmt::format("the client layout could not be derived: {}", error));
            if (options.RequireDerivedLayout)
            {
                reportLayout();
                if (!RequireDerivedLayout(layout, error))
                    return fail(error);
            }
        }

        std::optional<DiscoveryVote> const listInitializer = ClientDiscovery::FindPropertyListInitializer(machine, code, *types, layout, error);
        if (!listInitializer)
            return fail(fmt::format("the PropertyList initializer was not found: {}", error));
        result.Discovered.push_back(fmt::format("PropertyList initializer at {} ({} votes, runner-up {} votes)", process.DescribeAddress(listInitializer->Winner), listInitializer->WinnerVotes, listInitializer->RunnerUpVotes));
        std::optional<uint64> const raceAdder = ClientDiscovery::FindRaceAdder(code, error);
        if (!raceAdder)
            return fail(fmt::format("the race adder was not found: {}", error));
        result.Discovered.push_back(fmt::format("race adder at {}", process.DescribeAddress(*raceAdder)));
        result.Stats.DiscoverMilliseconds = Milliseconds(phase);

        phase = Clock::now();
        std::vector<uint64> getters;
        std::unordered_set<uint64> seen;
        for (uint64 const target : { constructor->Winner, listInitializer->Winner })
            for (uint64 const function : ClientDiscovery::FunctionsCalling(code, target))
                if (seen.insert(function).second)
                    getters.push_back(function);
        result.Stats.LazyGetters = getters.size();
        progress(fmt::format("running {} lazy type and property list getters", getters.size()));
        std::array<uint64, 4> const noArguments{ 0, 0, 0, 0 };
        std::map<std::string, uint64> faultKinds;
        uint64 nullThisFaults = 0;
        for (uint64 const getter : getters)
        {
            try
            {
                process.Call(getter, noArguments, options.GetterBudget);
                ++result.Stats.LazyGettersRun;
            }
            catch (EmulationError const& failure)
            {
                std::string_view const message = failure.what();
                if (message.find("instruction budget") != std::string_view::npos || message.find("exhausted") != std::string_view::npos)
                    return fail(fmt::format("the lazy getter {} did not finish: {}", process.DescribeAddress(getter), message));
                ++result.Stats.LazyGettersFaulted;
                if (std::optional<GuestFault> const& fault = process.GetMachine().GetLastFault())
                {
                    ++faultKinds[fault->Kind];
                    if (fault->Address < Machine::PageSize)
                        ++nullThisFaults;
                }
                else
                    ++faultKinds["no fault recorded"];
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("the lazy getter {} failed: {}", process.DescribeAddress(getter), failure.what()));
            }
        }
        result.Discovered.push_back(fmt::format("{} of {} lazy getters ran, {} faulted", result.Stats.LazyGettersRun, result.Stats.LazyGetters, result.Stats.LazyGettersFaulted));
        for (auto const& [kind, count] : faultKinds)
            result.Discovered.push_back(fmt::format("lazy getter fault: {} x{}", kind, count));
        if (nullThisFaults != 0)
            result.Discovered.push_back(fmt::format("lazy getter fault: {} touched the first page, which is what a getter called with no object does", nullThisFaults));

        std::vector<std::string> const races = ReadRaces(options.ClientDir, error);
        if (races.empty())
            return fail(error);
        progress(fmt::format("adding {} races from {}", races.size(), RaceFile));
        for (std::string const& race : races)
        {
            try
            {
                std::array<uint64, 1> const arguments{ StoreStdString(process, layout, race) };
                process.Call(*raceAdder, arguments, options.RaceBudget);
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("adding the race {} failed: {}", race, failure.what()));
            }
        }
        result.Stats.Races = races.size();

        types = ClientDiscovery::WalkTypeMap(machine, *head, layout, error);
        if (!types)
            return fail(fmt::format("the type map could not be walked: {}", error));
        if (!options.RequireDerivedLayout && !referenceLayout)
        {
            reportLayout();
            if (!RequireDerivedLayout(layout, error))
                return fail(error);
        }
        progress(fmt::format("reading and checking {} types", types->size()));
        TypeWalker walker(process, layout);
        TypeWalkResult walk = walker.Walk(*types);
        result.Stats.WalkMilliseconds = Milliseconds(phase);
        result.Stats.Classes = walk.Dump.Classes.size();
        result.Stats.Properties = walk.PropertyCount;
        result.Stats.DuplicateTypes = walk.DuplicateTypes;
        result.Stats.DuplicateOptions = walk.DuplicateOptions;
        result.Discovered.push_back(fmt::format("{} types after the lazy getters and races, {} registered twice, {} enum options listed twice", types->size(), walk.DuplicateTypes, walk.DuplicateOptions));
        result.Stats.HeapBytes = process.GetHeap().GetTop() - process.GetHeap().GetBase();
        result.ProblemCounts = std::move(walk.ProblemCounts);
        result.ProblemSamples = std::move(walk.ProblemSamples);
        result.Dump = std::move(walk.Dump);
        result.UnhandledApiCalls = process.GetUnhandledApiCalls();

        std::vector<std::string> kernel;
        for (auto const& [call, count] : result.UnhandledApiCalls)
            if (call.starts_with("kernel32.dll!"))
                kernel.push_back(fmt::format("{} ({} calls)", call.substr(13), count));
        if (!kernel.empty())
            return fail(fmt::format("the client called Windows functions the emulator does not provide: {}", fmt::join(kernel, ", ")));
        if (!result.ProblemCounts.empty())
        {
            std::vector<std::string> kinds;
            for (auto const& [kind, count] : result.ProblemCounts)
                kinds.push_back(fmt::format("{} {}", count, kind));
            return fail(fmt::format("the extracted types failed validation: {}", fmt::join(kinds, ", ")));
        }
        if (!CheckRaces(result.Dump, races, error))
            return fail(error);
        progress("building the server's type catalog from the dump");
        if (!CheckCatalog(result.Dump, result.Metadata.ExecutableSha256, error))
            return fail(error);

        if (!options.RequireDerivedLayout && referenceLayout)
        {
            deriveTypeLayout();
            reportLayout();
        }
    }
    catch (std::exception const& failure)
    {
        return fail(failure.what());
    }
    result.Stats.TotalMilliseconds = Milliseconds(started);
    return result;
}

bool TypeExtraction::RequireDerivedLayout(ClientLayout const& layout, std::string& error)
{
    std::optional<std::string> const unresolved = layout.FirstUnresolvedField();
    if (!unresolved)
    {
        error.clear();
        return true;
    }
    error = fmt::format("the client layout could not be derived: {}", *unresolved);
    return false;
}

bool TypeExtraction::SaveDump(TypeExtractionResult const& result, std::filesystem::path const& path, std::string& error)
{
    if (!result.Succeeded())
    {
        error = result.Error;
        return false;
    }
    error.clear();
    return TypeDumpWriter::Save(path, TypeDumpWriter::ToJson(result.Dump, result.Metadata), error);
}

bool TypeExtraction::IsPlainRevision(std::string_view revision)
{
    if (revision.empty() || revision == "." || revision == "..")
        return false;
    return std::all_of(revision.begin(), revision.end(), [](char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
}

std::optional<std::filesystem::path> TypeExtraction::DefaultOutputPath(std::filesystem::path const& dataFolder, std::string_view revision)
{
    if (!IsPlainRevision(revision) || dataFolder.empty() || !dataFolder.is_absolute())
        return std::nullopt;
    return dataFolder / "types" / (std::string(revision) + ".json");
}

bool TypeExtraction::CheckRaces(TypeDumpLoader::RawDump const& dump, std::span<std::string const> races, std::string& error)
{
    for (TypeDumpLoader::RawClass const& rawClass : dump.Classes)
    {
        for (TypeDumpLoader::RawProperty const& property : rawClass.Properties)
        {
            if (property.Type != RaceEnumType)
                continue;
            std::unordered_set<std::string_view> options;
            options.reserve(property.Options.size());
            for (auto const& option : property.Options)
                options.insert(option.first);
            std::string_view firstMissing;
            std::size_t missing = 0;
            for (std::string const& race : races)
            {
                if (options.contains(race))
                    continue;
                if (missing++ == 0)
                    firstMissing = race;
            }
            if (missing != 0)
            {
                error = fmt::format("the races did not land: {} property {} of type {} lacks {} of the {} races from Races.xml, among them {}", rawClass.Name.value_or(rawClass.Key), property.Name,
                    RaceEnumType, missing, races.size(), firstMissing);
                return false;
            }
        }
    }
    return true;
}

bool TypeExtraction::CheckCatalog(TypeDumpLoader::RawDump const& dump, std::string const& sha256, std::string& error)
{
    std::vector<std::string> errors;
    if (TypeCatalogBuilder::Build(dump, "extracted", sha256, 1, {}, errors))
        return true;
    if (errors.empty())
    {
        error = "the server's type loader refuses the extracted dump without naming a reason";
        return false;
    }
    std::size_t const listed = std::min(errors.size(), MaxListedLoaderErrors);
    error = fmt::format("the server's type loader refuses the extracted dump: {}", fmt::join(errors.begin(), errors.begin() + static_cast<std::ptrdiff_t>(listed), "; "));
    if (errors.size() > listed)
        error += fmt::format("; and {} more", errors.size() - listed);
    return false;
}
