/*
 * Project Ambrose by Imjustchico
 * schemaprobe entry point: sweeps the user's own Root.wad or every GameData WAD, BINd files and headerless versionable objects such as a zone's gamedata.bin alike, and every plain-XML object file, reports unknown classes with each property an object of one holds and unknown properties, with counts, paths and bit-width distributions, and the XML files' unknown classes with each element their objects hold, how often, whether repeated or keyed, the classes it holds and its distinct values, names each unknown class from the client program's own strings read as class names and each property hash through the property oracle from every type and name the loaded registry lists, every property name the client program holds or prints and every one the swept archives' text files spell out, and any candidates given, and writes the report with a draft schema of every property the oracle names one way only, in JSON. Given --server-classes it first finds the classes those archives hold, in BINd and XML files alike, that every object of reads cleanly with and writes them, with the evidence for each, the sweeps before and after and the version of the way they were found, as a class file in the type dump's own format, which --supplement loads back before a sweep, so the unknown classes it removes are seen gone; with no --output as well, the class file is all it writes.
 */

#include "ArchiveText.h"
#include "BindSweep.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "LogConfig.h"
#include "PeImage.h"
#include "ProgramStrings.h"
#include "PropertyOracle.h"
#include "ServerClassCache.h"
#include "ServerClassExtractor.h"
#include "StringHash.h"
#include "TypeDumpLoader.h"
#include "TypeRegistry.h"
#include "XmlSweep.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace
{
    using Json = nlohmann::json;
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;
    constexpr std::size_t XmlExamples = 5;

    struct Arguments
    {
        std::optional<std::string> Client;
        std::optional<std::string> TypeDump;
        std::vector<std::string> Wads;
        std::vector<std::string> Candidates;
        std::optional<std::string> Output;
        std::optional<std::string> Program;
        std::optional<std::string> ServerClasses;
        std::optional<std::string> Supplement;
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
            else if (arg == "--client" || arg == "--type-dump" || arg == "--wad" || arg == "--candidate" || arg == "--output" || arg == "--threads" || arg == "--program"
                || arg == "--server-classes" || arg == "--supplement")
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
                else if (arg == "--server-classes")
                    parsed.ServerClasses = value;
                else if (arg == "--supplement")
                    parsed.Supplement = value;
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

    Json ClassesJson(std::vector<ServerClassProposal> const& classes)
    {
        Json list = Json::object();
        for (ServerClassProposal const& proposal : classes)
        {
            TypeDumpLoader::RawClass const& type = proposal.Class;
            Json properties = Json::object();
            for (TypeDumpLoader::RawProperty const& property : type.Properties)
            {
                Json entry{ { "type", property.Type.value_or("") }, { "id", property.Id.value_or(0) }, { "offset", property.Offset.value_or(0) }, { "flags", property.Flags.value_or(0) },
                    { "container", property.Container.value_or("Static") }, { "dynamic", property.Dynamic.value_or(false) }, { "singleton", property.Singleton.value_or(false) },
                    { "pointer", property.Pointer.value_or(false) }, { "hash", property.Hash.value_or(0) } };
                if (!property.Options.empty())
                {
                    Json options = Json::object();
                    for (auto const& [name, value] : property.Options)
                        options[name] = std::holds_alternative<int64>(value) ? Json(std::get<int64>(value)) : Json(std::get<std::string>(value));
                    entry["enum_options"] = std::move(options);
                }
                properties[property.Name] = std::move(entry);
            }
            list[type.Key] = { { "name", type.Name.value_or("") }, { "hash", type.Hash.value_or(0) }, { "bases", type.Bases }, { "evidence", proposal.Evidence },
                { "properties", std::move(properties) } };
        }
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
            std::cout << "Usage: schemaprobe --client <dir> --type-dump <file> [--wad <file>] [--all-wads] [--candidate <class-or-type:name>] [--program <exe>] [--output <file>] [--threads <count>] "
                         "[--server-classes <file>] [--supplement <file>]\n";
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
        TypeDumpLoader::RawDump existing;
        if (arguments->Supplement)
        {
            std::ifstream input(LogConfig::Utf8Path(*arguments->Supplement), std::ios::binary);
            std::string const text{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
            std::vector<std::string> errors;
            if (!input || !TypeDumpLoader::Parse(text, existing, errors) || !registry.SetSupplement(existing, *arguments->Supplement, errors))
            {
                std::cerr << fmt::format("schemaprobe: cannot load the classes in {}: {}\n", *arguments->Supplement, errors.empty() ? std::string("it cannot be read") : errors.front());
                return Failure;
            }
        }
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
        std::filesystem::path const programPath = arguments->Program ? LogConfig::Utf8Path(*arguments->Program) : LogConfig::Utf8Path(*arguments->Client) / "Bin" / "WizardGraphicalClient.exe";
        ClassNameIndex programNames;
        std::size_t programStrings = 0;
        std::size_t programProperties = 0;
        if (std::unique_ptr<PeImage> const image = PeImage::Load(programPath, error))
        {
            ProgramStrings const strings(*image);
            programStrings = strings.Size();
            programNames = strings.ClassNames();
            std::vector<std::string> const properties = strings.PropertyNames();
            programProperties = properties.size();
            extraNames.insert(extraNames.end(), properties.begin(), properties.end());
        }
        else
            std::cerr << fmt::format("schemaprobe: {} cannot be read, so no class or property is named from the client program's strings: {}\n", ConfigMgr::PathToUtf8(programPath), error);
        std::size_t textProperties = 0;
        for (std::filesystem::path const& path : FindWads(*arguments))
            if (std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error))
            {
                std::vector<std::string> const written = ArchiveText::PropertyNames(*archive);
                textProperties += written.size();
                extraNames.insert(extraNames.end(), written.begin(), written.end());
            }
        PropertyOracle const oracle(*registry.GetCatalog(), extraNames, extraTypes);
        if (arguments->ServerClasses)
        {
            std::vector<std::filesystem::path> const archives = FindWads(*arguments);
            ServerClassExtractorOptions options;
            options.Threads = arguments->Threads;
            options.Progress = [](uint32 round, std::size_t kept) { std::cerr << fmt::format("schemaprobe: round {} keeps {} class(es)\n", round, kept); };
            ServerClassExtraction const found = ServerClassExtractor::Run(archives, registry, existing, oracle, programNames, options);
            for (std::string const& problem : found.Errors)
                std::cerr << "schemaprobe: " << problem << '\n';
            if (!found.Ok())
                return Failure;
            Json refused = Json::array();
            for (ServerClassRefusal const& refusal : found.Refused)
                refused.push_back({ { "hash", refusal.Hash }, { "reason", refusal.Reason } });
            Json const classes{ { "version", TypeDumpLoader::SupportedVersion }, { std::string(ServerClassCache::ExtractionKey), ServerClassCache::ExtractionVersion }, { "tool", "schemaprobe" },
                { "client", *arguments->Client }, { "classes", ClassesJson(found.Classes) },
                { "summary", { { "kept", found.Classes.size() }, { "rounds", found.Rounds }, { "unknown_before", found.UnknownBefore }, { "unknown_after", found.UnknownAfter },
                                 { "objects_before", found.ObjectsBefore }, { "objects_after", found.ObjectsAfter }, { "failures_before", found.FailuresBefore },
                                 { "failures_after", found.FailuresAfter }, { "xml_documents", found.XmlDocuments }, { "xml_unknown_before", found.XmlUnknownBefore },
                                 { "xml_unknown_after", found.XmlUnknownAfter }, { "xml_objects_before", found.XmlObjectsBefore }, { "xml_objects_after", found.XmlObjectsAfter },
                                 { "xml_failures_before", found.XmlFailuresBefore }, { "xml_failures_after", found.XmlFailuresAfter } } },
                { "refused", std::move(refused) } };
            std::ofstream output(LogConfig::Utf8Path(*arguments->ServerClasses), std::ios::binary);
            if (!output || !(output << classes.dump(2) << '\n'))
            {
                std::cerr << fmt::format("schemaprobe: cannot write {}\n", *arguments->ServerClasses);
                return Failure;
            }
            std::cerr << fmt::format("schemaprobe: kept {} class(es) after {} round(s): {} unknown class(es) with {} object(s) before, {} with {} after; {} file(s) failed before, {} after\n",
                found.Classes.size(), found.Rounds, found.UnknownBefore, found.ObjectsBefore, found.UnknownAfter, found.ObjectsAfter, found.FailuresBefore, found.FailuresAfter);
            std::cerr << fmt::format("schemaprobe: of {} XML object file(s), {} unknown class(es) with {} object(s) before, {} with {} after; {} file(s) refused before, {} after\n",
                found.XmlDocuments, found.XmlUnknownBefore, found.XmlObjectsBefore, found.XmlUnknownAfter, found.XmlObjectsAfter, found.XmlFailuresBefore, found.XmlFailuresAfter);
            if (!arguments->Output)
                return Success;
        }
        TypeCatalogPtr const catalog = registry.GetCatalog();
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
        XmlSweepReport xml;
        for (std::filesystem::path const& path : FindWads(*arguments))
        {
            std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(path, error);
            if (!archive)
            {
                std::cerr << fmt::format("schemaprobe: cannot open {}: {}\n", ConfigMgr::PathToUtf8(path), error);
                return Failure;
            }
            XmlSweepReport xmlSweep = XmlSweep::Run(*archive, catalog);
            for (XmlSweepFailure& failure : xmlSweep.Failures)
                failure.File = ConfigMgr::PathToUtf8(path.filename()) + ":" + failure.File;
            XmlSweep::Merge(xml, std::move(xmlSweep));
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
        Json xmlClasses = Json::array();
        for (XmlSweepClass const& seen : xml.UnknownClasses)
        {
            Json holders = Json::array();
            for (auto const& [holderClass, holderProperty] : seen.Holders)
                holders.push_back({ { "class", holderClass }, { "property", holderProperty } });
            Json elements = Json::array();
            for (XmlSweepProperty const& element : seen.Properties)
            {
                Json examples = Json::array();
                for (std::string const& value : element.Values)
                {
                    if (examples.size() == XmlExamples)
                        break;
                    examples.push_back(value);
                }
                elements.push_back({ { "name", element.Name }, { "count", element.Count }, { "empty", element.Empty }, { "repeats", element.Repeats }, { "keyed", element.Keyed },
                    { "mixed", element.Mixed }, { "held", Json(std::vector<std::string>(element.Held.begin(), element.Held.end())) }, { "distinct_values", element.Values.size() },
                    { "values_overflow", element.Overflow }, { "examples", std::move(examples) } });
            }
            xmlClasses.push_back({ { "name", seen.Name }, { "hash", StringHash::KiStringHash(seen.Name) }, { "count", seen.Count }, { "files", seen.Files }, { "first_file", seen.FirstFile },
                { "first_path", seen.FirstPath }, { "at_root", seen.AtRoot }, { "holders", std::move(holders) }, { "elements", std::move(elements) } });
        }
        Json xmlFailures = Json::array();
        for (XmlSweepFailure const& failure : xml.Failures)
            xmlFailures.push_back({ { "file", failure.File }, { "status", XmlObjectReader::GetStatusName(failure.Status) }, { "detail", failure.Detail } });
        Json xmlIssues = Json::array();
        for (BindSweepIssue const& issue : xml.Issues)
            xmlIssues.push_back({ { "kind", ObjectSerializer::GetIssueName(issue.Kind) }, { "hash", issue.Hash }, { "owner", issue.Owner }, { "count", issue.Count }, { "files", issue.Files },
                { "first_file", issue.FirstFile }, { "first_path", issue.FirstPath }, { "first_detail", issue.FirstDetail } });
        report["xml"] = { { "unknown_classes", std::move(xmlClasses) }, { "failures", std::move(xmlFailures) }, { "issues", std::move(xmlIssues) } };
        report["summary"] = { { "entries", entries }, { "bind_files", files }, { "decoded", decoded }, { "headerless", headerless }, { "headerless_decoded", headerlessDecoded },
            { "xml_entries", xml.Entries }, { "xml_documents", xml.Documents }, { "xml_read", xml.Read }, { "xml_refused", xml.Failures.size() },
            { "oracle_names", oracle.GetNameCount() }, { "oracle_types", oracle.GetTypeCount() }, { "program_strings", programStrings }, { "program_properties", programProperties }, { "text_properties", textProperties },
            { "supplement_classes", registry.GetSupplementClassCount() } };
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
