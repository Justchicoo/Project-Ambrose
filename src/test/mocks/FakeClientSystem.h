/*
 * Project Ambrose by Imjustchico
 * An in-memory machine for client discovery tests: files with their contents and the folders above them, symbolic links to folders that every query follows, environment variables whose empty values count as unset, uninstall entries and a Steam path, with paths compared case-insensitively when it plays Windows and a count of folder queries so tests can check a search stays bounded.
 */

#ifndef AMBROSE_FAKECLIENTSYSTEM_H
#define AMBROSE_FAKECLIENTSYSTEM_H

#include "ClientSystem.h"
#include "StringUtil.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class FakeClientSystem final : public ClientSystem
{
public:
    static constexpr int MaxLinkHops = 16;

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

    void AddInstall(std::string const& root, std::string const& revision, bool program = true)
    {
        AddFile(root + "/Data/GameData/Root.wad", "KIWAD");
        if (program)
            AddFile(root + "/Bin/WizardGraphicalClient.exe", "MZ");
        if (!revision.empty())
            AddFile(root + "/Bin/revision.dat", revision + "\n");
    }

    void AddLink(std::string const& link, std::string const& target)
    {
        std::string const from = Key(link);
        _links[from] = Key(target);
        AddFolder(std::filesystem::path(from).parent_path().generic_string());
    }

    bool IsWindows() const override { return Windows; }

    std::optional<std::string> GetEnv(std::string const& name) const override
    {
        auto const found = Environment.find(name);
        if (found == Environment.end() || found->second.empty())
            return std::nullopt;
        return found->second;
    }

    bool IsFile(std::filesystem::path const& path) const override { return FindFile(Resolve(path)) != _files.end(); }

    bool IsDirectory(std::filesystem::path const& path) const override
    {
        ++DirectoryQueries;
        return HasFolder(Resolve(path));
    }

    std::vector<std::filesystem::path> ListDirectories(std::filesystem::path const& path, std::size_t limit) const override
    {
        std::vector<std::filesystem::path> found;
        std::string const parent = Resolve(path);
        std::filesystem::path const spelled = Key(path);
        for (std::string const& folder : _folders)
            if (found.size() < limit && !Same(folder, parent) && Same(Key(std::filesystem::path(folder).parent_path()), parent))
                found.push_back(spelled / std::filesystem::path(folder).filename());
        for (auto const& [link, target] : _links)
            if (found.size() < limit && Same(Key(std::filesystem::path(link).parent_path()), parent) && HasFolder(Resolve(link)))
                found.push_back(spelled / std::filesystem::path(link).filename());
        return found;
    }

    std::vector<std::filesystem::path> ListFiles(std::filesystem::path const& path, std::size_t limit) const override
    {
        std::vector<std::filesystem::path> found;
        std::string const parent = Resolve(path);
        std::filesystem::path const spelled = Key(path);
        for (auto const& [file, content] : _files)
            if (found.size() < limit && Same(Key(std::filesystem::path(file).parent_path()), parent))
                found.push_back(spelled / std::filesystem::path(file).filename());
        return found;
    }

    std::optional<std::string> ReadText(std::filesystem::path const& path, std::size_t maxBytes) const override
    {
        auto const found = FindFile(Resolve(path));
        return found == _files.end() ? std::nullopt : std::optional<std::string>(found->second.substr(0, maxBytes));
    }

    std::filesystem::path Canonical(std::filesystem::path const& path) const override
    {
        return path.empty() ? path : std::filesystem::path(Resolve(path));
    }

    std::vector<UninstallEntry> GetUninstallEntries() const override { return Uninstall; }
    std::optional<std::string> GetSteamPath() const override { return Steam; }
    std::filesystem::path GetWorkingDirectory() const override { return Working; }
    std::filesystem::path GetExecutableDirectory() const override { return Executable; }

private:
    bool Same(std::string const& left, std::string const& right) const
    {
        return Windows ? Ambrose::EqualsIgnoreCase(left, right) : left == right;
    }

    std::string Resolve(std::filesystem::path const& path) const
    {
        std::string text = Key(path);
        for (int hop = 0; hop < MaxLinkHops; ++hop)
        {
            auto const link = std::find_if(_links.begin(), _links.end(), [&](auto const& entry)
            {
                std::string const& from = entry.first;
                return text.size() >= from.size() && Same(text.substr(0, from.size()), from) && (text.size() == from.size() || text[from.size()] == '/');
            });
            if (link == _links.end())
                break;
            text = Key(link->second + text.substr(link->first.size()));
        }
        return text;
    }

    std::map<std::string, std::string>::const_iterator FindFile(std::string const& key) const
    {
        auto const exact = _files.find(key);
        if (exact != _files.end() || !Windows)
            return exact;
        return std::find_if(_files.begin(), _files.end(), [&](auto const& entry) { return Ambrose::EqualsIgnoreCase(entry.first, key); });
    }

    bool HasFolder(std::string const& key) const
    {
        if (_folders.count(key) != 0)
            return true;
        return Windows && std::any_of(_folders.begin(), _folders.end(), [&](std::string const& folder) { return Ambrose::EqualsIgnoreCase(folder, key); });
    }

    std::map<std::string, std::string> _files;
    std::set<std::string> _folders;
    std::map<std::string, std::string> _links;
};

#endif
