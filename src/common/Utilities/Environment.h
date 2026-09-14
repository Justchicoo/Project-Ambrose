/*
 * Project Ambrose by Imjustchico
 * Portable reads and writes of process environment variables, and the folder holding the running executable.
 */

#ifndef AMBROSE_ENVIRONMENT_H
#define AMBROSE_ENVIRONMENT_H

#include <filesystem>
#include <optional>
#include <string>

namespace Ambrose
{
    std::optional<std::string> GetEnv(std::string const& name);
    bool SetEnv(std::string const& name, std::string const& value);
    bool UnsetEnv(std::string const& name);
    std::filesystem::path GetExecutableDirectory();
}

#endif
