/*
 * Project Ambrose by Imjustchico
 * An in-memory run folder for launcher tests: it records every folder created, file written and file removed so a test can prove what the launcher writes and that it writes nothing else, hands out archive entries a test put there, and fails a create, write or remove when a test asks it to.
 */

#ifndef AMBROSE_FAKELAUNCHERFILES_H
#define AMBROSE_FAKELAUNCHERFILES_H

#include "LauncherFiles.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

class FakeLauncherFiles final : public LauncherFiles
{
public:
    mutable std::vector<std::string> Folders;
    mutable std::map<std::string, std::string> Written;
    mutable std::vector<std::string> Order;
    mutable std::vector<std::string> Removed;
    std::map<std::string, std::string> Entries;
    std::string FolderError;
    std::string WriteError;
    std::string RemoveError;

    static std::string Key(std::filesystem::path const& path)
    {
        std::string text = path.generic_string();
        std::replace(text.begin(), text.end(), '\\', '/');
        return text;
    }

    void AddEntry(std::string const& archive, std::string const& name, std::string text)
    {
        Entries[archive + "|" + name] = std::move(text);
    }

    bool Has(std::string const& file) const { return Written.count(file) != 0; }

    std::string Text(std::string const& file) const
    {
        auto const found = Written.find(file);
        return found == Written.end() ? std::string() : found->second;
    }

    bool CreateFolder(std::filesystem::path const& folder, std::string& error) const override
    {
        if (!FolderError.empty())
        {
            error = FolderError;
            return false;
        }
        Folders.push_back(Key(folder));
        return true;
    }

    bool WriteFile(std::filesystem::path const& file, std::string_view text, std::string& error) const override
    {
        if (!WriteError.empty())
        {
            error = WriteError;
            return false;
        }
        Written[Key(file)] = std::string(text);
        Order.push_back(Key(file));
        return true;
    }

    bool RemoveFile(std::filesystem::path const& file, std::string& error) const override
    {
        if (!RemoveError.empty())
        {
            error = RemoveError;
            return false;
        }
        Removed.push_back(Key(file));
        Written.erase(Key(file));
        return true;
    }

    std::optional<std::string> ReadArchiveEntry(std::filesystem::path const& archive, std::string_view name, std::size_t maxBytes, std::string& error) const override
    {
        auto const found = Entries.find(Key(archive) + "|" + std::string(name));
        if (found == Entries.end())
        {
            error = "no such entry";
            return std::nullopt;
        }
        return found->second.substr(0, maxBytes);
    }
};

#endif
