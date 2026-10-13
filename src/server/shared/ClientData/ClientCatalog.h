/*
 * Project Ambrose by Imjustchico
 * The client catalog: every template id the user's own install lists, sorted into a category such as Mobs, NPCs, Equipment, Housing, Pets or Spells by its class and behaviors and into a group by the folder below ObjectData that holds it, each with its object name, class, behaviors, display key and the text that key holds in one locale, and every zone with its display name, named locations and the template ids it places, written as an index and one JSON file per category, split into numbered parts where a file would pass a size cap, in a folder named by the client revision, which replaces the folder a run before it wrote whole.
 */

#ifndef AMBROSE_CLIENTCATALOG_H
#define AMBROSE_CLIENTCATALOG_H

#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct TemplateExtraction;
struct ZoneExtraction;
class LocaleTable;

struct CatalogTemplate
{
    uint32 TemplateId = 0;
    std::string Group;
    std::string ObjectName;
    std::string ClassName;
    std::string DisplayKey;
    std::string DisplayName;
    std::string Archive;
    std::string Path;
    std::vector<std::string> Behaviors;
};

struct CatalogCategory
{
    std::string Name;
    std::vector<CatalogTemplate> Templates;
};

struct CatalogLocation
{
    std::string Name;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Direction = 0.0f;
};

struct CatalogZone
{
    std::string Path;
    std::string DisplayKey;
    std::string DisplayName;
    std::vector<CatalogLocation> Locations;
    std::vector<uint32> Templates;
};

class ClientCatalog
{
public:
    static constexpr std::string_view IndexFile = "index.json";
    static constexpr std::string_view ZonesStem = "zones";
    static constexpr std::size_t MaxFileBytes = 900 * 1024;
    static constexpr std::string_view TemplateFolder = "templates";
    static constexpr std::string_view ObjectDataFolder = "ObjectData/";
    static constexpr std::string_view ClassPrefix = "class ";
    static constexpr std::string_view OtherCategory = "Other";

    static std::string GroupOf(std::string_view path);
    static std::string_view CategoryOf(std::string_view className, std::vector<std::string> const& behaviors, std::string_view group);
    static ClientCatalog Build(TemplateExtraction const& templates, ZoneExtraction const& zones, LocaleTable const* locale, std::string revision);

    bool Write(std::filesystem::path const& folder, std::string& error) const;

    std::string const& GetRevision() const noexcept { return _revision; }
    std::string const& GetLocale() const noexcept { return _locale; }
    std::vector<CatalogCategory> const& GetCategories() const noexcept { return _categories; }
    std::vector<CatalogZone> const& GetZones() const noexcept { return _zones; }
    std::size_t GetTemplateCount() const noexcept;
    std::size_t GetNamedCount() const noexcept;

private:
    std::string _revision;
    std::string _locale;
    std::vector<CatalogCategory> _categories;
    std::vector<CatalogZone> _zones;
};

#endif
