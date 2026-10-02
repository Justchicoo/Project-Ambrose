/*
 * Project Ambrose by Imjustchico
 * extractor entry point: silences the log, reads its arguments and environment as UTF-8, refuses an option value that is itself an option and a command named twice, checks the world database, --sql and --dry-run before anything is searched, then when no install or type dump is named follows AMBROSE_SETUP_MODE: auto uses the newest install found and the type dump built from it, ask offers the finds and a build, off prints them with the flag to pass; opens the user's own Root.wad and a type dump bound to the views of every command named, extracts for each command in turn, the character names, disallowed names, schools and creation options for names, the level, school and stat tables for levels, every zone's settings, locations, placed objects, volumes and triggers from its own archive for zones, every template the manifest lists for templates, with each item template's own fields, those two read through the server classes of the world database when one is named, and for classes the classes the install's archives hold that the type dump does not describe, from the class file schemaprobe builds once per revision in the Ambrose data folder on the authored classes the world database holds when one is named, prints their counts, the problems found and the zone parts a type dump could not describe, then replaces every command's world tables in one transaction, writes the SQL to a file, or on a dry run writes nothing and checks the world tables of any database it was given; exits 0 on success, 1 when the install, dump, data or database fails, and 2 on bad usage.
 */

#include "CharacterNameExtractor.h"
#include "DatabaseEnv.h"
#include "ClientSetup.h"
#include "CharacterNameScript.h"
#include "LevelExtractor.h"
#include "LevelScript.h"
#include "LevelViews.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "LogConfig.h"
#include "NameViews.h"
#include "ObjectViews.h"
#include "ServerClassCache.h"
#include "ServerClassScript.h"
#include "TemplateExtractor.h"
#include "TemplateScript.h"
#include "TypedView.h"
#include "ZoneExtractor.h"
#include "ZoneScript.h"
#include "ZoneViews.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <exception>
#include <filesystem>
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
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;

    constexpr std::string_view Usage = R"(Usage: extractor [options] <command>...

Extracts world database rows from your own Wizard101 install. Name several
commands to extract them together; their tables are replaced in one transaction.

Commands:
  names   every character name table in every locale, the disallowed names, and
          the schools and creation options a new wizard is offered
  levels  every school's base stats and experience for each level, the magic
          schools and their badges, the level each mob rank stands for, and the
          stat settings with their crit, block and pip conversion bands
  zones   every zone's settings, named locations and placed objects, read from
          the gamedata.bin of each zone's own archive, and its volumes and
          triggers from its volumes.xml and triggers.xml, read through the
          server classes of the world database named
  templates every template TemplateManifest.xml lists, with its class, names,
          display key, icon, adjectives and behaviors, and each item
          template's school, cost, rank, limit, set bonus and colors, read
          through the server classes of the world database named
  classes the classes every archive of the install holds that the type dump
          does not describe and every object of which then decodes cleanly,
          with the evidence for each; schemaprobe finds them once per client
          revision, on the authored classes of the world database named, and
          keeps them in the Ambrose data folder's classes folder, and only the
          classes marked install are replaced

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
        std::set<std::string> named;
        for (std::string const& word : parsed.Words)
        {
            if (word != "names" && word != "levels" && word != "zones" && word != "templates" && word != "classes")
                error = fmt::format("unknown command '{}'", word);
            else if (!named.insert(word).second)
                error = fmt::format("{} is named twice", word);
            if (!error.empty())
                break;
        }
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

    struct Extracted
    {
        bool Ok = false;
        std::size_t ErrorCount = 0;
        std::vector<std::string> Errors;
        std::vector<WorldSqlScript> Scripts;
        std::vector<std::string_view> Tables;
    };

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

    void PrintCounts(LevelExtraction const& extraction)
    {
        PlayerLevelData const& levels = extraction.Levels;
        std::cout << fmt::format("player_level_stats: {} rows for {} schools\n", levels.Levels.size(), extraction.SchoolsWithTables.size());
        if (!extraction.SchoolsWithTables.empty())
            std::cout << fmt::format("  with level tables: {}\n", fmt::join(extraction.SchoolsWithTables, ", "));
        if (!extraction.SchoolsWithoutTables.empty())
            std::cout << fmt::format("  without: {}\n", fmt::join(extraction.SchoolsWithoutTables, ", "));
        std::size_t badges = 0;
        for (MagicSchool const& school : levels.Schools)
            badges += school.Badges.size();
        std::cout << fmt::format("magic_school_template: {} rows\n", levels.Schools.size());
        std::cout << fmt::format("magic_school_badge: {} rows\n", badges);
        std::cout << fmt::format("magic_xp_config: {} rows\n", levels.XpConfig.size());
        for (ConfigValue const& setting : levels.XpConfig)
            std::cout << fmt::format("  {} {}\n", setting.Name, setting.Value);
        std::cout << fmt::format("magic_xp_encounter_factor: {} rows\n", levels.EncounterXpFactors.size());
        std::cout << fmt::format("mob_rank_level: {} rows\n", levels.MobRanks.size());
        std::cout << fmt::format("stat_effect_config: {} rows\n", extraction.Stats.Settings.size());
        std::cout << fmt::format("stat_crit_block_band: {} rows\n", extraction.Stats.CritAndBlock.size());
        std::cout << fmt::format("stat_pip_conversion_band: {} rows\n", extraction.Stats.PipConversion.size());
    }

    void PrintCounts(ZoneExtraction const& extraction)
    {
        std::cout << fmt::format("zone_template: {} rows from {} archives\n", extraction.Zones.size(), extraction.Archives);
        std::cout << fmt::format("zone_location: {} rows\n", extraction.GetLocationCount());
        std::cout << fmt::format("zone_object: {} rows, {} object list entries left out\n", extraction.GetObjectCount(), extraction.GetSkippedObjectCount());
        std::size_t events = 0;
        std::size_t results = 0;
        std::size_t unknownResults = 0;
        for (ExtractedZone const& zone : extraction.Zones)
        {
            for (ExtractedVolume const& volume : zone.Volumes)
                events += volume.EnterEvents.size() + volume.ExitEvents.size();
            for (ExtractedTrigger const& trigger : zone.Triggers)
            {
                events += trigger.ActivateEvents.size() + trigger.FireEvents.size() + trigger.DeactivateEvents.size();
                for (auto const* list : { &trigger.Results, &trigger.CooldownResults })
                {
                    results += list->size();
                    unknownResults += static_cast<std::size_t>(std::count_if(list->begin(), list->end(), [](ExtractedTriggerResult const& result) { return !result.ClassName; }));
                }
            }
        }
        std::cout << fmt::format("zone_volume: {} rows\n", extraction.GetVolumeCount());
        std::cout << fmt::format("zone_trigger: {} rows\n", extraction.GetTriggerCount());
        std::cout << fmt::format("zone_trigger_event: {} rows\n", events);
        std::cout << fmt::format("zone_trigger_result: {} rows, {} of classes no class the server knows describes\n", results, unknownResults);
        std::cout << fmt::format("zones whose volume or trigger files do not decode: {}\n", extraction.GetTriggerFailureZoneCount());
        for (std::size_t index = 0; index < extraction.TriggerFailures.size() && index < 10; ++index)
            std::cout << fmt::format("  {} {}: {}\n", extraction.TriggerFailures[index].Zone, extraction.TriggerFailures[index].File, extraction.TriggerFailures[index].Detail);
        std::map<uint32, std::size_t> wholeByClass;
        std::map<uint32, std::size_t> partsByClass;
        for (SkippedZonePart const& part : extraction.Skipped)
            ++(part.WholeObject ? wholeByClass : partsByClass)[part.ClassHash];
        for (auto const& [hash, count] : wholeByClass)
            std::cout << fmt::format("  {} entries of class hash {}, which the type dump does not list\n", count, hash);
        for (auto const& [hash, count] : partsByClass)
            std::cout << fmt::format("  {} parts of kept entries of class hash {}, which the type dump does not list\n", count, hash);
    }

    void PrintCounts(TemplateExtraction const& extraction)
    {
        std::cout << fmt::format("object_template: {} rows from {} manifest entries, {} not read\n", extraction.Templates.size(), extraction.ManifestEntries, extraction.UnreadCount);
        std::cout << fmt::format("  ObjectData: {} of {} entries read\n", extraction.ObjectDataRead, extraction.ObjectDataEntries);
        std::cout << fmt::format("  NPC templates: {}\n", extraction.GetNpcTemplateCount());
        std::cout << fmt::format("object_template_adjective: {} rows\n", extraction.GetAdjectiveCount());
        std::cout << fmt::format("object_template_behavior: {} rows, {} of classes no class the server knows describes\n", extraction.GetBehaviorCount(),
            extraction.GetUnknownBehaviorCount());
        for (auto const& [hash, count] : extraction.UnknownBehaviorClasses)
            std::cout << fmt::format("  {} behaviors of class hash {}\n", count, hash);
        std::cout << fmt::format("item_template: {} rows\n", extraction.GetItemCount());
        for (std::string const& problem : extraction.Unread)
            std::cout << fmt::format("  not read: {}\n", problem);
    }

    bool ReadServerClasses(std::optional<std::string> const& worldDatabase, TypeDumpLoader::RawDump& classes, std::optional<std::string_view> source, Extracted& extracted)
    {
        if (!worldDatabase)
        {
            std::cout << (source ? "server_class: no world database is named, so the class file is built without the authored classes\n"
                                 : "no world database is named, so zone triggers, volumes and templates are read without the server classes\n");
            return true;
        }
        std::vector<std::string> errors;
        if (!WorldDatabase.SetConnectionInfo(*worldDatabase, 1, 1) || WorldDatabase.Open() != 0)
            errors.emplace_back("the world database cannot be opened to read its server classes");
        else
        {
            ServerClassScript::Read(classes, errors, source);
            WorldDatabase.Close();
        }
        if (errors.empty())
            return true;
        extracted.ErrorCount += errors.size();
        extracted.Errors.insert(extracted.Errors.end(), errors.begin(), errors.end());
        return false;
    }

    bool CollectClasses(std::string const& client, std::string const& typeDump, std::optional<std::string> const& worldDatabase, Extracted& extracted)
    {
        TypeDumpLoader::RawDump authored;
        if (!ReadServerClasses(worldDatabase, authored, ServerClassScript::AuthoredSource, extracted))
            return false;
        LocalClientSystem const system;
        std::optional<ClientInstall> const install = ClientInstall::Inspect(system, LogConfig::Utf8Path(client));
        std::string error;
        std::optional<std::filesystem::path> classes;
        if (!install)
            error = fmt::format("{} is not a Wizard101 install", client);
        else
        {
            ServerClassCacheOptions options;
            options.DataFolder = ClientLocator::GetDataFolder(system);
            options.Program = ServerClassCache::DefaultProgram(system.GetExecutableDirectory());
            options.Report = [](std::string const& line) { std::cerr << line << '\n'; };
            options.Authored = std::move(authored);
            classes = ServerClassCache::Ensure(*install, LogConfig::Utf8Path(typeDump), options, error);
        }
        TypeDumpLoader::RawDump dump;
        if (!classes || !ServerClassCache::Read(*classes, dump, error))
        {
            ++extracted.ErrorCount;
            extracted.Errors.push_back(error);
            return false;
        }
        dump = ServerClassScript::InstallClasses(dump);
        std::size_t properties = 0;
        for (TypeDumpLoader::RawClass const& type : dump.Classes)
            properties += type.Properties.size();
        std::cout << fmt::format("server_class: {} rows marked install, from {}\n", dump.Classes.size(), ConfigMgr::PathToUtf8(*classes));
        std::cout << fmt::format("server_class_property: {} rows\n", properties);
        extracted.Scripts.push_back(ServerClassScript::Build(dump));
        std::vector<std::string_view> const tables = ServerClassScript::GetTables();
        extracted.Tables.insert(extracted.Tables.end(), tables.begin(), tables.end());
        return true;
    }

    template<typename Script, typename Extraction>
    void Collect(Extraction const& extraction, Extracted& extracted)
    {
        PrintCounts(extraction);
        extracted.ErrorCount += extraction.ErrorCount;
        extracted.Errors.insert(extracted.Errors.end(), extraction.Errors.begin(), extraction.Errors.end());
        if (extraction.Ok())
            extracted.Scripts.push_back(Script::Build(extraction));
        std::vector<std::string_view> const tables = Script::GetTables();
        extracted.Tables.insert(extracted.Tables.end(), tables.begin(), tables.end());
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

        if (!arguments->Client || !arguments->TypeDump)
        {
            LocalClientSystem const system;
            SetupMode const mode = ClientSetup::ModeForTool(system, std::cerr, "extractor");
            std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ToolPrompt(std::cout, mode);
            ClientSetup::ForTool(mode, arguments->Client, &arguments->TypeDump, *prompt, system, ClientSetup::ToolTypeDumps(system, "extractor", std::cerr), "extractor", std::cerr);
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

        std::filesystem::path const rootWad = LogConfig::Utf8Path(*arguments->Client) / "Data" / "GameData" / "Root.wad";
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(rootWad, error);
        if (!archive)
        {
            std::cerr << fmt::format("extractor: cannot open {}: {}\n", ConfigMgr::PathToUtf8(rootWad), error);
            return Failure;
        }
        std::vector<std::string> const& commands = arguments->Words;
        bool const names = std::find(commands.begin(), commands.end(), "names") != commands.end();
        bool const levels = std::find(commands.begin(), commands.end(), "levels") != commands.end();
        bool const zones = std::find(commands.begin(), commands.end(), "zones") != commands.end();
        bool const templates = std::find(commands.begin(), commands.end(), "templates") != commands.end();
        TypedViewRegistry views;
        if (names)
            NameViews::RegisterAll(views);
        if (levels)
            LevelViews::RegisterAll(views);
        if (zones)
            ZoneViews::RegisterAll(views);
        if (templates)
            ObjectViews::RegisterAll(views);
        TypeRegistry registry(&views);
        if (zones || templates)
        {
            TypeDumpLoader::RawDump classes;
            Extracted refused;
            if (!ReadServerClasses(arguments->WorldDatabase, classes, std::nullopt, refused))
            {
                std::cerr << fmt::format("extractor: {}\n", refused.Errors.front());
                return Failure;
            }
            std::vector<std::string> problems;
            if (!classes.Classes.empty() && !registry.SetSupplement(std::move(classes), "the world database's server classes", problems))
            {
                std::cerr << fmt::format("extractor: the world database's server classes cannot join the type dump: {}\n", problems.empty() ? std::string() : problems.front());
                return Failure;
            }
        }
        if (!registry.LoadFromFile(LogConfig::Utf8Path(*arguments->TypeDump)))
        {
            std::cerr << fmt::format("extractor: cannot load the type dump {}\n", *arguments->TypeDump);
            for (std::string const& problem : registry.GetErrors())
                std::cerr << "  " << problem << '\n';
            return Failure;
        }

        Extracted extracted;
        for (std::string const& command : commands)
        {
            if (command == "names")
                Collect<CharacterNameScript>(CharacterNameExtractor::Extract(*archive, registry.GetCatalog()), extracted);
            else if (command == "levels")
                Collect<LevelScript>(LevelExtractor::Extract(*archive, registry.GetCatalog()), extracted);
            else if (command == "classes")
                CollectClasses(*arguments->Client, *arguments->TypeDump, arguments->WorldDatabase, extracted);
            else if (command == "templates")
                Collect<TemplateScript>(TemplateExtractor::Extract(rootWad.parent_path(), registry.GetCatalog()), extracted);
            else
                Collect<ZoneScript>(ZoneExtractor::Extract(rootWad.parent_path(), registry.GetCatalog()), extracted);
        }
        if (extracted.ErrorCount != 0)
        {
            std::cerr << fmt::format("extractor: {} problems; nothing was written\n", extracted.ErrorCount);
            for (std::string const& problem : extracted.Errors)
                std::cerr << "  " << problem << '\n';
            return Failure;
        }
        if (arguments->DryRun)
        {
            if (database)
            {
                if (!WorldSqlScript::CheckTables(*database, extracted.Tables, error))
                {
                    std::cerr << fmt::format("extractor: {}\n", error);
                    return Failure;
                }
                std::cout << fmt::format("checked the world tables in {}\n", database->ToLogString());
            }
            std::cout << "dry run: nothing was written\n";
            return Success;
        }
        WorldSqlScript script;
        for (WorldSqlScript const& part : extracted.Scripts)
            script.Append(part);
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
