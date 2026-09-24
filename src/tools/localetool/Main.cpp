/*
 * Project Ambrose by Imjustchico
 * localetool entry point: loads the locale .lang files of the user's own Root.wad and prints the keys whose text matches, checks that a key exists and prints its text, prints every entry of one table, or lists the installed locales with their sizes and the files each had to skip; reads its arguments and environment as UTF-8, checks its arguments before anything is searched, then when no install is named follows AMBROSE_SETUP_MODE: auto uses the newest install found, ask offers the finds, off prints them with the flag to pass; takes -- to end the options, writes UTF-8, and exits 0 on success, 1 when a key, table, match or locale is missing or cannot be loaded or the install cannot be read, and 2 on bad usage.
 */

#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LocaleStore.h"
#include "Log.h"
#include "LogConfig.h"

#include <fmt/format.h>

#include <exception>
#include <iostream>
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

    constexpr std::string_view Usage = R"(Usage: localetool [options] find <text>
       localetool [options] check <key>
       localetool [options] dump <stem>
       localetool [options] locales

Resolves locale keys against the .lang files of your own Wizard101 install.

Commands:
  find <text>    print every key whose text is exactly <text>
  check <key>    print the key's text, or exit 1 when the key is missing
  dump <stem>    print every key and text of one table, such as QuestTitle
  locales        print each installed locale with its file and key counts, and the files it skipped

Options:
  --client <dir>     the install holding Data/GameData (default: AMBROSE_CLIENT_DIR)
  --wad <file>       an archive in Data/GameData, or a path to one (default: Root.wad)
  --locale <name>    the locale to read, such as en-US or de (default: en-US)
  --help             print this text
  --                 treat every later argument as a command word, even one starting with --

Exit status: 0 on success, 1 when nothing matches, the key or table is missing, a
locale cannot be loaded or the install cannot be read, 2 on bad usage.
)";

    struct Arguments
    {
        std::optional<std::string> Client;
        std::string Wad = "Root.wad";
        std::string Locale = "en-US";
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
            {
                optionsEnded = true;
                continue;
            }
            if (arg == "--help" || arg == "-h")
            {
                parsed.Help = true;
                continue;
            }
            if (arg == "--client" || arg == "--wad" || arg == "--locale")
            {
                if (index + 1 >= args.size())
                {
                    error = fmt::format("{} needs a value", arg);
                    return std::nullopt;
                }
                std::string const& value = args[++index];
                if (arg == "--client")
                    parsed.Client = value;
                else if (arg == "--wad")
                    parsed.Wad = value;
                else
                    parsed.Locale = value;
                continue;
            }
            if (arg.starts_with("--"))
            {
                error = fmt::format("unknown option {}", arg);
                return std::nullopt;
            }
            parsed.Words.push_back(arg);
        }
        if (parsed.Help)
            return parsed;
        if (parsed.Words.empty())
        {
            error = "name a command: find, check, dump or locales";
            return std::nullopt;
        }
        std::string const& command = parsed.Words.front();
        std::size_t const expected = command == "locales" ? 1 : 2;
        if (command != "find" && command != "check" && command != "dump" && command != "locales")
        {
            error = fmt::format("unknown command {}", command);
            return std::nullopt;
        }
        if (parsed.Words.size() != expected)
        {
            error = expected == 1 ? fmt::format("{} takes no argument", command) : fmt::format("{} takes exactly one argument; quote text that holds spaces", command);
            return std::nullopt;
        }
        return parsed;
    }

    int Run(std::vector<std::string> const& args)
    {
        std::string error;
        std::optional<Arguments> arguments = Parse(args, error);
        if (!arguments)
        {
            std::cerr << "localetool: " << error << "\n\n" << Usage;
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << Usage;
            return Success;
        }
        if (!arguments->Client)
        {
            if (std::optional<std::string> found = Ambrose::GetEnv("AMBROSE_CLIENT_DIR"); found && !found->empty())
                arguments->Client = std::move(found);
        }

        std::filesystem::path wad = LogConfig::Utf8Path(arguments->Wad);
        if (!wad.has_parent_path() && !arguments->Client)
        {
            LocalClientSystem const system;
            SetupMode const mode = ClientSetup::ModeForTool(system, std::cerr, "localetool");
            std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ToolPrompt(std::cout, mode);
            ClientSetup::ForTool(mode, arguments->Client, nullptr, *prompt, system, nullptr, "localetool", std::cerr);
        }
        if (!wad.has_parent_path() && arguments->Client)
            wad = LogConfig::Utf8Path(*arguments->Client) / "Data" / "GameData" / wad;
        std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(wad, error);
        if (!archive)
        {
            std::cerr << fmt::format("localetool: cannot open {}: {}\n", ConfigMgr::PathToUtf8(wad), error);
            return Failure;
        }
        LocaleStore store;
        if (!store.Load(std::shared_ptr<KiwadArchive const>(std::move(archive)), arguments->Locale, error))
        {
            std::cerr << fmt::format("localetool: {}\n", error);
            return Failure;
        }

        std::string const& command = arguments->Words.front();
        if (command == "locales")
        {
            bool failed = false;
            for (std::string const& locale : store.GetLocales())
            {
                std::string problem;
                std::shared_ptr<LocaleTable const> const table = store.GetTable(locale, &problem);
                if (!table)
                {
                    std::cerr << fmt::format("localetool: {}\n", problem);
                    failed = true;
                    continue;
                }
                std::cout << fmt::format("{}\t{} files\t{} keys\t{} repeated\t{} skipped\n", locale, table->GetFileCount(), table->GetKeyCount(), table->GetDuplicateCount(), table->GetProblems().size());
                for (std::string const& skipped : table->GetProblems())
                    std::cerr << fmt::format("localetool: {} skipped {}\n", locale, skipped);
            }
            return failed ? Failure : Success;
        }

        std::shared_ptr<LocaleTable const> const table = store.GetTable();
        std::string const& argument = arguments->Words[1];
        if (command == "find")
        {
            std::vector<std::string> const keys = table->FindKeys(argument);
            for (std::string const& key : keys)
                std::cout << key << '\n';
            return keys.empty() ? Failure : Success;
        }
        if (command == "check")
        {
            std::string const* const text = table->Find(argument);
            if (!text)
            {
                std::cerr << fmt::format("localetool: {} has no key {}\n", arguments->Locale, argument);
                return Failure;
            }
            std::cout << *text << '\n';
            return Success;
        }
        if (!table->HasStem(argument))
        {
            std::cerr << fmt::format("localetool: {} has no table {}\n", arguments->Locale, argument);
            return Failure;
        }
        std::vector<std::pair<std::string, std::string>> const entries = table->GetEntries(argument);
        for (auto const& [key, text] : entries)
            std::cout << key << '\t' << text << '\n';
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
        std::cerr << "localetool: " << error.what() << '\n';
        return Failure;
    }
}
