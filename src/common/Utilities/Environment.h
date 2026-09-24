/*
 * Project Ambrose by Imjustchico
 * Portable reads and writes of process environment variables and the command line arguments, both as UTF-8 on every platform, switching the console to UTF-8, whether a person can answer on a terminal, and the running executable and the folder holding it.
 */

#ifndef AMBROSE_ENVIRONMENT_H
#define AMBROSE_ENVIRONMENT_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Ambrose
{
    std::optional<std::string> GetEnv(std::string const& name);
    bool SetEnv(std::string const& name, std::string const& value);
    bool UnsetEnv(std::string const& name);
    std::vector<std::string> GetArguments(int argc, char** argv);
    void UseUtf8Console();
    bool IsInteractiveTerminal();
    std::filesystem::path GetExecutablePath();
    std::filesystem::path GetExecutableDirectory();
}

#endif
