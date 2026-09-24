/*
 * Project Ambrose by Imjustchico
 * schemaprobe entry point: sweeps the user's own Root.wad or every GameData WAD, reports unknown classes and properties with counts, paths and bit-width distributions, matches hashes against the loaded registry and candidate names, and writes a draft schema report in JSON.
 */

#include "BindSweep.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "LogConfig.h"
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
            else if (arg == "--client" || arg == "--type-dump" || arg == "--wad" || arg == "--candidate" || arg == "--output" || arg == "--threads")
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

    std::vector<std::string> ClassMatches(TypeCatalog const& catalog, uint32 hash, std::vector<std::string> const& candidates)
    {
        std::vector<std::string> matches;
        for (ClassInfo const* type : catalog.GetClasses())
            if (type->Hash == hash)
                matches.push_back(type->Name);
        for (std::string const& candidate : candidates)
            if (StringHash::KiStringHash(candidate) == hash)
                matches.push_back(candidate);
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
            std::cout << "Usage: schemaprobe --client <dir> --type-dump <file> [--wad <file>] [--all-wads] [--candidate <class-or-type:name>] [--output <file>] [--threads <count>]\n";
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
        Json report{ { "tool", "schemaprobe" }, { "client", *arguments->Client }, { "wads", Json::array() }, { "unknown_classes", Json::array() }, { "unknown_properties", Json::array() } };
        std::map<uint32, BindSweepUnknownClass> classes;
        std::map<uint32, BindSweepIssue> properties;
        uint64 entries = 0;
        uint64 files = 0;
        uint64 decoded = 0;
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
            report["wads"].push_back({ { "path", ConfigMgr::PathToUtf8(path) }, { "entries", sweep.Entries }, { "bind_files", sweep.Files }, { "decoded", sweep.Decoded }, { "failed", sweep.Failures.size() }, { "unreadable", sweep.ReadErrors } });
            for (BindSweepUnknownClass const& item : sweep.UnknownClasses)
                Merge(classes[item.Hash], item);
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
            report["unknown_classes"].push_back({ { "hash", hash }, { "count", item.Count }, { "files", item.Files }, { "first_file", item.FirstFile }, { "first_path", item.FirstPath }, { "matches", ClassMatches(*catalog, hash, arguments->Candidates) } });
        }
        std::vector<BindSweepIssue> sortedProperties;
        for (auto const& [hash, item] : properties)
            if (item.Kind == DecodeIssueKind::UnknownProperty)
                sortedProperties.push_back(item);
        std::sort(sortedProperties.begin(), sortedProperties.end(), [](auto const& left, auto const& right) { return left.Count != right.Count ? left.Count > right.Count : left.Hash < right.Hash; });
        for (BindSweepIssue const& item : sortedProperties)
        {
            uint32 const hash = item.Hash;
            report["unknown_properties"].push_back({ { "hash", hash }, { "count", item.Count }, { "files", item.Files }, { "first_file", item.FirstFile }, { "first_path", item.FirstPath }, { "bit_sizes", Bits(item.BitSizes) }, { "matches", PropertyMatches(*catalog, hash, arguments->Candidates) } });
        }
        report["summary"] = { { "entries", entries }, { "bind_files", files }, { "decoded", decoded } };
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
