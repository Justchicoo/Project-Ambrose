/*
 * Project Ambrose by Imjustchico
 * Reads and writes the saved state as JSON with a schema number, refusing a file that is not that JSON so a damaged file is named rather than taken as empty, treating a missing file as a first start, and writing through the admin token's owner-only file writer before renaming the result into place.
 */

#include "SupervisorState.h"
#include "AdminToken.h"
#include "ConfigMgr.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <system_error>

SupervisorState::SupervisorState(std::filesystem::path file) : _file(std::move(file))
{
}

std::string SupervisorState::ToJson(std::map<std::string, SavedApp, std::less<>> const& apps)
{
    nlohmann::json list = nlohmann::json::object();
    for (auto const& [name, app] : apps)
    {
        nlohmann::json entry;
        entry["want_running"] = app.WantRunning;
        entry["started_epoch_ms"] = app.StartedEpochMs;
        if (app.Process)
        {
            entry["process"] = {
                { "id", app.Process->Id },
                { "start_time", app.Process->StartTime },
                { "boot_id", app.Process->BootId },
                { "executable", ConfigMgr::PathToUtf8(app.Process->Executable) }
            };
        }
        else
            entry["process"] = nullptr;
        list[name] = std::move(entry);
    }
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["apps"] = std::move(list);
    return body.dump(2);
}

std::optional<std::map<std::string, SavedApp, std::less<>>> SupervisorState::FromJson(std::string_view text, std::string& error)
{
    nlohmann::json const body = nlohmann::json::parse(text, nullptr, false);
    if (!body.is_object() || !body.contains("apps") || !body["apps"].is_object())
    {
        error = "it is not the supervisor's state";
        return std::nullopt;
    }
    if (!body.contains("schema") || !body["schema"].is_number_integer() || body["schema"].get<int>() != SchemaVersion)
    {
        error = fmt::format("it has a schema other than {}", SchemaVersion);
        return std::nullopt;
    }
    std::map<std::string, SavedApp, std::less<>> apps;
    for (auto const& [name, entry] : body["apps"].items())
    {
        if (!entry.is_object())
        {
            error = fmt::format("the entry for {} is not an object", name);
            return std::nullopt;
        }
        SavedApp app;
        app.WantRunning = entry.value("want_running", false);
        app.StartedEpochMs = entry.value("started_epoch_ms", int64{ 0 });
        auto const process = entry.find("process");
        if (process != entry.end() && process->is_object())
        {
            ChildProcessIdentity identity;
            identity.Id = process->value("id", int64{ 0 });
            identity.StartTime = process->value("start_time", uint64{ 0 });
            identity.BootId = process->value("boot_id", std::string());
            identity.Executable = ConfigMgr::PathFromUtf8(process->value("executable", std::string()));
            if (identity.Id > 0)
                app.Process = std::move(identity);
        }
        apps.emplace(name, std::move(app));
    }
    return apps;
}

bool SupervisorState::Load(std::string& error)
{
    std::lock_guard<std::mutex> const lock(_mutex);
    _apps.clear();
    std::error_code code;
    if (!std::filesystem::exists(_file, code))
        return true;
    std::ifstream stream(_file, std::ios::binary);
    if (!stream)
    {
        error = fmt::format("the supervisor's state {} cannot be read", ConfigMgr::PathToUtf8(_file));
        return false;
    }
    std::ostringstream text;
    text << stream.rdbuf();
    std::string problem;
    std::optional<std::map<std::string, SavedApp, std::less<>>> apps = FromJson(text.str(), problem);
    if (!apps)
    {
        error = fmt::format("the supervisor's state {} cannot be used because {}; move it aside to start from nothing", ConfigMgr::PathToUtf8(_file), problem);
        return false;
    }
    _apps = std::move(*apps);
    return true;
}

std::optional<SavedApp> SupervisorState::Get(std::string const& name) const
{
    std::lock_guard<std::mutex> const lock(_mutex);
    auto const found = _apps.find(name);
    if (found == _apps.end())
        return std::nullopt;
    return found->second;
}

bool SupervisorState::Put(std::string const& name, SavedApp const& app, std::string& error)
{
    std::lock_guard<std::mutex> const lock(_mutex);
    _apps[name] = app;
    return Write(error);
}

bool SupervisorState::Write(std::string& error) const
{
    std::filesystem::path temporary = _file;
    temporary += ".tmp";
    if (!AdminToken::WriteSecretFile(temporary, ToJson(_apps), error))
        return false;
    std::error_code code;
    std::filesystem::rename(temporary, _file, code);
    if (code)
    {
        error = fmt::format("{} could not replace {}: {}", ConfigMgr::PathToUtf8(temporary), ConfigMgr::PathToUtf8(_file), code.message());
        std::filesystem::remove(temporary, code);
        return false;
    }
    return true;
}
