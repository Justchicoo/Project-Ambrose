/*
 * Project Ambrose by Imjustchico
 * Lists a folder's entries by a case-blind match of the path's start, opens each archive once, shares the entries among the threads through one counter while each archive serializes only its own reads, decodes each entry's BINd object and hands it to the reader, and turns what failed into one line for each way of failing and a total.
 */

#include "TemplateFolder.h"
#include "BindFile.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "PropertyObject.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <thread>

namespace
{
    struct FailureGroup
    {
        std::string First;
        std::vector<std::string> Files;
    };

    std::string WithoutPath(std::string const& error, std::string const& path)
    {
        std::string key = error;
        for (std::size_t at = key.find(path); at != std::string::npos; at = key.find(path, at))
            key.erase(at, path.size());
        return key;
    }
}

std::vector<TemplateFolderEntry> TemplateFolder::List(TemplateManifest const& manifest, std::string_view folder)
{
    std::vector<TemplateFolderEntry> entries;
    for (auto const& [id, location] : manifest.GetLocations())
        if (location.Path.size() > folder.size() && Ambrose::EqualsIgnoreCase(std::string_view(location.Path).substr(0, folder.size()), folder))
            entries.push_back({ id, location });
    std::sort(entries.begin(), entries.end(), [](TemplateFolderEntry const& left, TemplateFolderEntry const& right) { return left.Id < right.Id; });
    return entries;
}

bool TemplateFolder::ReadAll(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, std::vector<TemplateFolderEntry> const& entries, std::string_view what,
    std::string_view folder, Reader const& reader, std::vector<std::string>& errors, std::size_t& threads)
{
    std::map<std::string, std::unique_ptr<KiwadArchive>, std::less<>> archives;
    for (TemplateFolderEntry const& entry : entries)
    {
        if (archives.contains(entry.Location.Archive))
            continue;
        std::string error;
        std::filesystem::path const path = gameData / ConfigMgr::PathFromUtf8(entry.Location.Archive);
        std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(path, error);
        if (!archive)
        {
            errors.push_back(fmt::format("{} cannot be opened: {}", ConfigMgr::PathToUtf8(path), error));
            return false;
        }
        archives.emplace(entry.Location.Archive, std::move(archive));
    }

    std::vector<std::string> failures(entries.size());
    std::vector<char> read(entries.size(), 0);
    std::atomic<std::size_t> next{ 0 };
    auto const work = [&]()
    {
        for (std::size_t index = next.fetch_add(1); index < entries.size(); index = next.fetch_add(1))
        {
            TemplateLocation const& location = entries[index].Location;
            KiwadReadResult const bytes = archives.find(location.Archive)->second->Read(location.Path);
            if (!bytes.Succeeded())
            {
                failures[index] = fmt::format("{} in {} cannot be read: {}", location.Path, location.Archive, bytes.Error);
                continue;
            }
            BindReadResult const decoded = BindFile::Read(catalog, bytes.Data);
            if (!decoded.Ok() || !decoded.Decoded.Object)
            {
                failures[index] = fmt::format("{} in {} does not read: {}", location.Path, location.Archive, decoded.Ok() ? std::string("it holds no object") : decoded.Detail);
                continue;
            }
            read[index] = reader(index, entries[index], *decoded.Decoded.Object, failures[index]) ? 1 : 0;
        }
    };
    std::size_t const hardware = std::max(1u, std::thread::hardware_concurrency());
    threads = std::clamp<std::size_t>(entries.size() / EntriesPerThread, 1, hardware);
    {
        std::vector<std::jthread> workers;
        workers.reserve(threads - 1);
        for (std::size_t worker = 1; worker < threads; ++worker)
            workers.emplace_back(work);
        work();
    }

    std::map<std::string, FailureGroup> groups;
    std::size_t failed = 0;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        if (read[index] != 0)
            continue;
        ++failed;
        std::string const& path = entries[index].Location.Path;
        FailureGroup& group = groups[WithoutPath(failures[index], path)];
        if (group.Files.empty())
            group.First = failures[index];
        group.Files.push_back(path);
    }
    if (failed == 0)
        return true;
    std::size_t reported = 0;
    for (auto const& [key, group] : groups)
    {
        if (reported++ == MaxReportedFailures)
        {
            errors.push_back(fmt::format("and {} more ways {} fail", groups.size() - MaxReportedFailures, what));
            break;
        }
        errors.push_back(group.Files.size() == 1 ? group.First : fmt::format("{}; {} more fail the same way, such as {}", group.First, group.Files.size() - 1, group.Files[1]));
    }
    errors.push_back(fmt::format("{} of the {} {} under {} fail to load", failed, entries.size(), what, folder));
    return false;
}
