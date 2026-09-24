/*
 * Project Ambrose by Imjustchico
 * Splits Supervisor.Apps at spaces and commas, refuses a name that is not 1 to 32 letters, digits, dashes or underscores and a name given twice, finds a program named without a folder beside the supervisor and adds .exe on Windows, resolves the working folder against the supervisor's and the config file against the app's working folder, and keeps each timeout from 1 second to an hour.
 */

#include "AppDefinition.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <set>

namespace
{
    std::chrono::seconds ReadTimeout(ConfigMgr const& config, std::string const& key, uint32 fallback, std::vector<std::string>& problems)
    {
        uint32 const value = config.GetOption<uint32>(key, fallback, true);
        uint32 const bounded = std::clamp<uint32>(value, 1, AppDefinition::MaxTimeoutSeconds);
        if (bounded != value)
            problems.push_back(fmt::format("{} is {} seconds, outside 1 to {}; using {}", key, value, AppDefinition::MaxTimeoutSeconds, bounded));
        return std::chrono::seconds(bounded);
    }

    std::filesystem::path Resolve(std::filesystem::path const& value, std::filesystem::path const& base)
    {
        return (value.is_absolute() ? value : base / value).lexically_normal();
    }
}

bool AppDefinition::IsValidName(std::string_view name) noexcept
{
    return !name.empty() && name.size() <= MaxNameLength
        && std::all_of(name.begin(), name.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; });
}

std::vector<AppDefinition> AppDefinition::Load(ConfigMgr const& config, std::filesystem::path const& programFolder, std::filesystem::path const& workingFolder, std::vector<std::string>& problems)
{
    std::string list = config.GetOption<std::string>("Supervisor.Apps", "", true);
    std::replace(list.begin(), list.end(), ',', ' ');
    std::vector<AppDefinition> apps;
    std::set<std::string, std::less<>> seen;
    for (std::string_view const name : Ambrose::Tokenize(list, ' ', false))
    {
        if (!IsValidName(name))
        {
            problems.push_back(fmt::format("Supervisor.Apps names '{}', which is not 1 to {} letters, digits, dashes or underscores, so it is left out", name, MaxNameLength));
            continue;
        }
        if (!seen.emplace(name).second)
        {
            problems.push_back(fmt::format("Supervisor.Apps names {} twice; it runs once", name));
            continue;
        }
        AppDefinition app;
        app.Name = std::string(name);
        std::string const prefix = fmt::format("App.{}.", name);

        std::filesystem::path program = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(prefix + "Program", app.Name, true)));
        if (program.empty())
            program = ConfigMgr::PathFromUtf8(app.Name);
        program = program.has_parent_path() ? Resolve(program, workingFolder) : programFolder / program;
#ifdef _WIN32
        if (!program.has_extension())
            program += ".exe";
#endif
        app.Program = program.lexically_normal();
        app.ProgramName = ConfigMgr::PathToUtf8(app.Program.stem());

        std::filesystem::path const working = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(prefix + "WorkingDirectory", "", true)));
        app.WorkingDirectory = working.empty() ? workingFolder.lexically_normal() : Resolve(working, workingFolder);

        std::filesystem::path configFile = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(prefix + "Config", "", true)));
        if (configFile.empty())
            configFile = ConfigMgr::PathFromUtf8(app.Name + ".conf");
        app.Config = Resolve(configFile, app.WorkingDirectory);

        app.Autostart = config.GetOption<bool>(prefix + "Autostart", true, true);
        app.StartTimeout = ReadTimeout(config, prefix + "StartTimeout", DefaultStartTimeoutSeconds, problems);
        app.StopTimeout = ReadTimeout(config, prefix + "StopTimeout", DefaultStopTimeoutSeconds, problems);
        apps.push_back(std::move(app));
    }
    return apps;
}
