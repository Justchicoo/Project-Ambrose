/*
 * Project Ambrose by Imjustchico
 * Implements environment access with the wide secure CRT calls on MSVC, converting names and values between UTF-8 and UTF-16, and POSIX calls elsewhere; reads the arguments from the wide command line on Windows; sets the console code pages to UTF-8 on Windows; tells whether standard input and output are both a console or terminal and, on POSIX, the process is the terminal's foreground job; and finds the executable through the module path or /proc/self/exe.
 */

#include "Environment.h"
#include "Utf.h"

#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

#ifdef _MSC_VER

std::optional<std::string> Ambrose::GetEnv(std::string const& name)
{
    std::optional<std::u16string> const wideName = Utf::Utf8ToUtf16(name, Utf::InvalidPolicy::Reject);
    if (!wideName)
        return std::nullopt;
    wchar_t* buffer = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&buffer, &length, reinterpret_cast<wchar_t const*>(wideName->c_str())) != 0 || buffer == nullptr)
        return std::nullopt;
    std::optional<std::string> value = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(buffer)), Utf::InvalidPolicy::ReplaceWithU_FFFD);
    std::free(buffer);
    return value;
}

bool Ambrose::SetEnv(std::string const& name, std::string const& value)
{
    std::optional<std::u16string> const wideName = Utf::Utf8ToUtf16(name, Utf::InvalidPolicy::Reject);
    std::optional<std::u16string> const wideValue = Utf::Utf8ToUtf16(value, Utf::InvalidPolicy::Reject);
    return wideName && wideValue && _wputenv_s(reinterpret_cast<wchar_t const*>(wideName->c_str()), reinterpret_cast<wchar_t const*>(wideValue->c_str())) == 0;
}

bool Ambrose::UnsetEnv(std::string const& name)
{
    std::optional<std::u16string> const wideName = Utf::Utf8ToUtf16(name, Utf::InvalidPolicy::Reject);
    return wideName && _wputenv_s(reinterpret_cast<wchar_t const*>(wideName->c_str()), L"") == 0;
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

std::vector<std::string> Ambrose::GetArguments(int argc, char** argv)
{
#ifdef _WIN32
    int count = 0;
    if (LPWSTR* const wide = CommandLineToArgvW(GetCommandLineW(), &count))
    {
        std::vector<std::string> arguments;
        arguments.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index)
            arguments.push_back(Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(wide[index])), Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string()));
        LocalFree(wide);
        return arguments;
    }
#endif
    return std::vector<std::string>(argv, argv + argc);
}

void Ambrose::UseUtf8Console()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

bool Ambrose::IsInteractiveTerminal()
{
#ifdef _WIN32
    DWORD mode = 0;
    HANDLE const input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE const output = GetStdHandle(STD_OUTPUT_HANDLE);
    return input != nullptr && input != INVALID_HANDLE_VALUE && output != nullptr && output != INVALID_HANDLE_VALUE
        && GetFileType(input) == FILE_TYPE_CHAR && GetConsoleMode(input, &mode) != 0
        && GetFileType(output) == FILE_TYPE_CHAR && GetConsoleMode(output, &mode) != 0;
#else
    return isatty(STDIN_FILENO) == 1 && isatty(STDOUT_FILENO) == 1 && tcgetpgrp(STDIN_FILENO) == getpgrp();
#endif
}

std::filesystem::path Ambrose::GetExecutablePath()
{
#ifdef _WIN32
    std::wstring buffer(260, L'\0');
    while (true)
    {
        DWORD const length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size())
        {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer);
#else
    std::error_code error;
    std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error)
        return {};
    return executable;
#endif
}

std::filesystem::path Ambrose::GetExecutableDirectory()
{
    std::filesystem::path const executable = GetExecutablePath();
    return executable.empty() ? std::filesystem::current_path() : executable.parent_path();
}
