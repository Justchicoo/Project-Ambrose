/*
 * Project Ambrose by Imjustchico
 * Builds the status, apps and capabilities bodies from a snapshot and the capability registries, writing every stat value in its own JSON type, and registers the three GET routes on a router.
 */

#include "AdminStatus.h"
#include "AdminRouter.h"
#include "Log.h"
#include "LogRedaction.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

namespace
{
    nlohmann::json ToJson(Ambrose::StatValue const& value)
    {
        return std::visit([](auto const& held) { return nlohmann::json(held); }, value);
    }
}

std::vector<std::string> const& AdminStatus::StatusFields()
{
    static std::vector<std::string> const fields{ "schema", "app", "role", "realm", "revision", "state", "uptime", "memory", "threads", "sessions", "tick", "stats", "problems" };
    return fields;
}

std::vector<std::string> const& AdminStatus::ErrorFields()
{
    static std::vector<std::string> const fields{ "app", "revision", "level", "category", "file", "line", "function", "template", "count", "first_epoch_ms", "last_epoch_ms", "last_message" };
    return fields;
}

std::vector<std::string> const& AdminStatus::AppFields()
{
    static std::vector<std::string> const fields{ "name", "role", "realm", "address", "port", "revision" };
    return fields;
}

std::vector<std::string> const& AdminStatus::CapabilityFields()
{
    static std::vector<std::string> const fields{ "schema", "reload_targets", "schedule_actions", "announcement_channels", "problem_codes" };
    return fields;
}

std::string AdminStatus::StatusJson(AdminStatusSnapshot const& snapshot)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["app"] = snapshot.App.Name;
    body["role"] = snapshot.App.Role;
    body["realm"] = snapshot.App.Realm;
    body["revision"] = snapshot.App.Revision;
    body["state"] = snapshot.State;
    body["uptime"] = snapshot.UptimeSeconds;
    if (snapshot.Process)
    {
        body["memory"] = { { "resident_bytes", snapshot.Process->ResidentBytes } };
        body["threads"] = snapshot.Process->ThreadCount;
    }
    else
    {
        body["memory"] = nullptr;
        body["threads"] = nullptr;
    }
    if (snapshot.Sessions)
        body["sessions"] = *snapshot.Sessions;
    else
        body["sessions"] = nullptr;
    if (snapshot.Tick)
        body["tick"] = { { "average_ms", snapshot.Tick->AverageMs }, { "max_ms", snapshot.Tick->MaxMs }, { "samples", snapshot.Tick->Samples }, { "window_seconds", snapshot.Tick->WindowSeconds } };
    else
        body["tick"] = nullptr;
    nlohmann::json stats = nlohmann::json::object();
    for (auto const& [name, value] : snapshot.Stats)
        stats[name] = ToJson(value);
    body["stats"] = std::move(stats);
    nlohmann::json problems = nlohmann::json::array();
    for (AdminProblem const& problem : snapshot.Problems)
        problems.push_back({ { "code", problem.Code }, { "message", problem.Message }, { "subject", problem.Subject } });
    body["problems"] = std::move(problems);
    return body.dump();
}

std::string AdminStatus::AppsJson(AdminStatusSnapshot const& snapshot)
{
    nlohmann::json app;
    app["name"] = snapshot.App.Name;
    app["role"] = snapshot.App.Role;
    app["realm"] = snapshot.App.Realm;
    app["address"] = snapshot.App.Address;
    app["port"] = snapshot.App.Port;
    app["revision"] = snapshot.App.Revision;
    nlohmann::json body = nlohmann::json::array();
    body.push_back(std::move(app));
    return body.dump();
}

std::string AdminStatus::ErrorsJson(std::string const& appName)
{
    nlohmann::json groups = nlohmann::json::array();
    for (LogErrorGroup const& group : sLog.GetErrors().Groups())
    {
        nlohmann::json entry;
        entry["app"] = appName;
        entry["revision"] = group.Revision;
        entry["level"] = Ambrose::ToLower(std::string(Ambrose::Logging::GetLogLevelName(group.Level)));
        entry["category"] = group.Category;
        entry["file"] = group.File;
        entry["line"] = group.Line;
        entry["function"] = group.Function;
        entry["template"] = group.Template;
        entry["count"] = group.Count;
        entry["first_epoch_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(group.FirstSeen.time_since_epoch()).count();
        entry["last_epoch_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(group.LastSeen.time_since_epoch()).count();
        entry["last_message"] = LogRedaction::Redact(group.LastMessage);
        groups.push_back(std::move(entry));
    }
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["groups"] = std::move(groups);
    body["dropped"] = sLog.GetErrors().GetDropped();
    return body.dump();
}

std::string AdminStatus::CapabilitiesJson()
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["reload_targets"] = sAdminCapabilities.ReloadTargets();
    body["schedule_actions"] = sAdminCapabilities.ScheduleActions();
    body["announcement_channels"] = sAdminCapabilities.AnnouncementChannels();
    nlohmann::json codes = nlohmann::json::array();
    for (auto const& [code, description] : sAdminCapabilities.ProblemCodes())
        codes.push_back({ { "code", code }, { "description", description } });
    body["problem_codes"] = std::move(codes);
    return body.dump();
}

void AdminStatus::Register(AdminRouter& router, Source source)
{
    router.AddGuarded("GET", "/api/status", "status.read", [source](AdminRequest const&) { return AdminResponse::Json(200, StatusJson(source())); });
    router.AddGuarded("GET", "/api/apps", "status.read", [source](AdminRequest const&) { return AdminResponse::Json(200, AppsJson(source())); });
    router.AddOpen("GET", "/api/capabilities", [](AdminRequest const&) { return AdminResponse::Json(200, CapabilitiesJson()); });
    router.AddGuarded("GET", "/api/errors", "status.read", [source](AdminRequest const&) { return AdminResponse::Json(200, ErrorsJson(source().App.Name)); });
}
