/*
 * Project Ambrose by Imjustchico
 * Keeps the apps behind a shared lock that only starting and stopping the supervisor takes alone, so a request always finds a whole list; the saved state and each app's output live under the data folder unless Supervisor.StateFile or Supervisor.OutputDir says otherwise; a power request names its action and, for a stop or restart, a countdown, and anything else in it is refused field by field; a relayed request keeps its method, path, body and request id, sessions stay the supervisor's own and are never relayed, and an app that is not running or has its admin API off is answered 503 with the reason.
 */

#include "Supervisor.h"
#include "AdminRouter.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
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
    settings.OutputFolder = ConfiguredPath(config, "Supervisor.OutputDir", home / "output", settings.WorkingFolder);
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
        apps.push_back(std::make_unique<ManagedApp>(std::move(definition), *state, settings.OutputFolder, settings.MaxOutputBytes, settings.DataFolder, _sendBreak, _log));
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
    _routes = &router;
    router.AddGuarded("GET", "/api/apps", "status.read", [this, self](AdminRequest const&) { return AdminResponse::Json(200, AppsJson(self(), Snapshots())); });
    router.AddGuarded("GET", "/api/supervisor", "status.read", [this](AdminRequest const&) { return AdminResponse::Json(200, SupervisionJson(Snapshots())); });
    for (char const* method : { "GET", "POST", "PUT", "PATCH", "DELETE" })
        router.AddGuardedPrefix(method, "/api/apps/", "status.read", [this](AdminRequest const& request) { return Answer(request); });
}

std::string_view Supervisor::PermissionFor(std::string_view tail) noexcept
{
    if (tail == "/api/command")
        return "console.write";
    if (tail.starts_with("/api/logs"))
        return "console.read";
    if (tail.starts_with("/api/settings"))
        return "settings.read";
    if (tail.starts_with("/api/database"))
        return "database.read";
    return "status.read";
}

std::optional<AdminResponse> Supervisor::Refuse(AdminRequest const& request, std::string_view permission) const
{
    if (_routes == nullptr)
        return std::nullopt;
    switch (_routes->MayI(request, permission))
    {
        case PermissionVerdict::Allowed:
            return std::nullopt;
        case PermissionVerdict::OutOfScope:
            return AdminResponse::Problem(404, "not_found", fmt::format("The supervisor has nothing at {}", request.Path));
        case PermissionVerdict::Forbidden:
            return AdminResponse::Problem(403, "forbidden", fmt::format("This account is not allowed to {}", permission));
    }
    return std::nullopt;
}

AdminResponse Supervisor::Answer(AdminRequest const& request)
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
        return PowerRoute(*app, request);
    }
    if (std::optional<AdminResponse> refused = Refuse(request, tail == "/output/current" || tail == "/output/previous" ? "console.read" : PermissionFor(tail)))
        return std::move(*refused);
    if (tail == "/output/current" || tail == "/output/previous")
    {
        if (method != "GET")
            return only("GET");
        OutputRun const run = tail == "/output/current" ? OutputRun::Current : OutputRun::Previous;
        return AdminResponse::Json(200, OutputJson(name, run, app->Output(run, 0)));
    }
    if (tail.starts_with("/api/"))
        return Relay(*app, request, tail);
    return AdminResponse::Problem(404, "not_found", fmt::format("The supervisor has nothing at {}", request.Path));
}

AdminResponse Supervisor::PowerRoute(ManagedApp& app, AdminRequest const& request)
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
    if (std::optional<AdminResponse> refused = Refuse(request, std::string("power.") + std::string(ManagedApp::ActionName(action))))
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

AdminResponse Supervisor::Relay(ManagedApp& app, AdminRequest const& request, std::string_view path)
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
    AdminClientResponse const answer = admin->Send({ request.Method, std::string(path), request.Body, "application/json", request.Id }, RelayTimeout);
    if (!answer.Answered)
        return AdminResponse::Problem(502, "app_unreachable", fmt::format("{} did not answer: {}", name, answer.Error));
    AdminResponse response;
    response.Status = answer.Status;
    response.ContentType = answer.ContentType.empty() ? std::string("application/json") : answer.ContentType;
    response.Body = answer.Body;
    return response;
}
