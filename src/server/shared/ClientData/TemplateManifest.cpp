/*
 * Project Ambrose by Imjustchico
 * Parses a manifest path into its archive and entry and builds the map, where a world or part that could step out of the GameData folder is refused like a missing one, and the report stops listing faults after MaxReportedErrors with a count of the rest.
 */

#include "TemplateManifest.h"
#include "BindFile.h"
#include "KiwadArchive.h"
#include "ObjectViews.h"

#include <fmt/format.h>

#include <algorithm>
#include <set>

namespace
{
    bool IsArchivePart(std::string_view part) noexcept
    {
        if (part.empty() || part == "." || part == "..")
            return false;
        return std::none_of(part.begin(), part.end(), [](char c) { return c == '/' || c == '\\' || c == ':' || c == '|' || static_cast<unsigned char>(c) < 0x20; });
    }

    void Report(std::vector<std::string>& errors, std::size_t& faults, std::string error)
    {
        if (++faults <= TemplateManifest::MaxReportedErrors)
            errors.push_back(std::move(error));
    }
}

std::optional<TemplateLocation> TemplateManifest::ParseLocation(std::string_view filename)
{
    if (filename.empty())
        return std::nullopt;
    if (filename.front() != '|')
        return TemplateLocation{ std::string(RootArchive), std::string(filename) };
    std::size_t const worldEnd = filename.find('|', 1);
    if (worldEnd == std::string_view::npos)
        return std::nullopt;
    std::size_t const partEnd = filename.find('|', worldEnd + 1);
    if (partEnd == std::string_view::npos)
        return std::nullopt;
    std::string_view const world = filename.substr(1, worldEnd - 1);
    std::string_view const part = filename.substr(worldEnd + 1, partEnd - worldEnd - 1);
    std::string_view const path = filename.substr(partEnd + 1);
    if (!IsArchivePart(world) || !IsArchivePart(part) || path.empty())
        return std::nullopt;
    return TemplateLocation{ fmt::format("{}-{}.wad", world, part), std::string(path) };
}

std::shared_ptr<TemplateManifest const> TemplateManifest::Build(std::vector<std::pair<uint32, std::string>> const& entries, std::vector<std::string>& errors)
{
    std::size_t faults = 0;
    auto manifest = std::make_shared<TemplateManifest>();
    manifest->_locations.reserve(entries.size());
    for (auto const& [id, filename] : entries)
    {
        if (id == 0)
        {
            Report(errors, faults, fmt::format("{} lists template 0 at {}, and 0 names no template", Entry, filename.empty() ? std::string("no path") : filename));
            continue;
        }
        std::optional<TemplateLocation> location = ParseLocation(filename);
        if (!location)
        {
            Report(errors, faults, filename.empty() ? fmt::format("{} gives template {} no path", Entry, id)
                                                     : fmt::format("{} gives template {} the path {}, which names no archive and entry", Entry, id, filename));
            continue;
        }
        if (!manifest->_locations.emplace(id, std::move(*location)).second)
            Report(errors, faults, fmt::format("{} lists template {} twice", Entry, id));
    }
    if (faults > MaxReportedErrors)
        errors.push_back(fmt::format("and {} more problems", faults - MaxReportedErrors));
    if (faults != 0)
        return nullptr;
    return manifest;
}

std::shared_ptr<TemplateManifest const> TemplateManifest::Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors)
{
    KiwadReadResult const bytes = root.Read(Entry);
    if (!bytes.Succeeded())
    {
        errors.push_back(fmt::format("{} cannot be read: {}", Entry, bytes.Error));
        return nullptr;
    }
    BindReadResult const decoded = BindFile::Read(catalog, bytes.Data);
    std::optional<TemplateManifestView> const view = decoded.Ok() && decoded.Decoded.Object ? TemplateManifestView::From(*decoded.Decoded.Object) : std::nullopt;
    if (!view)
    {
        errors.push_back(fmt::format("{} does not read as a TemplateManifest: {}", Entry, decoded.Ok() ? std::string("its root is another class") : decoded.Detail));
        return nullptr;
    }
    std::vector<std::pair<uint32, std::string>> entries;
    entries.reserve(view->GetSerializedTemplates().size());
    std::size_t position = 0;
    for (PropertyValue const& entry : view->GetSerializedTemplates())
    {
        std::optional<TemplateLocationView> const location = entry.AsObject() ? TemplateLocationView::From(*entry.AsObject()) : std::nullopt;
        if (!location)
        {
            errors.push_back(fmt::format("{} entry {} is not a TemplateLocation", Entry, position));
            return nullptr;
        }
        entries.emplace_back(location->GetId(), location->GetFilename());
        ++position;
    }
    return Build(entries, errors);
}

TemplateLocation const* TemplateManifest::Find(uint32 id) const noexcept
{
    auto const found = _locations.find(id);
    return found == _locations.end() ? nullptr : &found->second;
}

std::vector<std::string> TemplateManifest::GetArchives() const
{
    std::set<std::string> archives;
    for (auto const& [id, location] : _locations)
        archives.insert(location.Archive);
    return { archives.begin(), archives.end() };
}
