/*
 * Project Ambrose by Imjustchico
 * Supervisor entry point: with --console-break and a process group it only sends Ctrl+Break to that group's console and exits, which is how it interrupts an app on Windows without leaving its own console; otherwise it runs as an app of its own that starts, takes back and watches the apps Supervisor.Apps names, only checking their definitions and its saved state under --check so a check leaves no app running, serves the panel and the supervisor routes on its admin API, offers apps, start, stop, restart and kill on its console, and leaves the apps running when it stops so the next start takes them back.
 */

#include "AdminServer.h"
#include "AdminStatus.h"
#include "ChildProcess.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "Duration.h"
#include "Environment.h"
#include "Log.h"
#include "AppOptions.h"
#include "Panel.h"
#include "PanelUsers.h"
#include "ServerApp.h"
#include "StringUtil.h"
#include "Supervisor.h"
#include "TlsCertificate.h"

#include <fmt/format.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
    constexpr std::string_view ConsoleBreakOption = "--console-break";
    constexpr std::string_view SelfSignedOption = "--panel-self-signed";

    int SendConsoleBreak(std::vector<std::string> const& arguments)
    {
        std::optional<int64> const group = arguments.size() == 3 ? Ambrose::StringTo<int64>(arguments[2]) : std::nullopt;
        if (!group)
        {
            std::cerr << "supervisor: " << ConsoleBreakOption << " takes one process group id\n";
            return 2;
        }
        std::string error;
        if (ChildProcess::SendConsoleBreak(*group, error))
            return 0;
        std::cerr << "supervisor: " << error << "\n";
        return 1;
    }

    ChildBreakSender BreakThroughThisProgram()
    {
        return [](int64 group, std::string& error)
        {
            ChildProcessOptions options;
            options.Program = Ambrose::GetExecutablePath();
            options.Arguments = { std::string(ConsoleBreakOption), std::to_string(group) };
            options.Timeout = std::chrono::seconds(10);
            std::string output;
            options.OnLine = [&output](std::string_view line, bool)
            {
                if (!output.empty())
                    output += "; ";
                output += line;
            };
            ChildProcessResult const result = ChildProcess::Run(options);
            if (result.Succeeded())
                return true;
            error = !output.empty() ? output : !result.Error.empty() ? result.Error : fmt::format("the Ctrl+Break helper exited with {}", result.ExitCode.value_or(-1));
            return false;
        };
    }

    int WriteSelfSignedCertificate(std::vector<std::string> const& arguments)
    {
        std::vector<std::string> rest;
        std::copy_if(arguments.begin(), arguments.end(), std::back_inserter(rest), [](std::string const& argument) { return argument != SelfSignedOption; });
        AppOptions const options = AppOptions::Parse(rest, "supervisor.conf");
        if (!options.Error.empty())
        {
            std::cerr << "supervisor: " << options.Error << '\n';
            return 2;
        }
        ConfigLoadResult const loaded = sConfigMgr.LoadInitial(ConfigMgr::PathFromUtf8(options.ConfigFile));
        if (!loaded.Succeeded())
        {
            std::cerr << "supervisor: " << options.ConfigFile << " could not be read" << '\n';
            return 1;
        }
        ListenerSettings const settings = Panel::LoadSettings(sConfigMgr);
        if (!settings.HasTls())
        {
            std::cerr << "supervisor: set " << settings.Option("CertificateFile") << " and " << settings.Option("PrivateKeyFile")
                      << " to the files to write, then run " << SelfSignedOption << " again" << '\n';
            return 2;
        }
        std::string error;
        if (!TlsCertificate::CreateSelfSigned(settings.CertificateFile, settings.PrivateKeyFile, "Ambrose panel",
                { "localhost", "127.0.0.1", "::1" }, TlsCertificate::SelfSignedDays, error))
        {
            std::cerr << "supervisor: " << error << '\n';
            return 1;
        }
        TlsCertificate written;
        if (!written.Load(settings.CertificateFile, settings.PrivateKeyFile, error))
        {
            std::cerr << "supervisor: " << error << '\n';
            return 1;
        }
        std::cout << "Wrote " << ConfigMgr::PathToUtf8(settings.CertificateFile) << " and " << ConfigMgr::PathToUtf8(settings.PrivateKeyFile) << '\n'
                  << written.Describe() << '\n'
                  << "It signed itself, so a browser trusts it only once someone tells it to." << '\n';
        return 0;
    }

    std::string WhenText(int64 epochMs)
    {
        std::time_t const value = static_cast<std::time_t>(epochMs / 1000);
        std::tm parts = {};
#ifdef _WIN32
        localtime_s(&parts, &value);
#else
        localtime_r(&value, &parts);
#endif
        return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    }

    std::optional<uint32> ParseCountdown(std::string_view text)
    {
        if (text == "0")
            return 0;
        std::optional<Seconds> const duration = Ambrose::ParseDuration(text);
        if (!duration || duration->count() > ManagedApp::MaxCountdownSeconds)
            return std::nullopt;
        return static_cast<uint32>(duration->count());
    }

    class SupervisorApp : public ServerApp
    {
    public:
        static constexpr uint16 DefaultAdminPort = 12020;

        SupervisorApp() : ServerApp({ "supervisor", "supervisor.conf", DefaultAdminPort }, sConfigMgr, sLog, std::cout, std::cerr), _supervisor(sLog, BreakThroughThisProgram()), _panel(sLog, ClientLocator::GetDataFolder(LocalClientSystem()), sConfigMgr.GetFilename().parent_path())
        {
            RegisterCommands();
        }

    protected:
        void OnAdminApiReady(AdminServer& admin) override
        {
            _supervisor.Register(admin.Routes(), [this] { return BuildStatus(); });
        }

        bool OnStart() override
        {
            std::vector<std::string> problems;
            LocalClientSystem const system;
            std::error_code code;
            std::filesystem::path const working = std::filesystem::current_path(code);
            SupervisorSettings const settings = SupervisorSettings::Load(Config(), ClientLocator::GetDataFolder(system), Ambrose::GetExecutableDirectory(), working, problems);
            std::string error;
            bool const started = _supervisor.Start(Config(), settings, !IsCheckOnly(), problems, error);
            for (std::string const& problem : problems)
                LOG_WARN("server.supervisor", "{}", problem);
            if (!started)
            {
                LOG_ERROR("server.supervisor", "{}", error);
                return false;
            }
            std::vector<AppSnapshot> const apps = _supervisor.Snapshots();
            if (apps.empty())
                LOG_WARN("server.supervisor", "Supervisor.Apps names no app, so the supervisor has nothing to run");
            LOG_INFO("server.supervisor", "Watching {} app(s), with their state in {} and their output in {}", apps.size(), ConfigMgr::PathToUtf8(settings.StateFile), ConfigMgr::PathToUtf8(settings.OutputFolder));
            RegisterStandardRoutes(_panel.Routes());
            _supervisor.Register(_panel.Routes(), [this] { return BuildStatus(); });
            _panel.SetErrorSource([this]
            {
                std::vector<std::pair<std::string, std::string>> reports = _supervisor.CollectErrorReports();
                reports.emplace_back(GetInfo().Name, AdminStatus::ErrorsJson(GetInfo().Name));
                return reports;
            });
            if (!_panel.Start(Config(), error))
            {
                LOG_ERROR("server.panel", "{}", error);
                return false;
            }
            return true;
        }

        void OnStop() override
        {
            _panel.Stop();
            _supervisor.Shutdown();
            LOG_INFO("server.supervisor", "The supervisor stopped watching; the apps it runs keep running and are taken back when it starts again");
        }

        std::vector<RestartRequiredOption> GetRestartRequiredOptions() const override
        {
            constexpr std::string_view AtStart = "The supervisor reads it when it starts watching, so a change takes effect at its next start";
            constexpr std::string_view PanelStore = "The panel opens its store when it starts, so a change takes effect at its next start";
            return { { "Supervisor.Apps", AtStart }, { "Supervisor.StateFile", AtStart }, { "Supervisor.OutputDir", AtStart }, { "Supervisor.OutputMaxBytes", AtStart },
                { "App.*", AtStart }, { "Panel.StoreFile", PanelStore } };
        }

        void OnStatus(std::vector<std::pair<std::string, std::string>>& fields) override
        {
            for (AppSnapshot const& app : _supervisor.Snapshots())
                fields.emplace_back(app.Name, app.ProcessId ? fmt::format("{} (process {})", ManagedApp::StateName(app.State), *app.ProcessId) : std::string(ManagedApp::StateName(app.State)));
        }

    private:
        void RegisterCommands()
        {
            Commands().Register({ "panel user list", "", "list the panel's operators", false,
                [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                {
                    if (!arguments.empty())
                        return false;
                    std::string error;
                    std::vector<PanelUser> const users = _panel.Users().List(error);
                    if (!error.empty())
                    {
                        reply(error);
                        return true;
                    }
                    if (users.empty())
                        reply("The panel has no operator yet; its console printed a one-time link when it started");
                    for (PanelUser const& user : users)
                        reply(fmt::format("{}{}{} last signed in {}", user.Username, user.IsOwner ? " (owner)" : "", user.Disabled ? " (disabled)" : "",
                            user.SignedInEpochMs ? WhenText(*user.SignedInEpochMs) : std::string("never")));
                    return true;
                } });
            Commands().Register({ "panel user create", "<name>", "make an operator and print a one-time link they set their password from", false,
                [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                {
                    if (arguments.size() != 1)
                        return false;
                    std::string error;
                    std::string const password = PanelUsers::Unguessable();
                    int64 id = 0;
                    PanelUserResult const made = _panel.Users().Create(arguments[0], password, false, true, &id, error);
                    if (made != PanelUserResult::Ok)
                    {
                        reply(fmt::format("{} was not made: {}", arguments[0], made == PanelUserResult::StoreFailed ? error : std::string(PanelUsers::Explain(made))));
                        return true;
                    }
                    reply(fmt::format("{} was made. They set their own password once, from this machine, within 30 minutes: {}",
                        arguments[0], _panel.LinkFor(_panel.MintPasswordLink(id))));
                    return true;
                } });
            Commands().Register({ "panel user reset-password", "<name>", "print a one-time link that operator sets a new password from", false,
                [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                {
                    if (arguments.size() != 1)
                        return false;
                    std::string error;
                    std::optional<PanelUser> const user = _panel.Users().Find(arguments[0], error);
                    if (!user)
                    {
                        reply(error.empty() ? fmt::format("The panel has no operator named {}", arguments[0]) : error);
                        return true;
                    }
                    reply(fmt::format("{} sets a new password once, from this machine, within 30 minutes: {}",
                        user->Username, _panel.LinkFor(_panel.MintPasswordLink(user->Id))));
                    return true;
                } });
            Commands().Register({ "panel user disable", "<name>", "stop an operator signing in and end the sessions they have", false,
                [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                {
                    if (arguments.size() != 1)
                        return false;
                    std::string error;
                    std::optional<PanelUser> const user = _panel.Users().Find(arguments[0], error);
                    if (!user)
                    {
                        reply(error.empty() ? fmt::format("The panel has no operator named {}", arguments[0]) : error);
                        return true;
                    }
                    if (!_panel.Users().SetDisabled(user->Id, true, error))
                    {
                        reply(error.empty() ? fmt::format("{} was not disabled", user->Username) : error);
                        return true;
                    }
                    _panel.Sessions().CloseEveryOne(user->Id, "the operator was disabled", error);
                    reply(fmt::format("{} can no longer sign in, and the sessions they had have ended", user->Username));
                    return true;
                } });
            Commands().Register({ "panel user enable", "<name>", "let a disabled operator sign in again", false,
                [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                {
                    if (arguments.size() != 1)
                        return false;
                    std::string error;
                    std::optional<PanelUser> const user = _panel.Users().Find(arguments[0], error);
                    if (!user)
                    {
                        reply(error.empty() ? fmt::format("The panel has no operator named {}", arguments[0]) : error);
                        return true;
                    }
                    if (!_panel.Users().SetDisabled(user->Id, false, error))
                    {
                        reply(error.empty() ? fmt::format("{} was not enabled", user->Username) : error);
                        return true;
                    }
                    reply(fmt::format("{} can sign in again", user->Username));
                    return true;
                } });
            Commands().Register({ "apps", "", "list the apps the supervisor runs and their state", false,
                [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                {
                    if (!arguments.empty())
                        return false;
                    std::vector<AppSnapshot> const apps = _supervisor.Snapshots();
                    if (apps.empty())
                        reply("The supervisor runs no app");
                    for (AppSnapshot const& app : apps)
                        reply(fmt::format("{:<14}{:<10}{:<12}crashes {}{}", app.Name, ManagedApp::StateName(app.State), app.ProcessId ? fmt::format("process {}", *app.ProcessId) : std::string("-"), app.Crashes,
                            app.Message.empty() ? std::string() : fmt::format("  {}", app.Message)));
                    return true;
                } });
            RegisterPower("start", "<app>", "start an app", PowerAction::Start, false);
            RegisterPower("stop", "<app> [seconds]", "stop an app gracefully, now or after a countdown", PowerAction::Stop, true);
            RegisterPower("restart", "<app> [seconds]", "restart an app gracefully, now or after a countdown", PowerAction::Restart, true);
            RegisterPower("kill", "<app>", "end an app's whole process tree at once", PowerAction::Kill, false);
        }

        void RegisterPower(std::string name, std::string usage, std::string help, PowerAction action, bool countdown)
        {
            Commands().Register({ std::move(name), std::move(usage), std::move(help), false,
                [this, action, countdown](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
                {
                    if (arguments.empty() || arguments.size() > (countdown ? 2u : 1u))
                        return false;
                    uint32 seconds = 0;
                    if (arguments.size() == 2)
                    {
                        std::optional<uint32> const parsed = ParseCountdown(arguments[1]);
                        if (!parsed)
                            return false;
                        seconds = *parsed;
                    }
                    PowerResult const result = _supervisor.Power(arguments[0], action, seconds);
                    reply(result.Message);
                    return true;
                } });
        }

        Supervisor _supervisor;
        Panel _panel;
    };
}

int main(int argc, char** argv)
{
    std::vector<std::string> const arguments = Ambrose::GetArguments(argc, argv);
    if (arguments.size() >= 2 && arguments[1] == ConsoleBreakOption)
        return SendConsoleBreak(arguments);
    if (std::find(arguments.begin(), arguments.end(), std::string(SelfSignedOption)) != arguments.end())
        return WriteSelfSignedCertificate(arguments);
    SupervisorApp app;
    return app.Run(arguments);
}
