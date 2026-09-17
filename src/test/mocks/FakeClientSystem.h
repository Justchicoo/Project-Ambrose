/*
 * Project Ambrose by Imjustchico
 * An in-memory machine for client discovery tests: files with their contents and the folders above them, environment variables, uninstall entries and a Steam path, with a count of folder queries so tests can check a search stays bounded.
 */

#ifndef AMBROSE_FAKECLIENTSYSTEM_H
#define AMBROSE_FAKECLIENTSYSTEM_H

#include "ClientSystem.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

class FakeClientSystem final : public ClientSystem
{
public:
    bool Windows = true;
    std::map<std::string, std::string> Environment;
    std::vector<UninstallEntry> Uninstall;
    std::optional<std::string> Steam;
    std::filesystem::path Working;
    std::filesystem::path Executable;
    mutable std::size_t DirectoryQueries = 0;

    static std::string Key(std::filesystem::path const& path)
    {
        std::string text = path.generic_string();
        std::replace(text.begin(), text.end(), '\\', '/');
        text = std::filesystem::path(text).lexically_normal().generic_string();
        while (text.size() > 1 && text.back() == '/')
            text.pop_back();
        return text;
    }

    void AddFolder(std::string const& path)
    {
        std::filesystem::path current = Key(path);
        while (!current.empty() && _folders.insert(Key(current)).second && current.has_relative_path())
            current = current.parent_path();
    }

    void AddFile(std::string const& path, std::string content)
    {
        std::filesystem::path const file = Key(path);
        _files[Key(file)] = std::move(content);
        AddFolder(file.parent_path().generic_string());
    }

    void AddInstall(std::string const& root, std::string const& revision)
    {
        AddFile(root + "/Data/GameData/Root.wad", "KIWAD");
        AddFile(root + "/Bin/WizardGraphicalClient.exe", "MZ");
        if (!revision.empty())
            AddFile(root + "/Bin/revision.dat", revision + "\n");
    }

    bool IsWindows() const override { return Windows; }

    std::optional<std::string> GetEnv(std::string const& name) const override
    {
        auto const found = Environment.find(name);
        return found == Environment.end() ? std::nullopt : std::optional<std::string>(found->second);
    }

    bool IsFile(std::filesystem::path const& path) const override { return _files.count(Key(path)) != 0; }

    bool IsDirectory(std::filesystem::path const& path) const override
    {
        ++DirectoryQueries;
        return _folders.count(Key(path)) != 0;
    }

    std::vector<std::filesystem::path> ListDirectories(std::filesystem::path const& path, std::size_t limit) const override
    {
        std::vector<std::filesystem::path> found;
        std::string const parent = Key(path);
        for (std::string const& folder : _folders)
            if (found.size() < limit && folder != parent && Key(std::filesystem::path(folder).parent_path()) == parent)
                found.emplace_back(folder);
        return found;
    }

    std::vector<std::filesystem::path> ListFiles(std::filesystem::path const& path, std::size_t limit) const override
    {
        std::vector<std::filesystem::path> found;
        std::string const parent = Key(path);
        for (auto const& [file, content] : _files)
            if (found.size() < limit && Key(std::filesystem::path(file).parent_path()) == parent)
                found.emplace_back(file);
        return found;
    }

    std::optional<std::string> ReadText(std::filesystem::path const& path, std::size_t maxBytes) const override
    {
        auto const found = _files.find(Key(path));
        return found == _files.end() ? std::nullopt : std::optional<std::string>(found->second.substr(0, maxBytes));
    }

    std::vector<UninstallEntry> GetUninstallEntries() const override { return Uninstall; }
    std::optional<std::string> GetSteamPath() const override { return Steam; }
    std::filesystem::path GetWorkingDirectory() const override { return Working; }
    std::filesystem::path GetExecutableDirectory() const override { return Executable; }

private:
    std::map<std::string, std::string> _files;
    std::set<std::string> _folders;
};

#endif
