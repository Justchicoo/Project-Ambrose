/*
 * Project Ambrose by Imjustchico
 * Installs and removes the supervisor's native host service registration on Windows and systemd.
 */

#ifndef AMBROSE_SERVICEINSTALLER_H
#define AMBROSE_SERVICEINSTALLER_H

#include <string>
#include <functional>
#include <vector>

namespace SupervisorService
{
    int Install(std::vector<std::string> const& arguments);
    int Uninstall();
    int Run(std::vector<std::string> const& arguments, std::function<int(std::vector<std::string> const&)> runner, std::function<void()> stop);
}

#endif
