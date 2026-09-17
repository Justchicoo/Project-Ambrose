/*
 * Project Ambrose by Imjustchico
 * Answers client discovery for the real machine: file system queries that never throw, bounded directory listings and reads, and on Windows the uninstall entries of both registry views under the machine and the user, and Steam's SteamPath value, all read as UTF-8.
 */

#include "ClientSystem.h"
#include "Environment.h"
#include "Utf.h"

#include <fstream>
#include <iterator>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
#ifdef _WIN32
    constexpr wchar_t const* UninstallKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

    std::optional<std::string> ReadRegistryString(HKEY root, std::wstring const& subkey, wchar_t const* value, REGSAM view)
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ | view, &key) != ERROR_SUCCESS)
            return std::nullopt;
        std::optional<std::string> result;
        DWORD size = 0;
        DWORD type = 0;
        if (RegQueryValueExW(key, value, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0 && size < 65536)
        {
            std::wstring buffer(size / sizeof(wchar_t) + 1, L'\0');
            DWORD read = size;
            if (RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &read) == ERROR_SUCCESS)
            {
                buffer.resize(wcsnlen(buffer.c_str(), buffer.size()));
                result = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(buffer.data()), buffer.size()), Utf::InvalidPolicy::ReplaceWithU_FFFD);
            }
        }
        RegCloseKey(key);
        return result;
    }

    void CollectUninstallEntries(HKEY root, REGSAM view, std::vector<ClientSystem::UninstallEntry>& entries)
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(root, UninstallKey, 0, KEY_READ | view, &key) != ERROR_SUCCESS)
            return;
        wchar_t name[256];
        for (DWORD index = 0; index < 4096; ++index)
        {
            DWORD length = static_cast<DWORD>(std::size(name));
            LONG const status = RegEnumKeyExW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr);
            if (status == ERROR_NO_MORE_ITEMS)
                break;
            if (status != ERROR_SUCCESS)
                continue;
            std::wstring const subkey = std::wstring(UninstallKey) + L"\\" + std::wstring(name, length);
            std::optional<std::string> displayName = ReadRegistryString(root, subkey, L"DisplayName", view);
            std::optional<std::string> location = ReadRegistryString(root, subkey, L"InstallLocation", view);
            if (displayName && location && !location->empty())
                entries.push_back({ std::move(*displayName), std::move(*location) });
        }
        RegCloseKey(key);
    }
#endif
}

bool LocalClientSystem::IsWindows() const
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

std::optional<std::string> LocalClientSystem::GetEnv(std::string const& name) const
{
    std::optional<std::string> value = Ambrose::GetEnv(name);
    if (value && value->empty())
        return std::nullopt;
    return value;
}

bool LocalClientSystem::IsFile(std::filesystem::path const& path) const
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

bool LocalClientSystem::IsDirectory(std::filesystem::path const& path) const
{
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

std::vector<std::filesystem::path> LocalClientSystem::ListDirectories(std::filesystem::path const& path, std::size_t limit) const
{
    std::vector<std::filesystem::path> found;
    std::error_code error;
    std::filesystem::directory_iterator entries(path, std::filesystem::directory_options::skip_permission_denied, error);
    for (auto it = entries; !error && it != std::filesystem::directory_iterator() && found.size() < limit; it.increment(error))
    {
        std::error_code typeError;
        if (it->is_directory(typeError))
            found.push_back(it->path());
    }
    return found;
}

std::vector<std::filesystem::path> LocalClientSystem::ListFiles(std::filesystem::path const& path, std::size_t limit) const
{
    std::vector<std::filesystem::path> found;
    std::error_code error;
    std::filesystem::directory_iterator entries(path, std::filesystem::directory_options::skip_permission_denied, error);
    for (auto it = entries; !error && it != std::filesystem::directory_iterator() && found.size() < limit; it.increment(error))
    {
        std::error_code typeError;
        if (it->is_regular_file(typeError))
            found.push_back(it->path());
    }
    return found;
}

std::optional<std::string> LocalClientSystem::ReadText(std::filesystem::path const& path, std::size_t maxBytes) const
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return std::nullopt;
    std::string text(maxBytes, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(maxBytes));
    text.resize(static_cast<std::size_t>(stream.gcount()));
    return text;
}

std::vector<ClientSystem::UninstallEntry> LocalClientSystem::GetUninstallEntries() const
{
    std::vector<UninstallEntry> entries;
#ifdef _WIN32
    for (HKEY const root : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
        for (REGSAM const view : { KEY_WOW64_64KEY, KEY_WOW64_32KEY })
            CollectUninstallEntries(root, view, entries);
#endif
    return entries;
}

std::optional<std::string> LocalClientSystem::GetSteamPath() const
{
#ifdef _WIN32
    if (std::optional<std::string> path = ReadRegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", 0))
        return path;
    for (REGSAM const view : { KEY_WOW64_32KEY, KEY_WOW64_64KEY })
        if (std::optional<std::string> path = ReadRegistryString(HKEY_LOCAL_MACHINE, L"Software\\Valve\\Steam", L"InstallPath", view))
            return path;
#endif
    return std::nullopt;
}

std::filesystem::path LocalClientSystem::GetWorkingDirectory() const
{
    std::error_code error;
    std::filesystem::path const path = std::filesystem::current_path(error);
    return error ? std::filesystem::path() : path;
}

std::filesystem::path LocalClientSystem::GetExecutableDirectory() const
{
    return Ambrose::GetExecutableDirectory();
}
