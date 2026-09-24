/*
 * Project Ambrose by Imjustchico
 * What the launcher must write to build the folder the client runs from, and read out of the install's archives: creating the folder, writing and removing one file, and reading one entry of a KIWAD archive, behind an interface so tests can keep the folder in memory and prove nothing else is touched; LocalLauncherFiles does it on the real machine.
 */

#ifndef AMBROSE_LAUNCHERFILES_H
#define AMBROSE_LAUNCHERFILES_H

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

class LauncherFiles
{
public:
    virtual ~LauncherFiles() = default;

    virtual bool CreateFolder(std::filesystem::path const& folder, std::string& error) const = 0;
    virtual bool WriteFile(std::filesystem::path const& file, std::string_view text, std::string& error) const = 0;
    virtual bool RemoveFile(std::filesystem::path const& file, std::string& error) const = 0;
    virtual std::optional<std::string> ReadArchiveEntry(std::filesystem::path const& archive, std::string_view name, std::size_t maxBytes, std::string& error) const = 0;
};

class LocalLauncherFiles final : public LauncherFiles
{
public:
    bool CreateFolder(std::filesystem::path const& folder, std::string& error) const override;
    bool WriteFile(std::filesystem::path const& file, std::string_view text, std::string& error) const override;
    bool RemoveFile(std::filesystem::path const& file, std::string& error) const override;
    std::optional<std::string> ReadArchiveEntry(std::filesystem::path const& archive, std::string_view name, std::size_t maxBytes, std::string& error) const override;
};

#endif
