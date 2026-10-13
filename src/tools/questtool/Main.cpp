/*
 * Project Ambrose by Imjustchico
 * Scaffolds key-only quest SQL and indexes quest and goal references in the user's own zone data through the shared zone extractor and server-class supplement.
 */

#include "ConfigMgr.h"
#include "Environment.h"
#include "Log.h"
#include "LogConfig.h"
#include "ServerClassCache.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;

    constexpr std::string_view Usage = R"(Usage: questtool <command> [options]

Commands:
  scaffold  write a Prep-first quest SQL skeleton
  refs      index quest and goal references in zone requirement data

Scaffold options:
  --title-key <key>   locale key for the quest title
  --giver <id>        object_template id bound to the first Prep dialog entry
  --lang-stem <stem>  locale key stem and quest identifier for the scaffold

Reference options:
  --client <dir>      client install (default: AMBROSE_CLIENT_DIR)
  --type-dump <file>  type dump (default: AMBROSE_TYPE_DUMP_PATH)
  --classes <file>    server-class supplement from schemaprobe --server-classes
  --zone <text>       include only zone paths containing this text

Exit status: 0 on success, 1 on operational failure, 2 on bad usage.
)";

    struct Arguments
    {
        std::string TitleKey;
        std::string LangStem;
        std::optional<std::string> Client;
        std::optional<std::string> TypeDump;
        std::optional<std::string> Classes;
        std::optional<std::string> Zone;
        uint32 Giver = 0;
        bool Help = false;
        bool Refs = false;
    };

    bool IsKey(std::string_view value)
    {
        if (value.empty() || !((value.front() >= 'A' && value.front() <= 'Z') || (value.front() >= 'a' && value.front() <= 'z')))
            return false;
        return std::all_of(value.begin() + 1, value.end(), [](char character)
        {
            return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_';
        });
    }

    std::optional<uint32> ParseTemplateId(std::string_view value)
    {
        uint32 id = 0;
        auto const [end, error] = std::from_chars(value.data(), value.data() + value.size(), id);
        if (error != std::errc() || end != value.data() + value.size() || id == 0)
            return std::nullopt;
        return id;
    }

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        if (args.size() < 2 || args[1] == "--help" || args[1] == "-h")
        {
            if (args.size() < 2)
            {
                error = "name the questtool command";
                return std::nullopt;
            }
            Arguments parsed;
            parsed.Help = true;
            return parsed;
        }

        Arguments parsed;
        if (args[1] == "refs")
        {
            parsed.Refs = true;
            for (std::size_t index = 2; index < args.size(); ++index)
            {
                std::string const& option = args[index];
                if (option == "--help" || option == "-h")
                {
                    parsed.Help = true;
                    continue;
                }
                if (option != "--client" && option != "--type-dump" && option != "--classes" && option != "--zone")
                {
                    error = fmt::format("unknown option {}", option);
                    return std::nullopt;
                }
                if (index + 1 >= args.size())
                {
                    error = fmt::format("{} needs a value", option);
                    return std::nullopt;
                }
                std::string const& value = args[++index];
                if (value.empty() || value.starts_with("--"))
                {
                    error = fmt::format("{} needs a value", option);
                    return std::nullopt;
                }
                if (option == "--client")
                    parsed.Client = value;
                else if (option == "--type-dump")
                    parsed.TypeDump = value;
                else if (option == "--classes")
                    parsed.Classes = value;
                else
                    parsed.Zone = value;
            }
            if (!parsed.Help && !parsed.Classes)
            {
                error = "refs requires --classes";
                return std::nullopt;
            }
            return parsed;
        }
        if (args[1] != "scaffold")
        {
            error = fmt::format("unknown command {}", args[1]);
            return std::nullopt;
        }

        bool hasGiver = false;
        for (std::size_t index = 2; index < args.size(); ++index)
        {
            std::string const& option = args[index];
            if (option == "--help" || option == "-h")
            {
                parsed.Help = true;
                continue;
            }
            if (option != "--title-key" && option != "--giver" && option != "--lang-stem")
            {
                error = fmt::format("unknown option {}", option);
                return std::nullopt;
            }
            if (index + 1 >= args.size())
            {
                error = fmt::format("{} needs a value", option);
                return std::nullopt;
            }
            std::string const& value = args[++index];
            if (value.empty() || value.starts_with("--"))
            {
                error = fmt::format("{} needs a value", option);
                return std::nullopt;
            }
            if (option == "--title-key")
                parsed.TitleKey = value;
            else if (option == "--lang-stem")
                parsed.LangStem = value;
            else
            {
                std::optional<uint32> const id = ParseTemplateId(value);
                if (!id)
                {
                    error = "--giver must be a nonzero unsigned template id";
                    return std::nullopt;
                }
                parsed.Giver = *id;
                hasGiver = true;
            }
        }
        if (parsed.Help)
            return parsed;
        if (!IsKey(parsed.TitleKey))
        {
            error = "--title-key must be a locale key containing only letters, digits and underscores";
            return std::nullopt;
        }
        if (!IsKey(parsed.LangStem) || parsed.LangStem.size() > 120)
        {
            error = "--lang-stem must start with a letter, contain only letters, digits and underscores, and be at most 120 characters";
            return std::nullopt;
        }
        if (!hasGiver)
        {
            error = "--giver is required";
            return std::nullopt;
        }
        return parsed;
    }

    std::string BuildSql(Arguments const& arguments)
    {
        std::string const& quest = arguments.LangStem;
        std::string const goal = fmt::format("{}_Goal0", quest);
        return fmt::format(
            "-- Project Ambrose by Imjustchico\n"
            "-- Scaffolds a key-only quest with a giver-bound Prep entry and a persona goal stub.\n"
            "INSERT INTO `quest_template` (`name`, `title_key`, `info_key`, `prep_key`, `underway_key`, `complete_key`) "
            "VALUES ('{}', '{}', '{}_Info', '{}_Prep', '{}_Underway', '{}_Complete');\n"
            "INSERT INTO `quest_start_goal` (`quest`, `goal_name`) VALUES ('{}', '{}');\n"
            "INSERT INTO `quest_goal` (`quest`, `goal_name`, `type`, `title_key`, `underway_key`, `complete_key`, `location_key`, `persona_name`) "
            "VALUES ('{}', '{}', 'persona', '{}_Title', '{}_Underway', '{}_Complete', '{}_Location', '');\n"
            "INSERT INTO `quest_dialog` (`quest`, `owner`, `goal_name`, `tag`) VALUES ('{}', 'quest', '', 'Prep');\n"
            "INSERT INTO `quest_dialog_entry` (`quest`, `owner`, `goal_name`, `tag`, `position`, `actor_template_id`, `dialog_key`) "
            "VALUES ('{}', 'quest', '', 'Prep', 0, {}, '{}_Prep0');\n",
            quest, arguments.TitleKey, quest, quest, quest, quest, quest, goal, quest, goal, quest, quest, quest, quest, quest, quest, arguments.Giver, quest);
    }

    std::filesystem::path FindOutputPath(std::filesystem::path const& directory, Arguments const& arguments)
    {
        auto const timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::string const base = fmt::format("rev_{}_quest_{}", timestamp, arguments.LangStem);
        for (uint32 suffix = 1; ; ++suffix)
        {
            std::string const name = suffix == 1 ? base : fmt::format("{}_{}", base, suffix);
            std::filesystem::path const path = directory / fmt::format("{}.sql", name);
            if (!std::filesystem::exists(path))
                return path;
        }
    }

    bool WriteScaffold(Arguments const& arguments, std::filesystem::path& outputPath, std::string& error)
    {
        std::filesystem::path const directory = "data/sql/updates/pending_db_world";
        std::error_code filesystemError;
        std::filesystem::create_directories(directory, filesystemError);
        if (filesystemError)
        {
            error = fmt::format("cannot create {}: {}", ConfigMgr::PathToUtf8(directory), filesystemError.message());
            return false;
        }

        std::string const sql = BuildSql(arguments);
        for (uint32 attempt = 0; attempt < 100; ++attempt)
        {
            std::filesystem::path const candidate = FindOutputPath(directory, arguments);
            std::filesystem::path const temporary = candidate.string() + fmt::format(".tmp.{}", attempt);
            if (std::filesystem::exists(temporary))
                continue;
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::out | std::ios::trunc);
                if (!file)
                {
                    error = fmt::format("cannot write {}", ConfigMgr::PathToUtf8(temporary));
                    return false;
                }
                file << sql;
                if (!file)
                {
                    error = fmt::format("cannot finish writing {}", ConfigMgr::PathToUtf8(temporary));
                    file.close();
                    std::filesystem::remove(temporary, filesystemError);
                    return false;
                }
            }
            std::filesystem::rename(temporary, candidate, filesystemError);
            if (!filesystemError)
            {
                outputPath = candidate;
                return true;
            }
            std::error_code removeError;
            std::filesystem::remove(temporary, removeError);
            if (!std::filesystem::exists(candidate))
            {
                error = fmt::format("cannot publish {}: {}", ConfigMgr::PathToUtf8(candidate), filesystemError.message());
                return false;
            }
            filesystemError.clear();
        }
        error = "could not choose a unique pending update filename after 100 attempts";
        return false;
    }

    std::string CleanField(std::string_view value)
    {
        std::string cleaned;
        cleaned.reserve(value.size());
        for (char const character : value)
            cleaned.push_back(character == '\t' || character == '\r' || character == '\n' ? ' ' : character);
        return cleaned;
    }

    bool MatchesZone(std::string_view zone, std::optional<std::string> const& filter)
    {
        return !filter || zone.find(*filter) != std::string_view::npos;
    }

    bool IsRequirementPath(std::string_view path)
    {
        return path.find("m_requirements") != std::string_view::npos || path.find("m_spawnRequirements") != std::string_view::npos
            || path.find("m_globalDynamicReqs") != std::string_view::npos;
    }

    int RunRefs(Arguments const& arguments)
    {
        std::optional<std::string> const client = arguments.Client ? arguments.Client : Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
        std::optional<std::string> const typeDump = arguments.TypeDump ? arguments.TypeDump : Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
        if (!client || client->empty())
        {
            std::cerr << "questtool: refs needs --client or AMBROSE_CLIENT_DIR\n";
            return Failure;
        }
        if (!typeDump || typeDump->empty())
        {
            std::cerr << "questtool: refs needs --type-dump or AMBROSE_TYPE_DUMP_PATH\n";
            return Failure;
        }

        TypeDumpLoader::RawDump supplement;
        std::string error;
        if (!ServerClassCache::Read(LogConfig::Utf8Path(*arguments.Classes), supplement, error))
        {
            std::cerr << fmt::format("questtool: {}\n", error);
            return Failure;
        }
        ZoneExtractionProgress const progress = [](std::size_t archivesRead, std::size_t archives)
        {
            if (archivesRead % 500 == 0 || archivesRead == archives)
                std::cerr << fmt::format("questtool: read {} of {} zone archives\n", archivesRead, archives);
        };
        std::optional<ZoneExtraction> extraction = ZoneExtractor::ExtractFromInstall(LogConfig::Utf8Path(*client), LogConfig::Utf8Path(*typeDump), error,
            progress, std::move(supplement));
        if (!extraction)
        {
            std::cerr << fmt::format("questtool: {}\n", error);
            return Failure;
        }
        if (!extraction->Ok())
        {
            for (std::string const& extractionError : extraction->Errors)
                std::cerr << fmt::format("questtool: {}\n", extractionError);
            if (extraction->ErrorCount > extraction->Errors.size())
                std::cerr << fmt::format("questtool: {} additional zone extraction error(s) omitted\n", extraction->ErrorCount - extraction->Errors.size());
            return Failure;
        }

        std::size_t matchedZones = 0;
        for (ExtractedZone const& zone : extraction->Zones)
            if (MatchesZone(zone.Path, arguments.Zone))
                ++matchedZones;
        if (matchedZones == 0)
        {
            if (arguments.Zone)
                std::cerr << fmt::format("questtool: no zone path contains {}\n", *arguments.Zone);
            else
                std::cerr << "questtool: no zones were extracted\n";
            return Failure;
        }

        std::cout << "zone\tsource_type\tsource_name\trequirement\tquest\tgoal\n";
        for (ExtractedZone const& zone : extraction->Zones)
        {
            if (!MatchesZone(zone.Path, arguments.Zone))
                continue;
            for (ExtractedQuestReference const& reference : zone.QuestReferences)
                std::cout << CleanField(zone.Path) << '\t' << CleanField(reference.SourceType) << '\t' << CleanField(reference.SourceName) << '\t'
                          << CleanField(reference.RequirementClass) << '\t' << CleanField(reference.QuestName) << '\t' << CleanField(reference.GoalName) << '\n';
        }

        std::size_t failures = 0;
        for (TriggerFileFailure const& failure : extraction->TriggerFailures)
        {
            if (!MatchesZone(failure.Zone, arguments.Zone) || (failure.File != ZoneExtractor::TriggerEntry && failure.File != ZoneExtractor::VolumeEntry
                    && failure.File != ZoneExtractor::SpawnEntry))
                continue;
            ++failures;
            std::cerr << fmt::format("questtool: {} {}: {}\n", failure.Zone, failure.File, failure.Detail);
        }
        for (SkippedZonePart const& skipped : extraction->Skipped)
        {
            if (!MatchesZone(skipped.Zone, arguments.Zone) || !IsRequirementPath(skipped.Path))
                continue;
            ++failures;
            std::cerr << fmt::format("questtool: {} skipped requirement part {} (class hash {})\n", skipped.Zone, skipped.Path, skipped.ClassHash);
        }
        if (failures != 0)
        {
            std::cerr << fmt::format("questtool: reference output may be incomplete; {} zone file or requirement part(s) could not be decoded\n", failures);
            return Failure;
        }
        return Success;
    }

    int Run(std::vector<std::string> const& args)
    {
        std::string error;
        std::optional<Arguments> const arguments = Parse(args, error);
        if (!arguments)
        {
            std::cerr << fmt::format("questtool: {}\n\n{}", error, Usage);
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << Usage;
            return Success;
        }
        if (arguments->Refs)
            return RunRefs(*arguments);
        std::filesystem::path outputPath;
        if (!WriteScaffold(*arguments, outputPath, error))
        {
            std::cerr << fmt::format("questtool: {}\n", error);
            return Failure;
        }
        std::cout << fmt::format("wrote {}\n", ConfigMgr::PathToUtf8(outputPath));
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
        std::cerr << fmt::format("questtool: {}\n", error.what());
        return Failure;
    }
}
