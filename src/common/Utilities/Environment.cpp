/*
 * Project Ambrose by Imjustchico
 * Implements environment access with the secure CRT calls on MSVC and POSIX calls elsewhere, and finds the executable through the module path or /proc/self/exe.
 */

#include "Environment.h"

#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

std::filesystem::path Ambrose::GetExecutableDirectory()
{
#ifdef _WIN32
    std::wstring buffer(260, L'\0');
    while (true)
    {
        DWORD const length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return std::filesystem::current_path();
        if (length < buffer.size())
        {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path();
#else
    std::error_code error;
    std::filesystem::path const executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error)
        return std::filesystem::current_path();
    return executable.parent_path();
#endif
}
