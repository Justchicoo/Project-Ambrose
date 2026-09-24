/*
 * Project Ambrose by Imjustchico
 * launcher entry point: reads its arguments and environment as UTF-8, loads launcher.conf when there is one, lets every option override it, and then has the Launcher library find the user's own install, build the folder the client runs from and the command that starts the client against an Ambrose login server; a machine that cannot start a Windows program is named before anything is written, --dry-run prints the folder and the command and starts nothing, --wait returns the client's own exit code and ends the client if the launcher is stopped, --tail waits and prints the client's own log lines, and without either the client is started detached so closing the launcher leaves the game running; it exits 0 on success, 1 when a refusal names its cause or the client cannot be started, and 2 on bad usage.
 */

#include "ClientSetup.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "Launcher.h"
#include "LauncherFiles.h"
#include "Log.h"
#include "LogTail.h"

#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
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

    constexpr std::chrono::milliseconds TailInterval{ 200 };

    constexpr std::string_view Usage = R"(Usage: launcher [options]

Starts your own Wizard101 client against an Ambrose login server. KingsIsle's
launcher and patcher are never run, nothing inside the install is written, and
the client runs from a folder of its own whose configuration reaches nothing
outside this machine.

Options:
  --config <file>      settings to read (default: launcher.conf in this folder or beside the program)
  --client <dir>       the install holding Bin and Data (default: ClientDir, AMBROSE_CLIENT_DIR, else the newest install found)
  --host <host>        login server to connect to (default: 127.0.0.1)
  --port <port>        login server port (default: 12000)
  --locale <name>      client locale, such as en-US (default: en-US)
  --window <w>x<h>     windowed size (default: 1280x720)
  --fullscreen         start fullscreen instead of windowed
  --run-dir <dir>      folder the client runs from (default: client/<revision> in the Ambrose data folder)
  --user <id> <key> [name]
                       log in without the login window, as the client's own -U option
                       (needs milestone 5.06, which answers MSG_USER_VALIDATE)
  --character <name>   create or select that character, as the client's own -C option
                       (needs milestone 3.16, which creates a wizard)
  --dry-run            print the run folder and the exact command, and start nothing
  --wait               wait for the client, return its exit code, and end it if the launcher is stopped
  --tail               print the client's own log lines while it runs, waiting as --wait does
  --help               print this text

Exit status: 0 on success, or the client's own code with --wait; 1 when no install is
found, the client program is missing, patching is asked for, a login host or port is
missing, the run folder cannot be written or the client cannot be started; 2 on bad usage.
)";

    volatile std::sig_atomic_t StopSignal = 0;

    void OnStopSignal(int)
    {
        StopSignal = 1;
    }

    struct Arguments
    {
        std::optional<std::string> Config;
        LauncherRequest Request;
        bool DryRun = false;
        bool Wait = false;
        bool Tail = false;
        bool Help = false;
    };

    bool IsValue(std::vector<std::string> const& args, std::size_t index)
    {
        return index < args.size() && !args[index].starts_with("--");
    }

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments parsed;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            if (arg == "--help" || arg == "-h")
                parsed.Help = true;
            else if (arg == "--dry-run")
                parsed.DryRun = true;
            else if (arg == "--wait")
                parsed.Wait = true;
            else if (arg == "--tail")
                parsed.Tail = true;
            else if (arg == "--fullscreen")
                parsed.Request.Fullscreen = "1";
            else if (arg == "--user")
            {
                if (!IsValue(args, index + 1) || !IsValue(args, index + 2))
                {
                    error = "--user needs a user id and a key, and takes a character name after them";
                    return std::nullopt;
                }
                ClientLogin login;
                login.UserId = args[++index];
                login.Key = args[++index];
                if (IsValue(args, index + 1))
                    login.Name = args[++index];
                parsed.Request.User = std::move(login);
            }
            else if (arg == "--config" || arg == "--client" || arg == "--host" || arg == "--port" || arg == "--locale" || arg == "--window" || arg == "--run-dir" || arg == "--character")
            {
                if (!IsValue(args, index + 1))
                {
                    error = fmt::format("{} needs a value", arg);
                    return std::nullopt;
                }
                std::string const& value = args[++index];
                if (arg == "--config")
                    parsed.Config = value;
                else if (arg == "--client")
                    parsed.Request.ClientDir = value;
                else if (arg == "--host")
                    parsed.Request.Host = value;
                else if (arg == "--port")
                    parsed.Request.Port = value;
                else if (arg == "--locale")
                    parsed.Request.Locale = value;
                else if (arg == "--window")
                    parsed.Request.Window = value;
                else if (arg == "--run-dir")
                    parsed.Request.RunDir = value;
                else
                    parsed.Request.Character = value;
            }
            else
            {
                error = fmt::format("unknown argument {}", arg);
                return std::nullopt;
            }
        }
        return parsed;
    }

    std::filesystem::path SettingsFile(std::optional<std::string> const& named, ClientSystem const& system)
    {
        if (named)
            return ConfigMgr::PathFromUtf8(*named);
        std::filesystem::path const working = system.GetWorkingDirectory() / "launcher.conf";
        if (system.IsFile(working))
            return working;
        std::filesystem::path const beside = system.GetExecutableDirectory() / "launcher.conf";
        if (system.IsFile(beside))
            return beside;
        return std::filesystem::path();
    }

    int Run(std::vector<std::string> const& args)
    {
        std::string error;
        std::optional<Arguments> arguments = Parse(args, error);
        if (!arguments)
        {
            std::cerr << "launcher: " << error << "\n\n" << Usage;
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << Usage;
            return Success;
        }

        LocalClientSystem const system;
        LocalLauncherFiles const files;
        ConfigMgr config;
        if (std::filesystem::path const settings = SettingsFile(arguments->Config, system); !settings.empty())
        {
            ConfigLoadResult const loaded = config.LoadInitial(settings);
            if (!loaded.Succeeded())
            {
                for (ConfigIssue const& issue : loaded.Errors)
                    std::cerr << fmt::format("launcher: {}\n", issue.ToString());
                return Failure;
            }
        }
        Launcher::FromConfig(config, arguments->Request);

        SetupMode const mode = ClientSetup::ModeForTool(system, std::cerr, Launcher::ToolName);
        std::unique_ptr<SetupPrompt> const prompt = ClientSetup::ToolPrompt(std::cout, mode);
        Launcher const launcher(system, files, std::cerr);
        std::optional<LauncherPlan> const plan = launcher.Prepare(arguments->Request, mode, *prompt, error);
        if (!plan)
        {
            std::cerr << fmt::format("launcher: {}\n", error);
            return Failure;
        }
        std::cout << fmt::format("launcher: install {}\n", plan->Install.Describe());
        std::cout << fmt::format("launcher: run folder {}, its configuration from {}{}\n", ConfigMgr::PathToUtf8(plan->RunFolder), plan->Folder.ConfigSource,
            plan->Folder.Rebuild ? ", with the install's own revision.dat and data.dat copied into it" : "");
        std::cout << fmt::format("launcher: {}\n", plan->Command());
        std::string cannotStart;
        bool const startable = launcher.CanStart(cannotStart);
        if (!startable)
            std::cerr << fmt::format("launcher: {}\n", cannotStart);
        if (arguments->DryRun)
        {
            std::cout << "launcher: nothing was started and nothing was written, because --dry-run was given\n";
            return Success;
        }
        if (!startable)
            return Failure;
        if (!launcher.WriteRunFolder(*plan, error))
        {
            std::cerr << fmt::format("launcher: {}\n", error);
            return Failure;
        }

        bool const wait = arguments->Wait || arguments->Tail;
        std::signal(SIGINT, OnStopSignal);
        std::signal(SIGTERM, OnStopSignal);
        std::atomic<bool> following{ arguments->Tail };
        std::thread follower;
        if (arguments->Tail)
        {
            follower = std::thread([&plan, &following]
            {
                LogTail tail(plan->LogFile, [](std::string_view line) { std::cout << line << '\n'; });
                while (following.load(std::memory_order_relaxed))
                {
                    tail.Poll();
                    std::this_thread::sleep_for(TailInterval);
                }
                tail.Finish();
            });
        }
        std::optional<int> const code = launcher.Start(*plan, wait, [] { return StopSignal != 0; }, error);
        following.store(false, std::memory_order_relaxed);
        if (follower.joinable())
            follower.join();
        if (!code)
        {
            std::cerr << fmt::format("launcher: {}\n", error);
            return Failure;
        }
        if (!error.empty())
            std::cerr << fmt::format("launcher: {}\n", error);
        if (!wait)
        {
            std::cout << "launcher: the client is running, and closing the launcher leaves it running\n";
            return Success;
        }
        std::cout << fmt::format("launcher: the client exited with {}\n", *code);
        return *code;
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
        std::cerr << "launcher: " << error.what() << "\n";
        return Failure;
    }
}
