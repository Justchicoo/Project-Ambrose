/*
 * Project Ambrose by Imjustchico
 * extractor entry point: silences the log, reads its arguments and environment as UTF-8, refuses an option value that is itself an option, offers to use an install or type dump found on the machine when none is named, opens the user's own Root.wad and a type dump bound to the extractor's views, extracts the character names, disallowed names, schools and creation options, prints their counts and the problems found, then replaces the world tables in one transaction, writes the SQL to a file, or on a dry run writes nothing and checks the world tables of any database it was given; exits 0 on success, 1 when the install, dump, data or database fails, and 2 on bad usage.
 */

#include "CharacterNameExtractor.h"
#include "ClientSetup.h"
#include "CharacterNameScript.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "LogConfig.h"
#include "NameViews.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;

    constexpr std::string_view Usage = R"(Usage: extractor [options] names

Extracts world database rows from your own Wizard101 install.

Commands:
  names   every character name table in every locale, the disallowed names, and
          the schools and creation options a new wizard is offered

Options:
  --client <dir>      the install holding Data/GameData (default: AMBROSE_CLIENT_DIR)
  --type-dump <file>  the type dump made from that install (default: AMBROSE_TYPE_DUMP_PATH)
  --world-db <info>   the world database as "host;port;user;password;database",
                      optionally followed by ";tls;ca-file"
                      (default: AMBROSE_WORLD_DATABASE_INFO)
  --sql <file>        write the SQL to this file instead of the database
  --dry-run           extract and check everything, print the counts, write nothing;
                      checks the world tables when a world database is named
  --help              print this text
  --                  treat every later argument as a command word

The database must already hold the world tables; dbimport creates them. The rows
replace the tables in one transaction, so a failure changes nothing.

Exit status: 0 on success, 1 when the install, type dump, extracted data or
database fails, 2 on bad usage.
)";

    struct Arguments
    {
        std::optional<std::string> Client;
        std::optional<std::string> TypeDump;
        std::optional<std::string> WorldDatabase;
        std::optional<std::string> SqlFile;
        bool DryRun = false;
        bool Help = false;
        std::vector<std::string> Words;
    };

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments parsed;
        bool optionsEnded = false;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            if (optionsEnded)
            {
                parsed.Words.push_back(arg);
                continue;
            }
            if (arg == "--")
                optionsEnded = true;
            else if (arg == "--help" || arg == "-h")
                parsed.Help = true;
            else if (arg == "--dry-run")
                parsed.DryRun = true;
            else if (arg == "--client" || arg == "--type-dump" || arg == "--world-db" || arg == "--sql")
            {
                if (index + 1 >= args.size())
                {
                    error = fmt::format("{} needs a value", arg);
                    return std::nullopt;
                }
                std::string const& value = args[++index];
                if (value.starts_with("--"))
                {
                    error = fmt::format("{} needs a value, not the option {}", arg, value);
                    return std::nullopt;
                }
                (arg == "--client" ? parsed.Client : arg == "--type-dump" ? parsed.TypeDump : arg == "--world-db" ? parsed.WorldDatabase : parsed.SqlFile) = value;
            }
            else if (arg.starts_with("--"))
            {
                error = fmt::format("unknown option {}", arg);
                return std::nullopt;
            }
            else
                parsed.Words.push_back(arg);
        }
        if (parsed.Help)
            return parsed;
        if (parsed.Words.empty())
            error = "name a command";
        else if (parsed.Words.front() != "names")
            error = fmt::format("unknown command '{}'", parsed.Words.front());
        else if (parsed.Words.size() > 1)
            error = "names takes no arguments";
        if (!error.empty())
            return std::nullopt;
        if (parsed.DryRun && parsed.SqlFile)
        {
            error = "--dry-run and --sql cannot be combined";
            return std::nullopt;
        }
        return parsed;
    }

    void FromEnvironment(std::optional<std::string>& value, char const* name)
    {
        if (value)
            return;
        if (std::optional<std::string> found = Ambrose::GetEnv(name); found && !found->empty())
            value = std::move(found);
    }

    void PrintCounts(NameExtraction const& extraction)
    {
        std::map<std::string, std::vector<std::string>> tables;
        for (CharacterNameTable const& table : extraction.Tables)
            tables[table.Name].push_back(fmt::format("{} {}", table.Locale, table.Parts.size()));
        std::cout << fmt::format("character_name_part: {} rows in {} tables\n", extraction.GetPartCount(), extraction.Tables.size());
        for (auto const& [name, locales] : tables)
            std::cout << fmt::format("  {}: {}\n", name, fmt::join(locales, ", "));
        std::cout << fmt::format("character_name_disallowed: {} rows\n", extraction.Disallowed.size());
        std::cout << fmt::format("character_create_school: {} rows\n", extraction.Schools.size());
        for (CreationSchool const& school : extraction.Schools)
            std::cout << fmt::format("  {} {}\n", school.Name, school.Id);
        std::cout << fmt::format("character_create_option: {} rows\n", extraction.Options.size());
    }

    int Run(std::vector<std::string> const& args)
    {
        std::string error;
        std::optional<Arguments> arguments = Parse(args, error);
        if (!arguments)
        {
            std::cerr << "extractor: " << error << "\n\n" << Usage;
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << Usage;
            return Success;
        }
        FromEnvironment(arguments->Client, "AMBROSE_CLIENT_DIR");
        FromEnvironment(arguments->TypeDump, "AMBROSE_TYPE_DUMP_PATH");
        FromEnvironment(arguments->WorldDatabase, "AMBROSE_WORLD_DATABASE_INFO");
        if (!arguments->Client || !arguments->TypeDump)
        {
            LocalClientSystem const system;
            std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ToolPrompt(std::cout);
            ClientSetup::ForTool(arguments->Client, &arguments->TypeDump, *prompt, system, "extractor", std::cerr);
        }

        if (!arguments->Client)
        {
            std::cerr << "extractor: name your install with --client or AMBROSE_CLIENT_DIR\n";
            return Failure;
        }
        if (!arguments->TypeDump)
        {
            std::cerr << "extractor: name the type dump with --type-dump or AMBROSE_TYPE_DUMP_PATH\n";
            return Failure;
        }
        std::optional<MySQLConnectionInfo> database;
        if (!arguments->SqlFile && (!arguments->DryRun || arguments->WorldDatabase))
        {
            if (!arguments->WorldDatabase)
            {
                std::cerr << "extractor: name the world database with --world-db or AMBROSE_WORLD_DATABASE_INFO, or use --sql or --dry-run\n";
                return Failure;
            }
            database = MySQLConnectionInfo::Parse(*arguments->WorldDatabase, &error);
            if (!database)
            {
                std::cerr << fmt::format("extractor: the world database connection is not usable: {}\n", error);
                return Failure;
            }
        }

        std::filesystem::path const rootWad = LogConfig::Utf8Path(*arguments->Client) / "Data" / "GameData" / "Root.wad";
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(rootWad, error);
        if (!archive)
        {
            std::cerr << fmt::format("extractor: cannot open {}: {}\n", ConfigMgr::PathToUtf8(rootWad), error);
            return Failure;
        }
        TypedViewRegistry views;
        NameViews::RegisterAll(views);
        TypeRegistry registry(&views);
        if (!registry.LoadFromFile(LogConfig::Utf8Path(*arguments->TypeDump)))
        {
            std::cerr << fmt::format("extractor: cannot load the type dump {}\n", *arguments->TypeDump);
            for (std::string const& problem : registry.GetErrors())
                std::cerr << "  " << problem << '\n';
            return Failure;
        }

        NameExtraction const extraction = CharacterNameExtractor::Extract(*archive, registry.GetCatalog());
        PrintCounts(extraction);
        if (!extraction.Ok())
        {
            std::cerr << fmt::format("extractor: {} problems; nothing was written\n", extraction.ErrorCount);
            for (std::string const& problem : extraction.Errors)
                std::cerr << "  " << problem << '\n';
            return Failure;
        }
        if (arguments->DryRun)
        {
            if (database)
            {
                if (!WorldSqlScript::CheckTables(*database, CharacterNameScript::GetTables(), error))
                {
                    std::cerr << fmt::format("extractor: {}\n", error);
                    return Failure;
                }
                std::cout << fmt::format("checked the world tables in {}\n", database->ToLogString());
            }
            std::cout << "dry run: nothing was written\n";
            return Success;
        }
        WorldSqlScript const script = CharacterNameScript::Build(extraction);
        if (arguments->SqlFile)
        {
            std::filesystem::path const file = LogConfig::Utf8Path(*arguments->SqlFile);
            if (!script.WriteFile(file, error))
            {
                std::cerr << fmt::format("extractor: cannot write {}: {}\n", *arguments->SqlFile, error);
                return Failure;
            }
            std::cout << fmt::format("wrote {} statements to {}\n", script.GetStatements().size(), *arguments->SqlFile);
            return Success;
        }
        if (!script.Apply(*database, error))
        {
            std::cerr << fmt::format("extractor: {}\n", error);
            return Failure;
        }
        std::cout << fmt::format("replaced the world tables in {}\n", database->ToLogString());
        return Success;
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
        std::cerr << "extractor: " << error.what() << '\n';
        return Failure;
    }
}
