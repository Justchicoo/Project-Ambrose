/*
 * Project Ambrose by Imjustchico
 * schemaprobe entry point: sweeps the user's own Root.wad or every GameData WAD, BINd files and headerless versionable objects such as a zone's gamedata.bin alike, reports unknown classes with each property an object of one holds and unknown properties, with counts, paths and bit-width distributions, names each unknown class from the client program's own strings read as class names and each property hash through the property oracle from every type and name the loaded registry lists and any candidates given, and writes the report with a draft schema of every property the oracle names one way only, in JSON.
 */

#include "BindSweep.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "LogConfig.h"
#include "PeImage.h"
#include "ProgramStrings.h"
#include "PropertyOracle.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
    using Json = nlohmann::json;
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;

    struct Arguments
    {
        std::optional<std::string> Client;
        std::optional<std::string> TypeDump;
        std::vector<std::string> Wads;
        std::vector<std::string> Candidates;
        std::optional<std::string> Output;
        std::optional<std::string> Program;
        bool AllWads = false;
        bool Help = false;
        unsigned Threads = 0;
    };

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments parsed;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            if (arg == "--help" || arg == "-h")
                parsed.Help = true;
            else if (arg == "--all-wads")
                parsed.AllWads = true;
            else if (arg == "--client" || arg == "--type-dump" || arg == "--wad" || arg == "--candidate" || arg == "--output" || arg == "--threads" || arg == "--program")
            {
                if (index + 1 >= args.size())
                {
                    error = fmt::format("{} needs a value", arg);
                    return std::nullopt;
                }
                std::string const value = args[++index];
                if (value.starts_with("--"))
                {
                    error = fmt::format("{} needs a value, not {}", arg, value);
                    return std::nullopt;
                }
                if (arg == "--client")
                    parsed.Client = value;
                else if (arg == "--type-dump")
                    parsed.TypeDump = value;
                else if (arg == "--wad")
                    parsed.Wads.push_back(value);
                else if (arg == "--candidate")
                    parsed.Candidates.push_back(value);
                else if (arg == "--output")
                    parsed.Output = value;
                else if (arg == "--program")
                    parsed.Program = value;
                else if (auto const threads = Ambrose::StringTo<unsigned>(value); threads && *threads > 0 && *threads <= BindSweep::MaxThreads)
                    parsed.Threads = *threads;
                else
                {
                    error = fmt::format("--threads must be 1-{}", BindSweep::MaxThreads);
                    return std::nullopt;
                }
            }
            else
            {
                error = fmt::format("unknown option {}", arg);
                return std::nullopt;
            }
        }
        return parsed;
    }

    std::filesystem::path Resolve(std::string const& client, std::string const& wad)
    {
        std::filesystem::path const path = LogConfig::Utf8Path(wad);
        return path.has_parent_path() ? path : LogConfig::Utf8Path(client) / "Data" / "GameData" / path;
    }

    std::vector<std::filesystem::path> FindWads(Arguments const& arguments)
    {
        std::vector<std::filesystem::path> paths;
        if (!arguments.Wads.empty())
            for (std::string const& wad : arguments.Wads)
                paths.push_back(Resolve(*arguments.Client, wad));
        else if (arguments.AllWads)
        {
            std::error_code error;
            std::filesystem::path const root = LogConfig::Utf8Path(*arguments.Client) / "Data" / "GameData";
            for (std::filesystem::recursive_directory_iterator it(root, error), end; it != end && !error; it.increment(error))
                if (it->is_regular_file(error) && it->path().extension() == ".wad")
                    paths.push_back(it->path());
        }
        else
            paths.push_back(Resolve(*arguments.Client, "Root.wad"));
        std::sort(paths.begin(), paths.end());
        paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
        return paths;
    }

    using ClassNameIndex = std::unordered_map<uint32, std::vector<std::string>>;

    std::vector<std::string> ClassMatches(TypeCatalog const& catalog, uint32 hash, std::vector<std::string> const& candidates, ClassNameIndex const& program)
    {
        std::vector<std::string> matches;
        for (ClassInfo const* type : catalog.GetClasses())
            if (type->Hash == hash)
                matches.push_back(type->Name);
        for (std::string const& candidate : candidates)
            if (StringHash::KiStringHash(candidate) == hash)
                matches.push_back(candidate);
        if (auto const named = program.find(hash); named != program.end())
            for (std::string const& name : named->second)
                if (std::find(matches.begin(), matches.end(), name) == matches.end())
                    matches.push_back(name);
        return matches;
    }

    std::vector<std::string> PropertyMatches(TypeCatalog const& catalog, uint32 hash, std::vector<std::string> const& candidates)
    {
        std::vector<std::string> matches;
        for (ClassInfo const* type : catalog.GetClasses())
            for (PropertyInfo const& property : type->Properties)
                if (property.Hash == hash)
                    matches.push_back(fmt::format("{}::{}:{}", type->Name, property.Name, property.TypeName));
        for (std::string const& candidate : candidates)
        {
            std::size_t const separator = candidate.find(':');
            if (separator != std::string::npos && StringHash::PropertyHash(candidate.substr(0, separator), candidate.substr(separator + 1)) == hash)
                matches.push_back(candidate);
        }
        return matches;
    }

    Json Bits(std::map<uint64, uint64> const& sizes)
    {
        Json result = Json::object();
        for (auto const& [bits, count] : sizes)
            result[std::to_string(bits)] = count;
        return result;
    }

    void Merge(BindSweepUnknownClass& into, BindSweepUnknownClass const& item)
    {
        if (into.Hash == 0)
            into = item;
        else
        {
            into.Count += item.Count;
            into.Files += item.Files;
        }
    }

    void Merge(BindSweepClassProperty& into, BindSweepClassProperty const& item)
    {
        if (into.Hash == 0)
            into = item;
        else
        {
            into.Count += item.Count;
            into.Files += item.Files;
            for (auto const& [bits, count] : item.BitSizes)
                into.BitSizes[bits] += count;
        }
    }

    Json Guesses(std::vector<PropertyGuess> const& guesses)
    {
        Json list = Json::array();
        for (PropertyGuess const& guess : guesses)
            list.push_back(fmt::format("{}:{}{}", guess.Type, guess.Name, guess.Known ? " (known)" : ""));
        return list;
    }

    void Merge(BindSweepIssue& into, BindSweepIssue const& item)
    {
        if (into.Hash == 0)
            into = item;
        else
        {
            into.Count += item.Count;
            into.Files += item.Files;
            for (auto const& [bits, count] : item.BitSizes)
                into.BitSizes[bits] += count;
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        sLog.SetLoggerLevel("root", LogLevel::Disabled);
        Ambrose::UseUtf8Console();
        std::string error;
        std::optional<Arguments> arguments = Parse(Ambrose::GetArguments(argc, argv), error);
        if (!arguments)
        {
            std::cerr << "schemaprobe: " << error << '\n';
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << "Usage: schemaprobe --client <dir> --type-dump <file> [--wad <file>] [--all-wads] [--candidate <class-or-type:name>] [--program <exe>] [--output <file>] [--threads <count>]\n";
            return Success;
        }
        if (!arguments->Client)
            arguments->Client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
        if (!arguments->TypeDump)
            arguments->TypeDump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
        if (!arguments->Client || !arguments->TypeDump)
        {
            std::cerr << "schemaprobe: --client and --type-dump or their environment variables are required\n";
            return Failure;
        }

        TypeRegistry registry;
        if (!registry.LoadFromFile(LogConfig::Utf8Path(*arguments->TypeDump)))
            return Failure;
        TypeCatalogPtr const catalog = registry.GetCatalog();
        std::vector<std::string> classCandidates;
        std::vector<std::string> extraNames;
        std::vector<std::string> extraTypes;
        for (std::string const& candidate : arguments->Candidates)
        {
            std::size_t const separator = candidate.rfind(':');
            if (separator != std::string::npos && separator > 0 && candidate[separator - 1] != ':')
            {
                extraTypes.push_back(candidate.substr(0, separator));
                extraNames.push_back(candidate.substr(separator + 1));
            }
            else if (candidate.starts_with("m_"))
                extraNames.push_back(candidate);
            else
                classCandidates.push_back(candidate);
        }
        PropertyOracle const oracle(*catalog, extraNames, extraTypes);
        std::filesystem::path const programPath = arguments->Program ? LogConfig::Utf8Path(*arguments->Program) : LogConfig::Utf8Path(*arguments->Client) / "Bin" / "WizardGraphicalClient.exe";
        ClassNameIndex programNames;
        std::size_t programStrings = 0;
        if (std::unique_ptr<PeImage> const image = PeImage::Load(programPath, error))
        {
            ProgramStrings const strings(*image);
            programStrings = strings.Size();
            programNames = strings.ClassNames();
        }
        else
            std::cerr << fmt::format("schemaprobe: {} cannot be read, so no class is named from the client program's strings: {}\n", ConfigMgr::PathToUtf8(programPath), error);
        Json report{ { "tool", "schemaprobe" }, { "client", *arguments->Client }, { "wads", Json::array() }, { "unknown_classes", Json::array() }, { "unknown_properties", Json::array() },
            { "draft_schema", Json::array() } };
        std::map<uint32, BindSweepUnknownClass> classes;
        std::map<std::pair<uint32, uint32>, BindSweepClassProperty> classProperties;
        std::map<uint32, BindSweepIssue> properties;
        uint64 entries = 0;
        uint64 files = 0;
        uint64 decoded = 0;
        uint64 headerless = 0;
        uint64 headerlessDecoded = 0;
        for (std::filesystem::path const& path : FindWads(*arguments))
        {
            std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(path, error);
            if (!archive)
            {
                std::cerr << fmt::format("schemaprobe: cannot open {}: {}\n", ConfigMgr::PathToUtf8(path), error);
                return Failure;
            }
            BindSweepReport const sweep = BindSweep::Run(*archive, catalog, arguments->Threads);
            entries += sweep.Entries;
            files += sweep.Files;
            decoded += sweep.Decoded;
            headerless += sweep.Headerless;
            headerlessDecoded += sweep.HeaderlessDecoded;
            report["wads"].push_back({ { "path", ConfigMgr::PathToUtf8(path) }, { "entries", sweep.Entries }, { "bind_files", sweep.Files }, { "decoded", sweep.Decoded },
                { "headerless", sweep.Headerless }, { "headerless_decoded", sweep.HeaderlessDecoded }, { "failed", sweep.Failures.size() }, { "unreadable", sweep.ReadErrors } });
            for (BindSweepUnknownClass const& item : sweep.UnknownClasses)
                Merge(classes[item.Hash], item);
            for (BindSweepClassProperty const& item : sweep.ClassProperties)
                Merge(classProperties[{ item.Owner, item.Hash }], item);
            for (BindSweepIssue const& item : sweep.Issues)
                Merge(properties[item.Hash], item);
        }
        std::vector<BindSweepUnknownClass> sortedClasses;
        for (auto const& [hash, item] : classes)
            sortedClasses.push_back(item);
        std::sort(sortedClasses.begin(), sortedClasses.end(), [](auto const& left, auto const& right) { return left.Count != right.Count ? left.Count > right.Count : left.Hash < right.Hash; });
        for (BindSweepUnknownClass const& item : sortedClasses)
        {
            uint32 const hash = item.Hash;
            Json members = Json::array();
            Json draft = Json::array();
            Json unresolved = Json::array();
            for (auto const& [key, property] : classProperties)
            {
                if (key.first != hash)
                    continue;
                std::vector<PropertyGuess> const guesses = oracle.Guess(property.Hash);
                members.push_back({ { "hash", property.Hash }, { "count", property.Count }, { "files", property.Files }, { "bit_sizes", Bits(property.BitSizes) },
                    { "guesses", Guesses(guesses) } });
                if (guesses.size() == 1)
                    draft.push_back({ { "hash", property.Hash }, { "name", guesses.front().Name }, { "type", guesses.front().Type } });
                else
                    unresolved.push_back(property.Hash);
            }
            report["unknown_classes"].push_back({ { "hash", hash }, { "count", item.Count }, { "files", item.Files }, { "first_file", item.FirstFile }, { "first_path", item.FirstPath },
                { "matches", ClassMatches(*catalog, hash, classCandidates, programNames) }, { "properties", std::move(members) } });
            report["draft_schema"].push_back({ { "class_hash", hash }, { "properties", std::move(draft) }, { "unresolved", std::move(unresolved) } });
        }
        std::vector<BindSweepIssue> sortedProperties;
        for (auto const& [hash, item] : properties)
            if (item.Kind == DecodeIssueKind::UnknownProperty)
                sortedProperties.push_back(item);
        std::sort(sortedProperties.begin(), sortedProperties.end(), [](auto const& left, auto const& right) { return left.Count != right.Count ? left.Count > right.Count : left.Hash < right.Hash; });
        for (BindSweepIssue const& item : sortedProperties)
        {
            uint32 const hash = item.Hash;
            report["unknown_properties"].push_back({ { "hash", hash }, { "count", item.Count }, { "files", item.Files }, { "first_file", item.FirstFile }, { "first_path", item.FirstPath },
                { "bit_sizes", Bits(item.BitSizes) }, { "matches", PropertyMatches(*catalog, hash, arguments->Candidates) }, { "guesses", Guesses(oracle.Guess(hash)) } });
        }
        report["summary"] = { { "entries", entries }, { "bind_files", files }, { "decoded", decoded }, { "headerless", headerless }, { "headerless_decoded", headerlessDecoded },
            { "oracle_names", oracle.GetNameCount() }, { "oracle_types", oracle.GetTypeCount() }, { "program_strings", programStrings } };
        std::string const text = report.dump(2) + '\n';
        if (arguments->Output)
        {
            std::ofstream output(LogConfig::Utf8Path(*arguments->Output), std::ios::binary);
            if (!output)
            {
                std::cerr << "schemaprobe: cannot open output file\n";
                return Failure;
            }
            output << text;
        }
        std::cout << text;
        return Success;
    }
    catch (std::exception const& exception)
    {
        std::cerr << "schemaprobe: " << exception.what() << '\n';
        return Failure;
    }
}
