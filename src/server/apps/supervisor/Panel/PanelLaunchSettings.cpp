/*
 * Project Ambrose by Imjustchico
 * Reads and writes one app's launch settings in the supervisor's store: secret environment values sealed with AES-256-GCM under the supervisor's key through the keyring and unsealed on read, every write validated with a missing build, a working folder outside the install or an out-of-bounds value refused naming the problem, and the launch page answered with secrets masked and the exact command line and environment previewed.
 */

#include "PanelLaunchSettings.h"

#include "Base64.h"
#include "PanelKeyring.h"
#include "PanelStore.h"
#include "Settings.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace
{
    constexpr char const* ClosedStore = "the panel store is not open, so no launch setting can be read or changed";

    std::string RowName(std::string_view prefix, std::string_view name)
    {
        std::string row(prefix);
        row += name;
        return row;
    }

    std::string AssociatedData(std::string_view app, std::string_view name)
    {
        return fmt::format("launch:{}:env:{}", app, name);
    }

    void PutScalar(nlohmann::json const& body, std::string_view key, std::string& out)
    {
        if (body.contains(key) && body[key].is_string())
            out = body[key].get<std::string>();
    }

    void PutOptionalUint(nlohmann::json const& body, std::string_view key, std::optional<uint32>& out, std::string& error)
    {
        if (!body.contains(key) || body[key].is_null())
            return;
        if (!body[key].is_number_unsigned())
        {
            error = fmt::format("{} must be a whole number", key);
            return;
        }
        out = body[key].get<uint32>();
    }

    void PutOptionalUint64(nlohmann::json const& body, std::string_view key, std::optional<uint64>& out, std::string& error)
    {
        if (!body.contains(key) || body[key].is_null())
            return;
        if (!body[key].is_number_unsigned())
        {
            error = fmt::format("{} must be a whole number", key);
            return;
        }
        out = body[key].get<uint64>();
    }
}

PanelLaunchSettings::PanelLaunchSettings(PanelStore& store, PanelKeyring& keyring, std::mutex& mutex, std::filesystem::path installFolder)
    : _store(store), _keyring(keyring), _mutex(mutex), _installFolder(std::move(installFolder))
{
}

bool PanelLaunchSettings::Load(std::string_view app, LaunchSettings& settings, std::string& error)
{
    std::lock_guard const lock(_mutex);
    if (!_store.IsOpen())
    {
        error = ClosedStore;
        return false;
    }
    return LoadLocked(app, settings, error);
}

bool PanelLaunchSettings::LoadLocked(std::string_view app, LaunchSettings& settings, std::string& error) const
{
    settings = LaunchSettings();
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT name, value, secret, key_id FROM launch_settings WHERE app = ?1", error);
    if (!rows)
        return false;
    rows->Bind(1, app);
    std::map<std::string, std::pair<std::string, int64>, std::less<>> secrets;
    while (rows->Step(error))
    {
        std::string const name = rows->Text(0);
        std::string const value = rows->Text(1);
        bool const secret = rows->Int64(2) != 0;
        int64 const keyId = rows->Int64(3);
        if (name.starts_with("override."))
            settings.Overrides[std::string(name.substr(9))] = value;
        else if (name.starts_with("env."))
        {
            std::string const envName = std::string(name.substr(4));
            if (secret)
                secrets[envName] = { value, keyId };
            else
                settings.Environment[envName] = value;
        }
        else if (name == "build")
            settings.Build = value;
        else if (name == "working_directory")
            settings.WorkingDirectory = value;
        else if (name == "start_timeout")
            settings.StartTimeoutSeconds = static_cast<uint32>(std::stoul(value));
        else if (name == "stop_timeout")
            settings.StopTimeoutSeconds = static_cast<uint32>(std::stoul(value));
        else if (name == "restart_policy")
            settings.RestartPolicy = LaunchSettings::ParsePolicy(value);
        else if (name == "backoff_seconds")
            settings.BackoffSeconds = static_cast<uint32>(std::stoul(value));
        else if (name == "max_restarts")
            settings.MaxRestarts = static_cast<uint32>(std::stoul(value));
        else if (name == "crash_window_seconds")
            settings.CrashWindowSeconds = static_cast<uint32>(std::stoul(value));
        else if (name == "autostart")
            settings.Autostart = value == "1";
        else if (name == "priority")
            settings.Priority = std::stoi(value);
        else if (name == "memory_limit_bytes")
            settings.MemoryLimitBytes = std::stoull(value);
        else if (name == "cpu_limit_percent")
            settings.CpuLimitPercent = static_cast<uint32>(std::stoul(value));
    }
    if (!error.empty())
        return false;
    for (auto const& [envName, sealed] : secrets)
    {
        std::string value;
        if (!UnsealSecret(app, envName, sealed.first, sealed.second, value, error))
            return false;
        settings.Environment[envName] = value;
        settings.SecretEnvironment.insert(envName);
    }
    return true;
}

std::string PanelLaunchSettings::SealSecret(std::string_view app, std::string_view name, std::string_view value, int64& keyId, std::string& error) const
{
    std::optional<PanelSealed> const sealed = _keyring.Seal(PanelKeyPurpose::LaunchSecret,
        std::span<uint8 const>(reinterpret_cast<uint8 const*>(value.data()), value.size()), AssociatedData(app, name));
    if (!sealed)
    {
        error = "the keyring is not open, so no launch secret can be sealed";
        return {};
    }
    keyId = sealed->KeyId;
    return Base64::Encode(sealed->Bytes);
}

bool PanelLaunchSettings::UnsealSecret(std::string_view app, std::string_view name, std::string_view sealed, int64 keyId, std::string& value, std::string& error) const
{
    std::optional<std::vector<uint8>> const bytes = Base64::Decode(sealed);
    if (!bytes)
    {
        error = fmt::format("the sealed launch secret '{}' for {} is not valid base64", name, app);
        return false;
    }
    std::optional<std::vector<uint8>> const plain = _keyring.Unseal(PanelKeyPurpose::LaunchSecret, keyId, *bytes, AssociatedData(app, name));
    if (!plain)
    {
        error = fmt::format("the sealed launch secret '{}' for {} could not be opened", name, app);
        return false;
    }
    value.assign(reinterpret_cast<char const*>(plain->data()), plain->size());
    return true;
}

bool PanelLaunchSettings::Write(std::string_view app, LaunchSettings const& settings, SettingAuthor const& author,
    std::string& error, bool transactionAlreadyOpen)
{
    if (std::vector<std::string> const problems = settings.Validate(_installFolder); !problems.empty())
    {
        error = problems.front();
        return false;
    }
    std::lock_guard const lock(_mutex);
    if (!_store.IsOpen())
    {
        error = ClosedStore;
        return false;
    }
    bool const ownTransaction = !transactionAlreadyOpen;
    if (ownTransaction && !_store.Begin(error))
        return false;
    auto const fail = [this, ownTransaction]
    {
        if (ownTransaction)
            _store.Rollback();
        return false;
    };
    if (!WriteLocked(app, settings, author, error))
        return fail();
    if (ownTransaction && !_store.Commit(error))
        return fail();
    return true;
}

std::string PanelLaunchSettings::AuditProperties(std::string_view app, LaunchSettings const& old, LaunchSettings const& settings, std::string_view reason)
{
    nlohmann::json properties;
    properties["app"] = app;
    properties["old"] = FromSettings(old);
    properties["new"] = FromSettings(settings);
    properties["reason"] = reason;
    return properties.dump();
}

void PanelLaunchSettings::KeepMaskedSecrets(LaunchSettings& settings, LaunchSettings const& old)
{
    for (std::string const& name : settings.SecretEnvironment)
    {
        auto const current = settings.Environment.find(name);
        auto const previous = old.Environment.find(name);
        if (current != settings.Environment.end() && previous != old.Environment.end() && old.SecretEnvironment.contains(name)
            && current->second == LaunchSettings::SecretMask)
            current->second = previous->second;
    }
}

bool PanelLaunchSettings::WriteLocked(std::string_view app, LaunchSettings const& settings, SettingAuthor const& author, std::string& error)
{
    std::optional<PanelStore::Statement> clear = _store.Prepare("DELETE FROM launch_settings WHERE app = ?1", error);
    if (!clear)
        return false;
    clear->Bind(1, app);
    if (!clear->Run(error))
        return false;
    int64 const when = PanelStore::NowEpochMs();
    auto const insert = [&](std::string_view name, std::string_view value, bool secret, int64 keyId) -> bool
    {
        std::optional<PanelStore::Statement> row = _store.Prepare(
            "INSERT INTO launch_settings (app, name, value, secret, key_id, updated_by, updated_epoch_ms) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)", error);
        if (!row)
            return false;
        row->Bind(1, app);
        row->Bind(2, name);
        row->Bind(3, value);
        row->Bind(4, secret ? 1 : 0);
        row->Bind(5, keyId);
        row->Bind(6, author.Who);
        row->Bind(7, when);
        return row->Run(error);
    };
    auto const scalar = [&](std::string_view name, std::string_view value) -> bool
    {
        return value.empty() || insert(name, value, false, 0);
    };
    if (!scalar("build", settings.Build))
        return false;
    if (!scalar("working_directory", settings.WorkingDirectory))
        return false;
    if (settings.StartTimeoutSeconds && !insert("start_timeout", std::to_string(*settings.StartTimeoutSeconds), false, 0))
        return false;
    if (settings.StopTimeoutSeconds && !insert("stop_timeout", std::to_string(*settings.StopTimeoutSeconds), false, 0))
        return false;
    if (settings.RestartPolicy && !insert("restart_policy", LaunchSettings::PolicyName(*settings.RestartPolicy), false, 0))
        return false;
    if (settings.BackoffSeconds && !insert("backoff_seconds", std::to_string(*settings.BackoffSeconds), false, 0))
        return false;
    if (settings.MaxRestarts && !insert("max_restarts", std::to_string(*settings.MaxRestarts), false, 0))
        return false;
    if (settings.CrashWindowSeconds && !insert("crash_window_seconds", std::to_string(*settings.CrashWindowSeconds), false, 0))
        return false;
    if (settings.Autostart && !insert("autostart", *settings.Autostart ? "1" : "0", false, 0))
        return false;
    if (settings.Priority && !insert("priority", std::to_string(*settings.Priority), false, 0))
        return false;
    if (settings.MemoryLimitBytes && !insert("memory_limit_bytes", std::to_string(*settings.MemoryLimitBytes), false, 0))
        return false;
    if (settings.CpuLimitPercent && !insert("cpu_limit_percent", std::to_string(*settings.CpuLimitPercent), false, 0))
        return false;
    for (auto const& [key, value] : settings.Overrides)
        if (!insert(RowName("override.", key), value, false, 0))
            return false;
    for (auto const& [name, value] : settings.Environment)
    {
        bool const secret = settings.SecretEnvironment.contains(name);
        int64 keyId = 0;
        std::string stored = value;
        if (secret)
        {
            stored = SealSecret(app, name, value, keyId, error);
            if (!error.empty())
                return false;
        }
        if (!insert(RowName("env.", name), stored, secret, keyId))
            return false;
    }
    return true;
}

nlohmann::json PanelLaunchSettings::Answer(std::string_view app, std::filesystem::path const& program, std::vector<std::string> const& baseArguments, std::string& error) const
{
    LaunchSettings settings;
    {
        std::lock_guard const lock(_mutex);
        if (!_store.IsOpen())
        {
            error = ClosedStore;
            return {};
        }
        if (!LoadLocked(app, settings, error))
            return {};
    }
    nlohmann::json answer = FromSettings(settings);
    answer["preview_command_line"] = settings.PreviewCommandLine(program, baseArguments);
    nlohmann::json environment = nlohmann::json::array();
    for (auto const& [name, value] : settings.PreviewEnvironment())
        environment.push_back({ { "name", name }, { "value", value }, { "secret", settings.SecretEnvironment.contains(name) } });
    answer["preview_environment"] = environment;
    nlohmann::json locked = nlohmann::json::array();
    for (auto const& [key, value] : settings.Overrides)
    {
        static_cast<void>(value);
        locked.push_back({ { "key", key }, { "locked", true }, { "layer", "command-line override" } });
    }
    answer["locked_keys"] = locked;
    return answer;
}

void PanelLaunchSettings::ToSettings(nlohmann::json const& body, LaunchSettings& settings, std::string& error)
{
    settings = LaunchSettings();
    PutScalar(body, "build", settings.Build);
    PutScalar(body, "working_directory", settings.WorkingDirectory);
    PutOptionalUint(body, "start_timeout", settings.StartTimeoutSeconds, error);
    PutOptionalUint(body, "stop_timeout", settings.StopTimeoutSeconds, error);
    if (body.contains("restart_policy") && body["restart_policy"].is_string())
    {
        std::optional<LaunchRestartPolicy> const policy = LaunchSettings::ParsePolicy(body["restart_policy"].get<std::string>());
        if (!policy)
            error = "restart_policy must be always, on-crash or never";
        else
            settings.RestartPolicy = policy;
    }
    PutOptionalUint(body, "backoff_seconds", settings.BackoffSeconds, error);
    PutOptionalUint(body, "max_restarts", settings.MaxRestarts, error);
    PutOptionalUint(body, "crash_window_seconds", settings.CrashWindowSeconds, error);
    if (body.contains("autostart") && body["autostart"].is_boolean())
        settings.Autostart = body["autostart"].get<bool>();
    if (body.contains("priority") && body["priority"].is_number_integer())
        settings.Priority = body["priority"].get<int>();
    PutOptionalUint64(body, "memory_limit_bytes", settings.MemoryLimitBytes, error);
    PutOptionalUint(body, "cpu_limit_percent", settings.CpuLimitPercent, error);
    if (!error.empty())
        return;
    if (body.contains("overrides") && body["overrides"].is_object())
        for (auto const& [key, value] : body["overrides"].items())
        {
            if (!value.is_string())
            {
                error = fmt::format("the command-line override '{}' must be a string", key);
                return;
            }
            settings.Overrides[key] = value.get<std::string>();
        }
    if (body.contains("environment") && body["environment"].is_object())
        for (auto const& [name, value] : body["environment"].items())
        {
            if (!value.is_string())
            {
                error = fmt::format("the environment variable '{}' must be a string", name);
                return;
            }
            settings.Environment[name] = value.get<std::string>();
        }
    if (body.contains("secret_environment") && body["secret_environment"].is_array())
        for (auto const& name : body["secret_environment"])
        {
            if (!name.is_string())
            {
                error = "secret_environment must list variable names";
                return;
            }
            settings.SecretEnvironment.insert(name.get<std::string>());
        }
}

nlohmann::json PanelLaunchSettings::FromSettings(LaunchSettings const& settings)
{
    nlohmann::json body;
    body["build"] = settings.Build;
    body["working_directory"] = settings.WorkingDirectory;
    body["start_timeout"] = settings.StartTimeoutSeconds ? nlohmann::json(*settings.StartTimeoutSeconds) : nlohmann::json();
    body["stop_timeout"] = settings.StopTimeoutSeconds ? nlohmann::json(*settings.StopTimeoutSeconds) : nlohmann::json();
    body["restart_policy"] = settings.RestartPolicy ? nlohmann::json(std::string(LaunchSettings::PolicyName(*settings.RestartPolicy))) : nlohmann::json();
    body["backoff_seconds"] = settings.BackoffSeconds ? nlohmann::json(*settings.BackoffSeconds) : nlohmann::json();
    body["max_restarts"] = settings.MaxRestarts ? nlohmann::json(*settings.MaxRestarts) : nlohmann::json();
    body["crash_window_seconds"] = settings.CrashWindowSeconds ? nlohmann::json(*settings.CrashWindowSeconds) : nlohmann::json();
    body["autostart"] = settings.Autostart ? nlohmann::json(*settings.Autostart) : nlohmann::json();
    body["priority"] = settings.Priority ? nlohmann::json(*settings.Priority) : nlohmann::json();
    body["memory_limit_bytes"] = settings.MemoryLimitBytes ? nlohmann::json(*settings.MemoryLimitBytes) : nlohmann::json();
    body["cpu_limit_percent"] = settings.CpuLimitPercent ? nlohmann::json(*settings.CpuLimitPercent) : nlohmann::json();
    nlohmann::json overrides = nlohmann::json::object();
    for (auto const& [key, value] : settings.Overrides)
        overrides[key] = value;
    body["overrides"] = overrides;
    nlohmann::json environment = nlohmann::json::object();
    for (auto const& [name, value] : settings.Environment)
        environment[name] = settings.SecretEnvironment.contains(name) ? std::string(LaunchSettings::SecretMask) : value;
    body["environment"] = environment;
    nlohmann::json secrets = nlohmann::json::array();
    for (std::string const& name : settings.SecretEnvironment)
        secrets.push_back(name);
    body["secret_environment"] = secrets;
    return body;
}
