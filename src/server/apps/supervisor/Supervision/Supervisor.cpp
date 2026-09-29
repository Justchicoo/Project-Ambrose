/*
 * Project Ambrose by Imjustchico
 * Keeps the apps behind a shared lock that only starting and stopping the supervisor takes alone, so a request always finds a whole list; the saved state and each app's output live under the data folder unless Supervisor.StateFile or Supervisor.OutputDir says otherwise; a power request names its action and, for a stop or restart, a countdown, and anything else in it is refused field by field; a relayed request keeps its method, path, query, body and request id, a settings request also names its caller and the rights they hold that the route can use, a settings batch pays its cost on the panel's limit first, and each relayed settings or reload answer is handed on to be recorded; and is let through only for a method and path whose permission the relay knows, the same one the app's own route asks for, so a caller who could not reach a route on the app cannot reach it through the supervisor and a path the relay does not know is answered as absent, each judged by the listener the request came in on, since the admin token on the supervisor's own API and a panel user's session on the panel are different callers; sessions stay the supervisor's own and are never relayed, and an app that is not running or has its admin API off is answered 503 with the reason. Each app is given the forwarder to the status observer before it starts watching, so the first state it enters is handed on too. A permission that is allowed is followed by the listener's own fresh-check rule before it is used, and a read that asks to reveal secrets by a caller who may see them, or a change naming a setting the declarations mark restricted by a caller who may change those, asks for a check every time, a dry run excepted since it changes nothing.
 */

#include "Supervisor.h"
#include "AdminConfigView.h"
#include "AdminRouter.h"
#include "AdminSettingsView.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "SettingDeclarations.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <mutex>

namespace
{
    nlohmann::json OptionalNumber(int64 value)
    {
        return value == 0 ? nlohmann::json(nullptr) : nlohmann::json(value);
    }

    nlohmann::json TextOrNull(std::string const& text)
    {
        return text.empty() ? nlohmann::json(nullptr) : nlohmann::json(text);
    }

    bool IsRestricted(std::string_view key)
    {
        SettingDeclaration const* const declared = SettingDeclarations::Find(key);
        return declared && declared->Edit == SettingEditClass::Restricted;
    }

    bool NamesRestricted(AdminRequest const& request, std::string_view method, std::string_view tail)
    {
        constexpr std::string_view SettingPrefix = "/api/settings/";
        if (tail.starts_with(SettingPrefix) && tail != "/api/settings/batch")
        {
            std::string_view const key = tail.substr(SettingPrefix.size());
            return (method == "PUT" || method == "DELETE") && key.find('/') == std::string_view::npos && IsRestricted(key);
        }
        if (tail != "/api/settings/batch" && tail != "/api/settings")
            return false;
        nlohmann::json const body = nlohmann::json::parse(request.Body, nullptr, false);
        if (!body.is_object())
            return false;
        if (auto const dry = body.find("dry_run"); dry != body.end() && dry->is_boolean() && dry->get<bool>())
            return false;
        if (auto const entries = body.find("entries"); entries != body.end() && entries->is_array())
            for (nlohmann::json const& entry : *entries)
                if (entry.is_object() && entry.contains("key") && entry["key"].is_string() && IsRestricted(entry["key"].get_ref<std::string const&>()))
                    return true;
        if (auto const values = body.find("values"); values != body.end() && values->is_object())
            for (auto const& [key, value] : values->items())
                if (IsRestricted(key))
                    return true;
        return false;
    }

    nlohmann::json ExitJson(AppExit const& exit)
    {
        nlohmann::json body;
        body["epoch_ms"] = exit.EpochMs;
        body["code"] = exit.Code ? nlohmann::json(*exit.Code) : nlohmann::json(nullptr);
        body["signal"] = exit.Signal ? nlohmann::json(*exit.Signal) : nlohmann::json(nullptr);
        body["requested"] = exit.Requested;
        body["during"] = std::string(ManagedApp::StateName(exit.During));
        body["uptime_ms"] = exit.UptimeMs;
        return body;
    }

    nlohmann::json SnapshotJson(AppSnapshot const& snapshot)
    {
        nlohmann::json body;
        body["name"] = snapshot.Name;
        body["program"] = ConfigMgr::PathToUtf8(snapshot.Program);
        body["config"] = ConfigMgr::PathToUtf8(snapshot.Config);
        body["state"] = std::string(ManagedApp::StateName(snapshot.State));
        body["watching"] = snapshot.Watching;
        body["desired"] = snapshot.WantRunning ? "running" : "stopped";
        body["pid"] = snapshot.ProcessId ? nlohmann::json(*snapshot.ProcessId) : nlohmann::json(nullptr);
        body["adopted"] = snapshot.Adopted;
        body["started_epoch_ms"] = OptionalNumber(snapshot.StartedEpochMs);
        body["ready_epoch_ms"] = OptionalNumber(snapshot.ReadyEpochMs);
        if (snapshot.StartStage.empty())
            body["start"] = nullptr;
        else
            body["start"] = { { "stage", snapshot.StartStage }, { "until_ms", snapshot.StartUntilEpochMs } };
        body["admin"] = {
            { "enabled", snapshot.AdminEnabled },
            { "address", TextOrNull(snapshot.AdminHost) },
            { "port", snapshot.AdminPort == 0 ? nlohmann::json(nullptr) : nlohmann::json(snapshot.AdminPort) },
            { "problem", TextOrNull(snapshot.AdminProblem) }
        };
        if (snapshot.Stop == StopMethod::None)
            body["stop"] = nullptr;
        else
            body["stop"] = { { "method", std::string(ManagedApp::StopMethodName(snapshot.Stop)) }, { "requested_epoch_ms", snapshot.StopRequestedEpochMs } };
        body["restart_epoch_ms"] = OptionalNumber(snapshot.RestartEpochMs);
        body["crashes"] = snapshot.Crashes;
        body["failed_starts"] = snapshot.FailedStarts;
        body["restarts"] = snapshot.Restarts;
        nlohmann::json exits = nlohmann::json::array();
        for (AppExit const& exit : snapshot.Exits)
            exits.push_back(ExitJson(exit));
        body["last_exit"] = snapshot.Exits.empty() ? nlohmann::json(nullptr) : ExitJson(snapshot.Exits.back());
        body["exits"] = std::move(exits);
        body["message"] = TextOrNull(snapshot.Message);
        return body;
    }

    std::filesystem::path ConfiguredPath(ConfigMgr const& config, std::string const& key, std::filesystem::path const& fallback, std::filesystem::path const& workingFolder)
    {
        std::filesystem::path const value = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key, "", true)));
        if (value.empty())
            return fallback;
        return (value.is_absolute() ? value : workingFolder / value).lexically_normal();
    }
}

SupervisorSettings SupervisorSettings::Load(ConfigMgr const& config, std::filesystem::path dataFolder, std::filesystem::path programFolder, std::filesystem::path workingFolder, std::vector<std::string>& problems)
{
    SupervisorSettings settings;
    settings.DataFolder = std::move(dataFolder);
    settings.ProgramFolder = std::move(programFolder);
    settings.WorkingFolder = std::move(workingFolder);
    std::filesystem::path const home = (settings.DataFolder.empty() ? config.GetFilename().parent_path() : settings.DataFolder) / "supervisor";
    settings.StateFile = ConfiguredPath(config, "Supervisor.StateFile", home / "state.json", settings.WorkingFolder);
    settings.HistoryFile = ConfiguredPath(config, "Supervisor.HistoryFile", home / "history.bin", settings.WorkingFolder);
    settings.OutputFolder = ConfiguredPath(config, "Supervisor.OutputDir", home / "output", settings.WorkingFolder);
    int64 const sampleSeconds = config.GetOption<int64>("Supervisor.SampleSeconds", 5, true);
    settings.SampleInterval = std::chrono::seconds(std::clamp<int64>(sampleSeconds, 1, 300));
    if (settings.SampleInterval.count() != sampleSeconds)
        problems.push_back(fmt::format("Supervisor.SampleSeconds is {}, outside 1 to 300; using {}", sampleSeconds, settings.SampleInterval.count()));
    int64 const saveSeconds = config.GetOption<int64>("Supervisor.HistorySaveSeconds", 300, true);
    settings.SaveInterval = std::chrono::seconds(std::clamp<int64>(saveSeconds, 10, 3600));
    if (settings.SaveInterval.count() != saveSeconds)
        problems.push_back(fmt::format("Supervisor.HistorySaveSeconds is {}, outside 10 to 3600; using {}", saveSeconds, settings.SaveInterval.count()));
    uint64 const bytes = config.GetOption<uint64>("Supervisor.OutputMaxBytes", OutputLog::DefaultMaxFileBytes, true);
    settings.MaxOutputBytes = std::clamp<uint64>(bytes, OutputLog::MinMaxFileBytes, MaxOutputBytesLimit);
    if (settings.MaxOutputBytes != bytes)
        problems.push_back(fmt::format("Supervisor.OutputMaxBytes is {}, outside {} to {}; using {}", bytes, OutputLog::MinMaxFileBytes, MaxOutputBytesLimit, settings.MaxOutputBytes));
    return settings;
}

Supervisor::Supervisor(Log& log, ChildBreakSender sendBreak) : _log(log), _sendBreak(std::move(sendBreak))
{
}

Supervisor::~Supervisor()
{
    Shutdown();
}

bool Supervisor::Start(ConfigMgr const& config, SupervisorSettings const& settings, bool watch, std::vector<std::string>& problems, std::string& error)
{
    std::vector<AppDefinition> definitions = AppDefinition::Load(config, settings.ProgramFolder, settings.WorkingFolder, problems);
    auto state = std::make_unique<SupervisorState>(settings.StateFile);
    if (!state->Load(error))
        return false;
    std::vector<std::unique_ptr<ManagedApp>> apps;
    for (AppDefinition& definition : definitions)
    {
        apps.push_back(std::make_unique<ManagedApp>(std::move(definition), *state, settings.OutputFolder, settings.MaxOutputBytes, settings.DataFolder, _sendBreak, _log));
        apps.back()->SetStatusObserver([this](AppSnapshot const& snapshot) { ForwardStatus(snapshot); });
    }
    {
        std::unique_lock<std::shared_mutex> const lock(_mutex);
        _state = std::move(state);
        _apps = std::move(apps);
    }
    if (!watch)
        return true;
    std::shared_lock<std::shared_mutex> const lock(_mutex);
    for (std::unique_ptr<ManagedApp> const& app : _apps)
        app->Start();
    return true;
}

void Supervisor::Shutdown()
{
    std::shared_lock<std::shared_mutex> const lock(_mutex);
    for (std::unique_ptr<ManagedApp> const& app : _apps)
        app->Shutdown();
}

ManagedApp* Supervisor::Find(std::string_view name) const
{
    for (std::unique_ptr<ManagedApp> const& app : _apps)
        if (app->GetDefinition().Name == name)
            return app.get();
    return nullptr;
}

PowerResult Supervisor::Power(std::string_view name, PowerAction action, uint32 countdownSeconds)
{
    std::shared_lock<std::shared_mutex> const lock(_mutex);
    ManagedApp* const app = Find(name);
    if (!app)
        return { false, 404, "unknown_app", fmt::format("The supervisor runs no app named {}", name) };
    return app->Power(action, countdownSeconds);
}

std::vector<AppSnapshot> Supervisor::Snapshots() const
{
    std::shared_lock<std::shared_mutex> const lock(_mutex);
    std::vector<AppSnapshot> snapshots;
    snapshots.reserve(_apps.size());
    for (std::unique_ptr<ManagedApp> const& app : _apps)
        snapshots.push_back(app->Snapshot());
    return snapshots;
}

std::vector<OutputLine> Supervisor::Output(std::string_view name, OutputRun run, uint64 after) const
{
    std::shared_lock<std::shared_mutex> const lock(_mutex);
    ManagedApp const* const app = Find(name);
    return app ? app->Output(run, after) : std::vector<OutputLine>();
}

std::string Supervisor::SupervisionJson(std::vector<AppSnapshot> const& snapshots)
{
    nlohmann::json apps = nlohmann::json::array();
    for (AppSnapshot const& snapshot : snapshots)
        apps.push_back(SnapshotJson(snapshot));
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["apps"] = std::move(apps);
    return body.dump();
}

std::string Supervisor::AppsJson(AdminStatusSnapshot const& self, std::vector<AppSnapshot> const& snapshots)
{
    nlohmann::json list = nlohmann::json::parse(AdminStatus::AppsJson(self), nullptr, false);
    if (!list.is_array())
        list = nlohmann::json::array();
    for (nlohmann::json& entry : list)
        entry["supervision"] = nullptr;
    for (AppSnapshot const& snapshot : snapshots)
    {
        nlohmann::json entry;
        entry["name"] = snapshot.Name;
        entry["role"] = snapshot.Identity.Role.empty() ? snapshot.ProgramName : snapshot.Identity.Role;
        entry["realm"] = snapshot.Identity.Realm;
        entry["address"] = snapshot.Identity.Address;
        entry["port"] = snapshot.Identity.Port;
        entry["revision"] = snapshot.Identity.Revision;
        entry["supervision"] = SnapshotJson(snapshot);
        list.push_back(std::move(entry));
    }
    return list.dump();
}

std::string Supervisor::OutputJson(std::string_view name, OutputRun run, std::vector<OutputLine> const& lines)
{
    nlohmann::json list = nlohmann::json::array();
    for (OutputLine const& line : lines)
        list.push_back({ { "seq", line.Sequence }, { "stream", line.Stream }, { "text", line.Text }, { "epoch_ms", line.EpochMs == 0 ? nlohmann::json(nullptr) : nlohmann::json(line.EpochMs) } });
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["app"] = std::string(name);
    body["run"] = run == OutputRun::Current ? "current" : "previous";
    body["lines"] = std::move(list);
    return body.dump();
}

void Supervisor::Register(AdminRouter& router, std::function<AdminStatusSnapshot()> self)
{
    router.AddGuarded("GET", "/api/apps", "status.read", [this, self](AdminRequest const&) { return AdminResponse::Json(200, AppsJson(self(), Snapshots())); });
    router.AddGuarded("GET", "/api/supervisor", "status.read", [this](AdminRequest const&) { return AdminResponse::Json(200, SupervisionJson(Snapshots())); });
    for (char const* method : { "GET", "POST", "PUT", "PATCH", "DELETE" })
        router.AddGuardedPrefix(method, "/api/apps/", "status.read", [this, &router](AdminRequest const& request) { return Answer(request, router); });
}

std::optional<std::string_view> Supervisor::PermissionFor(std::string_view method, std::string_view tail) noexcept
{
    bool const read = method == "GET";
    if (tail == "/api/command")
        return method == "POST" ? std::optional<std::string_view>("console.write") : std::nullopt;
    if (tail == "/api/tick-profile")
        return (method == "GET" || method == "POST") ? std::optional<std::string_view>("metrics.profile") : std::nullopt;
    if (tail == "/api/tick-profile/trace")
        return method == "GET" ? std::optional<std::string_view>("metrics.profile") : std::nullopt;
    if (tail.starts_with("/api/logs/after/"))
        return read ? std::optional<std::string_view>("console.read") : std::nullopt;
    if (tail == "/api/settings")
        return read ? std::optional<std::string_view>("settings.read") : method == "PATCH" || method == "PUT" ? std::optional<std::string_view>("settings.edit") : std::nullopt;
    if (tail == "/api/settings/batch")
        return method == "POST" ? std::optional<std::string_view>("settings.edit") : std::nullopt;
    if (tail.starts_with("/api/settings/"))
    {
        std::string_view const key = tail.substr(std::string_view("/api/settings/").size());
        if (key.ends_with("/history") && key.size() > std::string_view("/history").size())
            return read ? std::optional<std::string_view>("settings.read") : std::nullopt;
        if (!key.empty() && key.find('/') == std::string_view::npos)
            return method == "PUT" || method == "DELETE" ? std::optional<std::string_view>("settings.edit") : std::nullopt;
        return std::nullopt;
    }
    if (tail.starts_with("/api/events/after/"))
        return read ? std::optional<std::string_view>("settings.read") : std::nullopt;
    if (tail == "/api/database" || tail == "/api/database/updates")
        return read ? std::optional<std::string_view>("database.read") : std::nullopt;
    if (tail == "/api/database/apply")
        return method == "POST" ? std::optional<std::string_view>("updates.apply") : std::nullopt;
    if (tail == "/api/database/reload")
        return method == "POST" ? std::optional<std::string_view>("reload.run") : std::nullopt;
    if (tail == "/api/reload")
        return read ? std::optional<std::string_view>("reload.read") : std::nullopt;
    if (tail.starts_with("/api/reload/"))
        return method == "POST" ? std::optional<std::string_view>("reload.run") : std::nullopt;
    if (tail == "/api/shutdown")
        return method == "POST" ? std::optional<std::string_view>("power.stop") : std::nullopt;
    if (!read)
        return std::nullopt;
    if (tail == "/api/realms")
        return "realms.read";
    if (tail == "/api/players")
        return "players.read";
    if (tail == "/api/client")
        return "clientdata.read";
    if (tail == "/api/activity")
        return "activity.read";
    if (tail == "/api/metrics" || tail == "/metrics")
        return "metrics.read";
    if (tail == "/api/status" || tail == "/api/apps" || tail == "/api/errors")
        return "status.read";
    return std::nullopt;
}

std::optional<AdminResponse> Supervisor::Refuse(AdminRequest const& request, std::string_view permission, AdminRouter const& router)
{
    switch (router.MayI(request, permission))
    {
        case PermissionVerdict::Allowed:
            return router.StepUp(request, permission, StepUpWhen::Changing);
        case PermissionVerdict::OutOfScope:
            return AdminResponse::Problem(404, "not_found", fmt::format("The supervisor has nothing at {}", request.Path));
        case PermissionVerdict::Forbidden:
            return AdminResponse::Problem(403, "forbidden", fmt::format("This account is not allowed to {}", permission));
    }
    return std::nullopt;
}

std::optional<AdminResponse> Supervisor::StepUpFor(AdminRequest const& request, AdminRouter const& router, std::string_view method, std::string_view tail)
{
    if (method == "GET")
    {
        if (tail == "/api/settings" && AdminConfigView::AsksToReveal(request) && router.MayI(request, "settings.secrets.read") == PermissionVerdict::Allowed)
            return router.StepUp(request, "settings.secrets.read", StepUpWhen::Always);
        return std::nullopt;
    }
    if (!tail.starts_with("/api/settings") || !NamesRestricted(request, method, tail))
        return std::nullopt;
    if (router.MayI(request, AdminSettingsView::RestrictedPermission) != PermissionVerdict::Allowed)
        return std::nullopt;
    return router.StepUp(request, AdminSettingsView::RestrictedPermission, StepUpWhen::Always);
}

AdminResponse Supervisor::Answer(AdminRequest const& request, AdminRouter const& router)
{
    constexpr std::string_view Prefix = "/api/apps/";
    std::string_view const rest = std::string_view(request.Path).substr(Prefix.size());
    std::size_t const slash = rest.find('/');
    std::string_view const name = rest.substr(0, slash);
    std::string_view const tail = slash == std::string_view::npos ? std::string_view() : rest.substr(slash);
    std::string const method = Ambrose::ToUpper(request.Method);
    auto const only = [&request](std::string_view allowed)
    {
        AdminResponse response = AdminResponse::Problem(405, "method_not_allowed", fmt::format("{} answers {}", request.Path, allowed));
        response.Headers.emplace_back("Allow", std::string(allowed));
        return response;
    };

    std::shared_lock<std::shared_mutex> const lock(_mutex);
    ManagedApp* const app = Find(name);
    if (!app)
        return AdminResponse::Problem(404, "unknown_app", fmt::format("The supervisor runs no app named {}", name));
    if (tail.empty() || tail == "/")
    {
        if (method != "GET")
            return only("GET");
        return AdminResponse::Json(200, SnapshotJson(app->Snapshot()).dump());
    }
    if (tail == "/power")
    {
        if (method != "POST")
            return only("POST");
        return PowerRoute(*app, request, router);
    }
    if (tail == "/output/current" || tail == "/output/previous")
    {
        if (std::optional<AdminResponse> refused = Refuse(request, "console.read", router))
            return std::move(*refused);
        if (method != "GET")
            return only("GET");
        OutputRun const run = tail == "/output/current" ? OutputRun::Current : OutputRun::Previous;
        return AdminResponse::Json(200, OutputJson(name, run, app->Output(run, 0)));
    }
    std::optional<std::string_view> const permission = PermissionFor(method, tail);
    if (!permission)
        return AdminResponse::Problem(404, "not_found", fmt::format("The supervisor relays nothing at {} {}", method, request.Path));
    if (std::optional<AdminResponse> refused = Refuse(request, *permission, router))
        return std::move(*refused);
    if (std::optional<AdminResponse> asked = StepUpFor(request, router, method, tail))
        return std::move(*asked);
    if (method == "POST" && tail == "/api/settings/batch")
        if (std::optional<AdminResponse> held = router.Charge(request, AdminSettingsView::BatchCost))
            return std::move(*held);
    std::string const actor = _hooks.NameOf ? _hooks.NameOf(request) : std::string();
    AdminResponse response = Relay(*app, request, tail, ForwardedHeaders(request, router, method, tail, *permission, actor));
    if (_hooks.Relayed && (tail.starts_with("/api/settings") || tail.starts_with("/api/reload")))
        _hooks.Relayed(request, RelayedAnswer{ std::string(name), method, std::string(tail) + QueryString(request), response.Status, response.Body });
    return response;
}

void Supervisor::SetRelayHooks(SupervisorRelayHooks hooks)
{
    _hooks = std::move(hooks);
}

void Supervisor::SetStatusObserver(AppStatusObserver observer)
{
    std::lock_guard<std::mutex> const lock(_observerMutex);
    _statusObserver = std::move(observer);
}

void Supervisor::ForwardStatus(AppSnapshot const& snapshot)
{
    std::lock_guard<std::mutex> const lock(_observerMutex);
    if (_statusObserver)
        _statusObserver(snapshot);
}

std::string Supervisor::StatusData(AppSnapshot const& snapshot)
{
    bool const alive = snapshot.State == AppState::Starting || snapshot.State == AppState::Running || snapshot.State == AppState::Stopping;
    nlohmann::json data;
    data["app"] = snapshot.Name;
    data["state"] = std::string(ManagedApp::StateName(snapshot.State));
    data["since"] = snapshot.StateSinceEpochMs;
    data["pid"] = snapshot.ProcessId ? nlohmann::json(*snapshot.ProcessId) : nlohmann::json(nullptr);
    data["exit_code"] = !alive && !snapshot.Exits.empty() && snapshot.Exits.back().Code ? nlohmann::json(*snapshot.Exits.back().Code) : nlohmann::json(nullptr);
    data["crashes"] = snapshot.Crashes;
    data["next_restart"] = OptionalNumber(snapshot.RestartEpochMs);
    return data.dump();
}

std::vector<std::pair<std::string, std::string>> Supervisor::ForwardedHeaders(AdminRequest const& request, AdminRouter const& router, std::string_view method, std::string_view tail,
    std::string_view permission, std::string const& name)
{
    std::vector<std::pair<std::string, std::string>> headers;
    if (!tail.starts_with("/api/settings"))
        return headers;
    headers.emplace_back("X-Ambrose-Actor", request.Principal);
    headers.emplace_back("X-Ambrose-Actor-Name", name.empty() ? request.Principal : name);
    std::vector<std::string> granted{ std::string(permission) };
    if (method != "GET" && router.MayI(request, AdminSettingsView::RestrictedPermission) == PermissionVerdict::Allowed)
        granted.emplace_back(AdminSettingsView::RestrictedPermission);
    if (method == "GET" && AdminConfigView::AsksToReveal(request) && router.MayI(request, "settings.secrets.read") == PermissionVerdict::Allowed)
        granted.emplace_back("settings.secrets.read");
    headers.emplace_back("X-Ambrose-Grants", fmt::format("{}", fmt::join(granted, ",")));
    return headers;
}

std::string Supervisor::QueryString(AdminRequest const& request)
{
    auto const encode = [](std::string_view text)
    {
        std::string out;
        for (char const c : text)
        {
            unsigned char const byte = static_cast<unsigned char>(c);
            if (std::isalnum(byte) != 0 || c == '-' || c == '_' || c == '.' || c == '~')
                out += c;
            else
                out += fmt::format("%{:02X}", byte);
        }
        return out;
    };
    std::string query;
    for (auto const& [name, value] : request.QueryValues)
        query += fmt::format("{}{}={}", query.empty() ? "?" : "&", encode(name), encode(value));
    return query;
}

AdminResponse Supervisor::PowerRoute(ManagedApp& app, AdminRequest const& request, AdminRouter const& router)
{
    nlohmann::json const body = nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object())
        return AdminResponse::Invalid("A power request takes a JSON object", { { "action", "Name start, stop, restart or kill" } });
    std::vector<std::pair<std::string, std::string>> fields;
    for (auto const& [key, value] : body.items())
        if (key != "action" && key != "seconds")
            fields.emplace_back(key, "A power request takes only action and seconds");
    PowerAction action = PowerAction::Start;
    bool known = false;
    if (auto const named = body.find("action"); named != body.end() && named->is_string())
    {
        if (std::optional<PowerAction> const parsed = ManagedApp::ParseAction(named->get<std::string>()))
        {
            action = *parsed;
            known = true;
        }
    }
    if (!known)
        fields.emplace_back("action", "Name start, stop, restart or kill");
    uint32 seconds = 0;
    auto const countdown = body.find("seconds");
    if (countdown != body.end())
    {
        if (!countdown->is_number_integer() || countdown->get<int64>() < 0 || countdown->get<int64>() > ManagedApp::MaxCountdownSeconds)
            fields.emplace_back("seconds", fmt::format("Give the countdown in whole seconds from 0 to {}", ManagedApp::MaxCountdownSeconds));
        else if (known && action != PowerAction::Stop && action != PowerAction::Restart && countdown->get<int64>() != 0)
            fields.emplace_back("seconds", "Only a stop or a restart counts down");
        else
            seconds = static_cast<uint32>(countdown->get<int64>());
    }
    if (!fields.empty())
        return AdminResponse::Invalid("The power request has problems", std::move(fields));
    if (std::optional<AdminResponse> refused = Refuse(request, std::string("power.") + std::string(ManagedApp::ActionName(action)), router))
        return std::move(*refused);
    PowerResult const result = app.Power(action, seconds);
    if (!result.Accepted)
        return AdminResponse::Problem(result.Status, result.Code, result.Message);
    AMBROSE_LOG(_log, LogLevel::Info, "server.supervisor", "The admin API asked to {} {}{} (request {})", ManagedApp::ActionName(action), app.GetDefinition().Name,
        seconds == 0 ? std::string() : fmt::format(" after {} s", seconds), request.Id);
    nlohmann::json answer;
    answer["app"] = app.GetDefinition().Name;
    answer["action"] = std::string(ManagedApp::ActionName(action));
    answer["seconds"] = seconds;
    answer["accepted"] = true;
    return AdminResponse::Json(202, answer.dump());
}

std::vector<std::pair<std::string, std::string>> Supervisor::CollectErrorReports()
{
    std::vector<std::pair<std::string, std::string>> reports;
    for (std::unique_ptr<ManagedApp> const& app : _apps)
    {
        AppSnapshot const snapshot = app->Snapshot();
        if (!snapshot.ProcessId)
            continue;
        std::optional<AdminClient> const admin = app->GetAdminClient();
        if (!admin)
            continue;
        AdminClientResponse const answer = admin->Send({ "GET", "/api/errors", {}, "application/json", {} }, RelayTimeout);
        if (!answer.Answered || answer.Status != 200)
            continue;
        reports.emplace_back(app->GetDefinition().Name, answer.Body);
    }
    return reports;
}

std::optional<std::string> Supervisor::AskApp(std::string_view name, std::string_view path, std::chrono::milliseconds timeout) const
{
    std::shared_lock<std::shared_mutex> const lock(_mutex);
    ManagedApp* const app = Find(name);
    if (!app)
        return std::nullopt;
    if (!app->Snapshot().ProcessId)
        return std::nullopt;
    std::optional<AdminClient> const admin = app->GetAdminClient();
    if (!admin)
        return std::nullopt;
    AdminClientResponse const answer = admin->Send({ "GET", std::string(path), std::string(), "application/json", std::string() }, timeout);
    if (!answer.Answered || answer.Status != 200)
        return std::nullopt;
    return answer.Body;
}

AdminResponse Supervisor::Relay(ManagedApp& app, AdminRequest const& request, std::string_view path, std::vector<std::pair<std::string, std::string>> headers)
{
    std::string const& name = app.GetDefinition().Name;
    if (path == "/api/session" || path.starts_with("/api/session/"))
        return AdminResponse::Problem(404, "not_relayed", "Signing in belongs to the supervisor, so an app's /api/session is never relayed");
    AppSnapshot const snapshot = app.Snapshot();
    std::optional<AdminClient> const admin = app.GetAdminClient();
    if (!admin)
        return AdminResponse::Problem(503, "app_admin_off", fmt::format("The admin API of {} cannot be reached: {}", name, snapshot.AdminProblem.empty() ? std::string("it is not set up") : snapshot.AdminProblem));
    if (!snapshot.ProcessId)
        return AdminResponse::Problem(503, "app_not_running", fmt::format("{} is {}, so its admin API is not answering", name, ManagedApp::StateName(snapshot.State)));
    AdminClientResponse const answer = admin->Send({ request.Method, std::string(path) + QueryString(request), request.Body, "application/json", request.Id, std::move(headers) }, RelayTimeout);
    if (!answer.Answered)
        return AdminResponse::Problem(502, "app_unreachable", fmt::format("{} did not answer: {}", name, answer.Error));
    AdminResponse response;
    response.Status = answer.Status;
    response.ContentType = answer.ContentType.empty() ? std::string("application/json") : answer.ContentType;
    response.Body = answer.Body;
    return response;
}
