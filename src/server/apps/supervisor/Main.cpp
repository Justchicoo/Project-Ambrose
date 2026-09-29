/*
 * Project Ambrose by Imjustchico
 * Supervisor entry point: with --console-break and a process group it only sends Ctrl+Break to that group's console and exits, which is how it interrupts an app on Windows without leaving its own console; otherwise it runs as an app of its own that starts, takes back and watches the apps Supervisor.Apps names, only checking their definitions and its saved state under --check so a check leaves no app running, serves the panel, the supervisor routes and the file roots on its admin API and the panel's listener alike, keeps its own live settings in the panel store when that is open and in config alone when it is not, hands a change of the minimum free space to the space guard, rebuilds the file roots on a configuration change, a reload of file_roots or a change of an owner's protected patterns, records relayed settings and reload answers, refused file paths and its own secret reveals in the panel's audit log, runs every relayed change inside a panel audit record while the panel store is open, as it runs them unrecorded by a panel that is off, and caps a relayed command at the level the caller's grants allow, the admin token keeping the top level, publishes every state an app passes through on the panel's status stream at that app's scope and gives the panel's event socket its list of apps, reloads the panel's own listener and its two-factor rules when a Panel option changes, offers apps, start, stop, restart and kill on its console with the panel's operators beside them, including the way back in for an operator who lost their authenticator and their recovery codes, and leaves the apps running when it stops so the next start takes them back.
 */

#include "AdminCapabilities.h"
#include "AdminGraphsView.h"
#include "AdminServer.h"
#include "AdminStatus.h"
#include "ChildProcess.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "Duration.h"
#include "Environment.h"
#include "FileRoots.h"
#include "FilesService.h"
#include "Log.h"
#include "AppOptions.h"
#include "Panel.h"
#include "PanelUsers.h"
#include "PublishedSampler.h"
#include "ReloadMgr.h"
#include "ResourceSampler.h"
#include "SeriesStore.h"
#include "ServerApp.h"
#include "ServiceInstaller.h"
#include "Settings.h"
#include "SourceFolder.h"
#include "SpaceGuard.h"
#include "StringUtil.h"
#include "Supervisor.h"
#include "TlsCertificate.h"

#include <fmt/format.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <chrono>
#include <memory>
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
    constexpr std::string_view InstallServiceOption = "--install-service";
    constexpr std::string_view UninstallServiceOption = "--uninstall-service";

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

        SupervisorApp() : ServerApp({ "supervisor", "supervisor.conf", DefaultAdminPort }, sConfigMgr, sLog, std::cout, std::cerr), _supervisor(sLog, BreakThroughThisProgram()), _panel(sLog, ClientLocator::GetDataFolder(LocalClientSystem()), sConfigMgr.GetFilename().parent_path()), _files(_roots, _space, FileHooks())
        {
            RegisterCommands();
        }

        ~SupervisorApp() override
        {
            _supervisor.SetStatusObserver({});
            _supervisor.Shutdown();
        }

        SupervisorApp(SupervisorApp const&) = delete;
        SupervisorApp& operator=(SupervisorApp const&) = delete;

    protected:
        uint8 GetSettingApps() const override
        {
            return SettingApps::Supervisor;
        }

        void OnSecretsRevealed(AdminRequest const& request, std::vector<std::string> const& keys) override
        {
            ServerApp::OnSecretsRevealed(request, keys);
            _panel.RecordReveal(request, GetInfo().Name, keys);
        }

        void OnAdminApiReady(AdminServer& admin) override
        {
            _supervisor.SetRelayHooks({ [this](AdminRequest const& request) { return _panel.NameOf(request); },
                [this](AdminRequest const& request, RelayedAnswer const& answer) { _panel.RecordRelayed(request, answer.App, answer.Method, answer.Path, answer.Status, answer.Body); } });
            _supervisor.Register(admin.Routes(), [this] { return BuildStatus(); });
            AdminGraphsView::Register(admin.Routes(), [this]() -> Ambrose::SeriesStore const& { return _history; });
            _files.Register(admin.Routes());
        }

        bool OnStart() override
        {
            std::vector<std::string> problems;
            LocalClientSystem const system;
            std::error_code code;
            std::filesystem::path const working = std::filesystem::current_path(code);
            SupervisorSettings const settings = SupervisorSettings::Load(Config(), ClientLocator::GetDataFolder(system), Ambrose::GetExecutableDirectory(), working, problems);
            _supervisor.SetStatusObserver([this](AppSnapshot const& snapshot)
            {
                _panel.Events().Publish("status", "status", snapshot.Name, Supervisor::StatusData(snapshot), snapshot.Name);
            });
            _panel.SetAppSource([this]
            {
                std::vector<std::string> names;
                for (AppSnapshot const& snapshot : _supervisor.Snapshots())
                    names.push_back(snapshot.Name);
                return names;
            });
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
            _historyFile = settings.HistoryFile;
            _sampleInterval = settings.SampleInterval;
            _saveInterval = settings.SaveInterval;
            _sampler = std::make_unique<ResourceSampler>(_history);
            _published = std::make_unique<PublishedSampler>(_history, [this](std::string const& app)
                { return _supervisor.AskApp(app, "/api/metrics", PublishedTimeout); });
            _lastSave = std::chrono::steady_clock::now();
            std::string historyError;
            if (_history.Load(_historyFile, historyError))
                LOG_INFO("server.supervisor", "Read {} series of history from {}", _history.Count(), ConfigMgr::PathToUtf8(_historyFile));
            else
                LOG_INFO("server.supervisor", "Starting with no history: {}", historyError);
            RegisterStandardRoutes(_panel.Routes());
            AdminGraphsView::Register(_panel.Routes(), [this]() -> Ambrose::SeriesStore const& { return _history; });
            _supervisor.SetAuditRecorder([this](AdminRequest const& request, std::string_view app, std::string_view action, std::function<AdminResponse()> operation)
            {
                if (!_panel.IsStoreOpen())
                    return operation();
                return _panel.AuditRequest(request, app, action, std::move(operation));
            });
            _supervisor.SetCommandContext([this](AdminRequest const& request)
            {
                return request.Principal == "token" ? uint8(4) : _panel.CommandLevel(request);
            }, [this](AdminRequest const& request)
            {
                return _panel.CommandActorName(request);
            });
            _supervisor.Register(_panel.Routes(), [this] { return BuildStatus(); });
            _files.Register(_panel.Routes());
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
            if (!_panel.IsStoreOpen())
                LOG_INFO("server.settings", "The panel store is not open (Panel.Enable = 0), so the supervisor's live settings resolve from its config alone and a live change is refused");
            else if (!StartSettings(_panel.LiveSettingStore()))
                LOG_WARN("server.settings", "The supervisor's live settings resolve from its config alone until the panel store's settings can be read");
            _files.Tune();
            _settingsSubscription = sSettings.Subscribe([this](SettingChange const& change)
            {
                if (change.Key.starts_with("Files.MinFree"))
                    _files.Tune();
            });
            std::vector<std::string> rootErrors;
            if (!RebuildRoots(rootErrors))
                for (std::string const& problem : rootErrors)
                    LOG_WARN("server.files", "The file roots could not be built: {}", problem);
            sReloadMgr.Register(std::string(FileRootsTarget), [this](std::vector<std::string>& found) { return RebuildRoots(found); }, { "config" });
            sAdminCapabilities.AddReloadTarget(std::string(FileRootsTarget));
            return true;
        }

        std::chrono::milliseconds GetUpdateInterval() const override
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(_sampleInterval);
        }

        void OnUpdate(std::chrono::milliseconds) override
        {
            if (!_sampler)
                return;
            int64 const now = static_cast<int64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            std::vector<AppSnapshot> const apps = _supervisor.Snapshots();
            _sampler->Sample(apps, now);
            if (_published)
                _published->Sample(apps, now);
            if (std::chrono::steady_clock::now() - _lastSave < _saveInterval)
                return;
            _lastSave = std::chrono::steady_clock::now();
            SaveHistory();
        }

        void SaveHistory()
        {
            if (_historyFile.empty())
                return;
            std::string error;
            if (!_history.Save(_historyFile, error))
                LOG_WARN("server.supervisor", "The history was not written to {}: {}", ConfigMgr::PathToUtf8(_historyFile), error);
        }

        void OnStop() override
        {
            if (_settingsSubscription != 0)
                sSettings.Unsubscribe(_settingsSubscription);
            _settingsSubscription = 0;
            SaveHistory();
            _panel.Stop();
            _supervisor.Shutdown();
            LOG_INFO("server.supervisor", "The supervisor stopped watching; the apps it runs keep running and are taken back when it starts again");
        }

        std::vector<RestartRequiredOption> GetRestartRequiredOptions() const override
        {
            constexpr std::string_view AtStart = "The supervisor reads it when it starts watching, so a change takes effect at its next start";
            constexpr std::string_view PanelStore = "The panel opens its store when it starts, so a change takes effect at its next start";
            constexpr std::string_view PanelKeyring = "The panel opens its keyring when it starts, so a change takes effect at its next start";
            return { { "Supervisor.Apps", AtStart }, { "Supervisor.StateFile", AtStart }, { "Supervisor.OutputDir", AtStart }, { "Supervisor.OutputMaxBytes", AtStart },
                { "App.*", AtStart }, { "Panel.StoreFile", PanelStore }, { "Panel.KeyringFile", PanelKeyring } };
        }

        void OnConfigChanged(std::vector<std::string> const& changed) override
        {
            ServerApp::OnConfigChanged(changed);
            if (std::any_of(changed.begin(), changed.end(), [](std::string const& key) { return key.starts_with("Panel."); }) && !_panel.Reload(Config()))
                LOG_WARN("server.panel", "The panel kept its earlier settings; the reason is logged above");
            if (std::all_of(changed.begin(), changed.end(), [](std::string const& key) { return key.starts_with("Files."); }))
                return;
            std::vector<std::string> errors;
            if (!RebuildRoots(errors))
                for (std::string const& problem : errors)
                    LOG_WARN("server.files", "The file roots were not rebuilt after the configuration changed, and the ones serving go on serving: {}", problem);
        }

        void OnStatus(std::vector<std::pair<std::string, std::string>>& fields) override
        {
            for (AppSnapshot const& app : _supervisor.Snapshots())
                fields.emplace_back(app.Name, app.ProcessId ? fmt::format("{} (process {})", ManagedApp::StateName(app.State), *app.ProcessId) : std::string(ManagedApp::StateName(app.State)));
        }

    private:
        static constexpr std::string_view FileRootsTarget = "file_roots";

        FilesHooks FileHooks()
        {
            FilesHooks hooks;
            hooks.Record = [this](AuditEvent const& event, std::string& error) { return _panel.Record(event, {}, error); };
            hooks.NameOf = [this](AdminRequest const& request) { return _panel.NameOf(request); };
            hooks.SaveRules = [this](AdminRequest const& request, AuditEvent const& event, std::string const& root, std::vector<std::string> const& patterns, std::string& error)
            {
                return _panel.SaveFileRules(request, event, root, patterns, error);
            };
            hooks.Rebuild = [](std::vector<std::string>& errors)
            {
                ReloadOutcome const outcome = sReloadMgr.Reload(FileRootsTarget);
                errors = outcome.Errors;
                return outcome.Ok;
            };
            hooks.Log = [](std::string const& line) { LOG_WARN("server.files", "{}", line); };
            return hooks;
        }

        bool RebuildRoots(std::vector<std::string>& errors)
        {
            LocalClientSystem const system;
            FileRootPlaces places;
            places.DataFolder = ClientLocator::GetDataFolder(system);
            places.ExecutableFolder = Ambrose::GetExecutableDirectory();
            places.SourceFolder = Ambrose::FindSourceFolder();
            std::error_code code;
            places.WorkingFolder = std::filesystem::current_path(code);
            places.StoreFile = Panel::StoreFile(Config(), places.DataFolder);
            for (ClientCandidate const& candidate : ClientLocator::FindInstalls(system))
                places.ClientInstalls.push_back(candidate.Install.Root);
            std::vector<std::string> problems;
            FileRootInputs inputs = FileRootInputs::Read(Config(), places, problems);
            for (std::string const& problem : problems)
                LOG_WARN("server.files", "{}", problem);
            std::string error;
            if (!_panel.ReadFileRules(inputs.OperatorRules, error))
            {
                errors.push_back(fmt::format("the protected patterns could not be read from the panel store: {}", error));
                return false;
            }
            if (!_roots.Rebuild(inputs, errors))
                return false;
            FileRoots::Snapshot const set = _roots.Get();
            for (std::string const& note : set->Notes)
                LOG_WARN("server.files", "{}", note);
            std::size_t const present = static_cast<std::size_t>(std::count_if(set->Roots.begin(), set->Roots.end(), [](FileRoot const& root) { return root.Present(); }));
            LOG_INFO("server.files", "{} file root(s) are open to the panel and {} are missing, generation {}", present, set->Roots.size() - present, _roots.GetGeneration());
            return true;
        }

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
                        reply(fmt::format("{}{}{}{} last signed in {}", user.Username, user.IsOwner ? " (owner)" : "", user.Disabled ? " (disabled)" : "",
                            user.TwoFactor ? " (two-factor)" : "", user.SignedInEpochMs ? WhenText(*user.SignedInEpochMs) : std::string("never")));
                    return true;
                } });
            Commands().Register({ "panel user reset-two-factor", "<name>", "turn off an operator's two-factor sign-in and end their sessions, for one who lost their authenticator and codes", false,
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
                    if (!user->TwoFactor)
                    {
                        reply(fmt::format("{} has no two-factor sign-in to reset", user->Username));
                        return true;
                    }
                    if (!_panel.ResetTwoFactor(*user, "console", error))
                    {
                        reply(fmt::format("Two-factor sign-in was not reset for {}: {}", user->Username, error));
                        return true;
                    }
                    reply(fmt::format("{} signs in with their password alone until they turn two-factor sign-in on again, and the sessions they had have ended", user->Username));
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

        static constexpr std::chrono::milliseconds PublishedTimeout{ 1500 };

        Supervisor _supervisor;
        Panel _panel;
        Ambrose::SeriesStore _history;
        std::unique_ptr<ResourceSampler> _sampler;
        std::unique_ptr<PublishedSampler> _published;
        std::filesystem::path _historyFile;
        std::chrono::seconds _sampleInterval{ 5 };
        std::chrono::seconds _saveInterval{ 300 };
        std::chrono::steady_clock::time_point _lastSave{};
        FileRoots _roots;
        Ambrose::SpaceGuard _space;
        FilesService _files;
        uint64 _settingsSubscription = 0;
    };
}

int main(int argc, char** argv)
{
    std::vector<std::string> const arguments = Ambrose::GetArguments(argc, argv);
    if (std::find(arguments.begin(), arguments.end(), std::string(InstallServiceOption)) != arguments.end())
        return SupervisorService::Install(arguments);
    if (std::find(arguments.begin(), arguments.end(), std::string(UninstallServiceOption)) != arguments.end())
        return SupervisorService::Uninstall();
    if (arguments.size() >= 2 && arguments[1] == ConsoleBreakOption)
        return SendConsoleBreak(arguments);
    if (std::find(arguments.begin(), arguments.end(), std::string(SelfSignedOption)) != arguments.end())
        return WriteSelfSignedCertificate(arguments);
    SupervisorApp app;
    if (std::find(arguments.begin(), arguments.end(), "--service") != arguments.end())
        return SupervisorService::Run(arguments, [&app](std::vector<std::string> const& normal) { return app.Run(normal); }, [&app] { app.RequestStop("service stop"); });
    return app.Run(arguments);
}
