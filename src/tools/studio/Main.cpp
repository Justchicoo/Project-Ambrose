/*
 * Project Ambrose by Imjustchico
 * studio entry point: finds the user's own install and type dump as the other tools do, opens Root.wad and its locale text, reads the quest tables of the world database --world-db or AMBROSE_WORLD_DATABASE_INFO names, and opens the Studio's window over them; --answer prints the channel's answer to one message instead, which is how a terminal or a test asks what the window would show. It exits 0 on success, 1 when the window or the content cannot be opened, and 2 on bad usage.
 */

#include "ClientLocator.h"
#include "ClientSetup.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LocaleStore.h"
#include "Log.h"
#include "LogConfig.h"
#include "QuestMgr.h"
#include "StudioChannel.h"
#include "StudioContent.h"
#include "StudioWindow.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <exception>
#include <filesystem>
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

    constexpr std::string_view Usage = R"(Usage: studio [options]

Opens Ambrose Studio, the content editor, over your own Wizard101 install and the
world database: quests with their goals, dialogs, requirements and rewards, and
every cinematic in the install with the quest steps that play it.

Options:
  --client <dir>       the install holding Bin and Data (default: the one the other tools use)
  --type-dump <file>   the type dump to decode with (default: the one the other tools use)
  --world-db <info>    world database as host;port;user;password;database
                       (default: AMBROSE_WORLD_DATABASE_INFO)
  --locale <name>      locale the text is shown in (default: en-US)
  --answer <json>      print the answer to one window message, such as
                       {"path":"/studio/quests","body":{"search":"unicorn"}}, and open no window
  --help               print this text

Exit status: 0 on success, 1 when the window or the content cannot be opened, 2 on bad usage.
)";

    struct Arguments
    {
        std::optional<std::string> Client;
        std::optional<std::string> TypeDump;
        std::optional<std::string> WorldDatabase;
        std::string Locale = "en-US";
        std::vector<std::string> Answers;
        bool Help = false;
    };

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments parsed;
        for (std::size_t i = 1; i < args.size(); ++i)
        {
            std::string const& arg = args[i];
            if (arg == "--help")
            {
                parsed.Help = true;
                continue;
            }
            if (arg != "--client" && arg != "--type-dump" && arg != "--world-db" && arg != "--locale" && arg != "--answer")
            {
                error = fmt::format("there is no option {}", arg);
                return std::nullopt;
            }
            if (i + 1 >= args.size() || args[i + 1].starts_with("--"))
            {
                error = fmt::format("{} needs a value", arg);
                return std::nullopt;
            }
            std::string const& value = args[++i];
            if (arg == "--client")
                parsed.Client = value;
            else if (arg == "--type-dump")
                parsed.TypeDump = value;
            else if (arg == "--world-db")
                parsed.WorldDatabase = value;
            else if (arg == "--locale")
                parsed.Locale = value;
            else
                parsed.Answers.push_back(value);
        }
        return parsed;
    }

    TypeCatalogPtr LoadTypes(std::optional<std::string> const& dump)
    {
        if (!dump || dump->empty())
            return nullptr;
        std::filesystem::path const json = LogConfig::Utf8Path(*dump);
        std::filesystem::path binary = json;
        binary.replace_extension(".bin");
        bool loaded = std::filesystem::exists(binary) && sTypeRegistry.LoadBinary(binary, json, {});
        if (!loaded)
            loaded = sTypeRegistry.LoadFromFile(json);
        if (!loaded)
        {
            std::cerr << fmt::format("studio: the type dump {} cannot be read, so nothing is decoded\n", *dump);
            return nullptr;
        }
        return sTypeRegistry.GetCatalog();
    }

    QuestRows ReadQuests(std::optional<std::string> const& info)
    {
        QuestRows rows;
        if (!info || info->empty())
        {
            std::cerr << "studio: no world database is named, so no quest is shown; pass --world-db or set AMBROSE_WORLD_DATABASE_INFO\n";
            return rows;
        }
        if (!WorldDatabase.SetConnectionInfo(*info, 1, 1) || WorldDatabase.Open() != 0)
        {
            std::cerr << "studio: the world database cannot be opened, so no quest is shown\n";
            return rows;
        }
        std::vector<std::string> errors;
        if (!QuestMgr::ReadWorldRows(rows, errors))
            for (std::string const& problem : errors)
                std::cerr << fmt::format("studio: {}\n", problem);
        WorldDatabase.Close();
        return rows;
    }

    int Run(std::vector<std::string> const& args)
    {
        std::string error;
        std::optional<Arguments> arguments = Parse(args, error);
        if (!arguments)
        {
            std::cerr << "studio: " << error << "\n\n" << Usage;
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << Usage;
            return Success;
        }

        LocalClientSystem const system;
        SetupMode const mode = ClientSetup::ModeForTool(system, std::cerr, "studio");
        std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ToolPrompt(std::cout, mode);
        std::optional<std::string> client = arguments->Client;
        ClientSetup::ForTool(mode, client, arguments->TypeDump ? nullptr : &arguments->TypeDump, *prompt, system, ClientSetup::ToolTypeDumps(system, "studio", std::cerr),
            "studio", std::cerr);

        TypeCatalogPtr const catalog = LoadTypes(arguments->TypeDump);
        std::shared_ptr<KiwadArchive const> root;
        if (client && !client->empty())
        {
            std::filesystem::path const wad = LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad";
            root = KiwadArchive::Open(wad, error);
            if (!root)
                std::cerr << fmt::format("studio: {} cannot be opened: {}\n", ConfigMgr::PathToUtf8(wad), error);
            else if (!sLocaleStore.Load(root, arguments->Locale, error))
                std::cerr << fmt::format("studio: the locale text cannot be read: {}\n", error);
        }
        else
            std::cerr << "studio: no install was found, so no cinematic or locale text is shown\n";

        if (!arguments->WorldDatabase)
            arguments->WorldDatabase = Ambrose::GetEnv("AMBROSE_WORLD_DATABASE_INFO");
        StudioContent const content(ReadQuests(arguments->WorldDatabase), catalog, root,
            [](std::string_view key) { return sLocaleStore.IsLoaded() ? sLocaleStore.Resolve(key) : std::nullopt; });
        StudioChannel const channel(content);

        if (!arguments->Answers.empty())
        {
            for (std::string const& message : arguments->Answers)
            {
                std::string const answer = channel.Answer(message);
                if (answer.empty())
                {
                    std::cerr << fmt::format("studio: {} is not a JSON object\n", message);
                    return BadUsage;
                }
                std::cout << answer << "\n";
            }
            return Success;
        }

        if (!StudioWindow::Show(ClientLocator::GetDataFolder(system), [&channel](std::string const& message) { return channel.Answer(message); }, error))
        {
            std::cerr << fmt::format("studio: {}\n", error);
            return Failure;
        }
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
        std::cerr << "studio: " << error.what() << "\n";
        return Failure;
    }
}
