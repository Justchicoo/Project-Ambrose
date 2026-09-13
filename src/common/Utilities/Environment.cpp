/*
 * Project Ambrose by Imjustchico
 * Implements environment access with the secure CRT calls on MSVC and POSIX calls elsewhere.
 */

#include "Environment.h"

#include <cstdlib>

#ifdef _MSC_VER

std::optional<std::string> Ambrose::GetEnv(std::string const& name)
{
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name.c_str()) != 0 || buffer == nullptr)
        return std::nullopt;
    std::string value(buffer);
    std::free(buffer);
    return value;
}

bool Ambrose::SetEnv(std::string const& name, std::string const& value)
{
    return _putenv_s(name.c_str(), value.c_str()) == 0;
}

bool Ambrose::UnsetEnv(std::string const& name)
{
    return _putenv_s(name.c_str(), "") == 0;
}

#else

std::optional<std::string> Ambrose::GetEnv(std::string const& name)
{
    char const* value = std::getenv(name.c_str());
    if (value == nullptr)
        return std::nullopt;
    return std::string(value);
}

bool Ambrose::SetEnv(std::string const& name, std::string const& value)
{
    return setenv(name.c_str(), value.c_str(), 1) == 0;
}

bool Ambrose::UnsetEnv(std::string const& name)
{
    return unsetenv(name.c_str()) == 0;
}

#endif
