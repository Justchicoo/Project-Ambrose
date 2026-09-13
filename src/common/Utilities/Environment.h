/*
 * Project Ambrose by Imjustchico
 * Portable reads and writes of process environment variables.
 */

#ifndef AMBROSE_ENVIRONMENT_H
#define AMBROSE_ENVIRONMENT_H

#include <optional>
#include <string>

namespace Ambrose
{
    std::optional<std::string> GetEnv(std::string const& name);
    bool SetEnv(std::string const& name, std::string const& value);
    bool UnsetEnv(std::string const& name);
}

#endif
