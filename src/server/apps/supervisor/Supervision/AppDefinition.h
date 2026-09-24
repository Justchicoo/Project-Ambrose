/*
 * Project Ambrose by Imjustchico
 * What the supervisor runs: each app Supervisor.Apps names, in that order, with its program, config file and working folder resolved to absolute paths, the name the program answers to, whether it starts with the supervisor, and how long a start and a stop may take, read from App.<name>.* with every problem reported by key.
 */

#ifndef AMBROSE_APPDEFINITION_H
#define AMBROSE_APPDEFINITION_H

#include "Types.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

struct AppDefinition
{
    static constexpr std::size_t MaxNameLength = 32;
    static constexpr uint32 DefaultStartTimeoutSeconds = 120;
    static constexpr uint32 DefaultStopTimeoutSeconds = 60;
    static constexpr uint32 MaxTimeoutSeconds = 3600;

    std::string Name;
    std::string ProgramName;
    std::filesystem::path Program;
    std::filesystem::path Config;
    std::filesystem::path WorkingDirectory;
    bool Autostart = true;
    std::chrono::seconds StartTimeout{ DefaultStartTimeoutSeconds };
    std::chrono::seconds StopTimeout{ DefaultStopTimeoutSeconds };

    static bool IsValidName(std::string_view name) noexcept;
    static std::vector<AppDefinition> Load(ConfigMgr const& config, std::filesystem::path const& programFolder, std::filesystem::path const& workingFolder, std::vector<std::string>& problems);
};

#endif
