/*
 * Project Ambrose by Imjustchico
 * bindecode entry point: silences the log so standard output holds only what it prints, reads its arguments and environment as UTF-8 and writes UTF-8 to the console, offers to use an install or type dump found on the machine when none is named, opens a KIWAD archive of the user's own client and its type dump, then prints the named BINd entries as JSON with their decode issues on standard error, lists entry names containing a pattern, or sweeps every BINd file and reports failures, unknown classes and grouped issues; exits 0 on success, 1 when something cannot be read or decoded or throws, and 2 on bad usage, a sweep counting only files whose root class the type dump does not list as success.
 */

#include "BindSweep.h"
#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "LogConfig.h"
#include "PropertyJson.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;
    constexpr std::size_t ReportedClasses = 50;

    constexpr std::string_view Usage = R"(Usage: bindecode [options] <entry>...
       bindecode [options] --list [pattern]
       bindecode [options] --sweep

Prints BINd entries of a KIWAD archive from your own Wizard101 install as JSON.

Options:
  --client <dir>      the install holding Data/GameData (default: AMBROSE_CLIENT_DIR)
  --wad <file>        an archive in Data/GameData, or a path to one (default: Root.wad)
  --type-dump <file>  the type dump made from that install (default: AMBROSE_TYPE_DUMP_PATH)
  --compact           print each object's JSON on one line
  --threads <count>   threads a sweep decodes on, 1-1024 (default: every hardware thread)
  --list [pattern]    print entry names that contain the pattern
  --sweep             decode every BINd entry and report what does not decode cleanly
  --help              print this text

Exit status: 0 on success, 1 when an entry or archive cannot be read or decoded,
2 on bad usage. A sweep exits 0 when every BINd entry decodes with no issue other
than classes the type dump does not list, or fails only because the dump does not
list its root class.
)";

    struct Arguments
    {
        std::optional<std::string> Client;
        std::string Wad = "Root.wad";
        std::optional<std::string> TypeDump;
        bool Compact = false;
        bool List = false;
        bool Sweep = false;
        bool Help = false;
        unsigned Threads = 0;
        std::vector<std::string> Entries;
    };

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments parsed;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            auto const value = [&](std::string_view option) -> std::optional<std::string>
            {
                if (index + 1 >= args.size())
                {
                    error = fmt::format("{} needs a value", option);
                    return std::nullopt;
                }
                return args[++index];
            };
            if (arg == "--help" || arg == "-h")
                parsed.Help = true;
            else if (arg == "--compact")
                parsed.Compact = true;
            else if (arg == "--sweep")
                parsed.Sweep = true;
            else if (arg == "--list")
                parsed.List = true;
            else if (arg == "--client" || arg == "--wad" || arg == "--type-dump" || arg == "--threads")
            {
                std::optional<std::string> const given = value(arg);
                if (!given)
                    return std::nullopt;
                if (arg == "--client")
                    parsed.Client = *given;
                else if (arg == "--wad")
                    parsed.Wad = *given;
                else if (arg == "--type-dump")
                    parsed.TypeDump = *given;
                else if (std::optional<unsigned> const threads = Ambrose::StringTo<unsigned>(*given); threads && *threads > 0 && *threads <= BindSweep::MaxThreads)
                    parsed.Threads = *threads;
                else
                {
                    error = fmt::format("--threads must be 1-{}, not '{}'", BindSweep::MaxThreads, *given);
                    return std::nullopt;
                }
            }
            else if (arg.starts_with("--"))
            {
                error = fmt::format("unknown option {}", arg);
                return std::nullopt;
            }
            else
                parsed.Entries.push_back(arg);
        }
        if (parsed.Help)
            return parsed;
        if (parsed.List && parsed.Sweep)
        {
            error = "--list and --sweep cannot be combined";
            return std::nullopt;
        }
        if (parsed.List && parsed.Entries.size() > 1)
        {
            error = "--list takes at most one pattern";
            return std::nullopt;
        }
        if (parsed.Sweep && !parsed.Entries.empty())
        {
            error = "--sweep takes no entries";
            return std::nullopt;
        }
        if (!parsed.List && !parsed.Sweep && parsed.Entries.empty())
        {
            error = "name at least one entry, or use --list or --sweep";
            return std::nullopt;
        }
        return parsed;
    }

    std::filesystem::path ResolveWad(Arguments const& arguments)
    {
        std::filesystem::path const given = LogConfig::Utf8Path(arguments.Wad);
        if (given.has_parent_path() || !arguments.Client)
            return given;
        return LogConfig::Utf8Path(*arguments.Client) / "Data" / "GameData" / given;
    }

    int PrintEntries(Arguments const& arguments, KiwadArchive const& archive, TypeCatalogPtr const& catalog)
    {
        int status = Success;
        for (std::string const& name : arguments.Entries)
        {
            KiwadReadResult const read = archive.Read(name);
            if (!read.Succeeded())
            {
                std::cerr << fmt::format("{}: {}\n", name, read.Error);
                status = Failure;
                continue;
            }
            BindReadResult const result = BindFile::Read(catalog, read.Data);
            if (!result.Ok())
            {
                std::cerr << fmt::format("{}: {}: {}\n", name, BindFile::GetStatusName(result.Status), result.Detail);
                status = Failure;
                continue;
            }
            for (DecodeIssue const& issue : result.Decoded.Issues)
                std::cerr << fmt::format("{}: {} at {} ({} bits): {}\n", name, ObjectSerializer::GetIssueName(issue.Kind), issue.Path, issue.Bits, issue.Detail);
            std::cout << PropertyJson::Dump(result.Decoded.Object.get(), arguments.Compact ? -1 : 2) << '\n';
        }
        return status;
    }

    int ListEntries(Arguments const& arguments, KiwadArchive const& archive)
    {
        std::string const pattern = arguments.Entries.empty() ? std::string() : arguments.Entries.front();
        for (KiwadEntry const& entry : archive.GetEntries())
            if (entry.Name.find(pattern) != std::string::npos)
                std::cout << entry.Name << '\n';
        return Success;
    }

    int SweepEntries(Arguments const& arguments, KiwadArchive const& archive, TypeCatalogPtr const& catalog)
    {
        BindSweepReport const report = BindSweep::Run(archive, catalog, arguments.Threads);
        std::cout << fmt::format("Swept {} entries: {} BINd files, {} decoded, {} failed, {} unreadable\n", report.Entries, report.Files, report.Decoded, report.Failures.size(), report.ReadErrors);
        for (BindSweepFailure const& failure : report.Failures)
            std::cout << fmt::format("failed {}: {}: {}\n", failure.File, BindFile::GetStatusName(failure.Status), failure.Detail);
        std::cout << fmt::format("{} classes the type dump does not list\n", report.UnknownClasses.size());
        for (std::size_t index = 0; index < report.UnknownClasses.size() && index < ReportedClasses; ++index)
        {
            BindSweepUnknownClass const& unknown = report.UnknownClasses[index];
            std::cout << fmt::format("class hash {} x{} in {} file(s), first in {} at {}\n", unknown.Hash, unknown.Count, unknown.Files, unknown.FirstFile, unknown.FirstPath);
        }
        if (report.UnknownClasses.size() > ReportedClasses)
            std::cout << fmt::format("and {} more\n", report.UnknownClasses.size() - ReportedClasses);
        std::cout << fmt::format("{} other kinds of issue\n", report.Issues.size());
        for (BindSweepIssue const& issue : report.Issues)
            std::cout << fmt::format("{} with hash {} x{} in {} file(s), first in {} at {}: {}\n", ObjectSerializer::GetIssueName(issue.Kind), issue.Hash, issue.Count, issue.Files, issue.FirstFile, issue.FirstPath, issue.FirstDetail);
        bool const clean = report.ReadErrors == 0 && report.Issues.empty()
            && std::all_of(report.Failures.begin(), report.Failures.end(), [](BindSweepFailure const& failure) { return failure.DecodeStatus == SerializerStatus::UnknownClass; });
        return clean ? Success : Failure;
    }
}

namespace
{
    int Run(std::vector<std::string> const& args)
    {
        std::string error;
        std::optional<Arguments> arguments = Parse(args, error);
        if (!arguments)
        {
            std::cerr << "bindecode: " << error << "\n\n" << Usage;
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << Usage;
            return Success;
        }
        auto const fromEnvironment = [](std::optional<std::string>& value, char const* name)
        {
            if (value)
                return;
            if (std::optional<std::string> found = Ambrose::GetEnv(name); found && !found->empty())
                value = std::move(found);
        };
        fromEnvironment(arguments->Client, "AMBROSE_CLIENT_DIR");
        fromEnvironment(arguments->TypeDump, "AMBROSE_TYPE_DUMP_PATH");
        bool const needsClient = !arguments->Client && !LogConfig::Utf8Path(arguments->Wad).has_parent_path();
        if (needsClient || (!arguments->List && !arguments->TypeDump))
        {
            LocalClientSystem const system;
            std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ToolPrompt(std::cout);
            std::optional<std::string> client = arguments->Client;
            if (!needsClient && !client)
                client = std::string();
            ClientSetup::ForTool(client, arguments->List ? nullptr : &arguments->TypeDump, *prompt, system, "bindecode", std::cerr);
            if (needsClient)
                arguments->Client = client;
        }

        std::filesystem::path const wadPath = ResolveWad(*arguments);
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(wadPath, error);
        if (!archive)
        {
            std::cerr << fmt::format("bindecode: cannot open {}: {}\n", ConfigMgr::PathToUtf8(wadPath), error);
            return Failure;
        }
        if (arguments->List)
            return ListEntries(*arguments, *archive);

        if (!arguments->TypeDump || arguments->TypeDump->empty())
        {
            std::cerr << "bindecode: name the type dump with --type-dump or AMBROSE_TYPE_DUMP_PATH\n";
            return Failure;
        }
        TypeRegistry registry;
        if (!registry.LoadFromFile(LogConfig::Utf8Path(*arguments->TypeDump)))
        {
            std::cerr << fmt::format("bindecode: cannot load the type dump {}\n", *arguments->TypeDump);
            for (std::string const& problem : registry.GetErrors())
                std::cerr << "  " << problem << '\n';
            return Failure;
        }
        TypeCatalogPtr const catalog = registry.GetCatalog();
        return arguments->Sweep ? SweepEntries(*arguments, *archive, catalog) : PrintEntries(*arguments, *archive, catalog);
    }
}

int main(int argc, char** argv)
{
    try
    {
        sLog.SetLoggerLevel("root", LogLevel::Disabled);
        Ambrose::UseUtf8Console();
        return Run(Ambrose::GetArguments(argc, argv));
    }
    catch (std::exception const& error)
    {
        std::cerr << "bindecode: " << error.what() << '\n';
        return Failure;
    }
}
