/*
 * Project Ambrose by Imjustchico
 * The client's TemplateManifest.xml as a map from template id to the archive and entry that hold it: a plain path is an entry of Root.wad, and a path written |World|Part|path is an entry of World-Part.wad in the same folder, as the install's own archives are named. A manifest is checked whole when it is built, refusing an id of 0, an id listed twice, an empty path and a piped path missing a part, and naming each one; it is read from an open Root.wad through its typed view.
 */

#ifndef AMBROSE_TEMPLATEMANIFEST_H
#define AMBROSE_TEMPLATEMANIFEST_H

#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class KiwadArchive;

struct TemplateLocation
{
    std::string Archive;
    std::string Path;

    bool operator==(TemplateLocation const&) const = default;
};

class TemplateManifest
{
public:
    static constexpr std::string_view Entry = "TemplateManifest.xml";
    static constexpr std::string_view RootArchive = "Root.wad";
    static constexpr std::size_t MaxReportedErrors = 100;

    TemplateManifest() = default;
    TemplateManifest(TemplateManifest const&) = delete;
    TemplateManifest& operator=(TemplateManifest const&) = delete;

    static std::optional<TemplateLocation> ParseLocation(std::string_view filename);
    static std::shared_ptr<TemplateManifest const> Build(std::vector<std::pair<uint32, std::string>> const& entries, std::vector<std::string>& errors);
    static std::shared_ptr<TemplateManifest const> Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors);

    TemplateLocation const* Find(uint32 id) const noexcept;
    std::size_t Size() const noexcept { return _locations.size(); }
    std::unordered_map<uint32, TemplateLocation> const& GetLocations() const noexcept { return _locations; }
    std::vector<std::string> GetArchives() const;

private:
    std::unordered_map<uint32, TemplateLocation> _locations;
};

#endif
