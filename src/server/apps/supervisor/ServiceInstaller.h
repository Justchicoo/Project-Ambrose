/*
 * Project Ambrose by Imjustchico
 * Installs and removes the supervisor's native host service registration on Windows and systemd.
 */

#ifndef AMBROSE_SERVICEINSTALLER_H
#define AMBROSE_SERVICEINSTALLER_H

#include <string>
#include <string_view>
#include <functional>
#include <vector>

namespace SupervisorService
{
    std::vector<std::string> ArgumentsWithoutServiceFlag(std::vector<std::string> const& arguments);
    std::string BuildSystemdUnitText(std::string_view executable, std::string_view config);
    bool SetConfigValue(std::string& contents, std::string_view key, std::string_view value);
    int Install(std::vector<std::string> const& arguments);
    int Uninstall();
    int Run(std::vector<std::string> const& arguments, std::function<int(std::vector<std::string> const&)> runner, std::function<void()> stop);
}

#endif
