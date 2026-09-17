/*
 * Project Ambrose by Imjustchico
 * What client discovery may ask of the machine it runs on: environment variables, files and folders, the Windows uninstall entries and the Steam install path, behind an interface so tests can describe a machine; LocalClientSystem answers for the real one.
 */

#ifndef AMBROSE_CLIENTSYSTEM_H
#define AMBROSE_CLIENTSYSTEM_H

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class ClientSystem
{
public:
    struct UninstallEntry
    {
        std::string DisplayName;
        std::string InstallLocation;
    };

    virtual ~ClientSystem() = default;

    virtual bool IsWindows() const = 0;
    virtual std::optional<std::string> GetEnv(std::string const& name) const = 0;
    virtual bool IsFile(std::filesystem::path const& path) const = 0;
    virtual bool IsDirectory(std::filesystem::path const& path) const = 0;
    virtual std::vector<std::filesystem::path> ListDirectories(std::filesystem::path const& path, std::size_t limit) const = 0;
    virtual std::vector<std::filesystem::path> ListFiles(std::filesystem::path const& path, std::size_t limit) const = 0;
    virtual std::optional<std::string> ReadText(std::filesystem::path const& path, std::size_t maxBytes) const = 0;
    virtual std::vector<UninstallEntry> GetUninstallEntries() const = 0;
    virtual std::optional<std::string> GetSteamPath() const = 0;
    virtual std::filesystem::path GetWorkingDirectory() const = 0;
    virtual std::filesystem::path GetExecutableDirectory() const = 0;
};

class LocalClientSystem final : public ClientSystem
{
public:
    bool IsWindows() const override;
    std::optional<std::string> GetEnv(std::string const& name) const override;
    bool IsFile(std::filesystem::path const& path) const override;
    bool IsDirectory(std::filesystem::path const& path) const override;
    std::vector<std::filesystem::path> ListDirectories(std::filesystem::path const& path, std::size_t limit) const override;
    std::vector<std::filesystem::path> ListFiles(std::filesystem::path const& path, std::size_t limit) const override;
    std::optional<std::string> ReadText(std::filesystem::path const& path, std::size_t maxBytes) const override;
    std::vector<UninstallEntry> GetUninstallEntries() const override;
    std::optional<std::string> GetSteamPath() const override;
    std::filesystem::path GetWorkingDirectory() const override;
    std::filesystem::path GetExecutableDirectory() const override;
};

#endif
