/*
 * Project Ambrose by Imjustchico
 * Builds type dumps and installs for tests of the managers that read templates from an install: the classes a manifest, a template, a spell with its effects and pip rank, a tiered spell with the file naming its group, the spellbook behavior with its spell trackers and a sigil with its circles and combat limits, and a game object and a wizard item template with its requirement list, equip effects and avatar info are made of, with a stat effect class a test can leave out of the dump that reads, written the way the client's own dump describes them, and an install folder whose Root.wad holds TemplateManifest.xml and the files it lists.
 */

#ifndef AMBROSE_TEMPLATEDUMPFIXTURES_H
#define AMBROSE_TEMPLATEDUMPFIXTURES_H

#include "PropertyObject.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace TemplateDumpFixtures
{
    using Json = nlohmann::json;
    using Files = std::vector<std::pair<std::string, std::vector<uint8>>>;
    using Locations = std::vector<std::pair<uint32, std::string>>;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static");
    Json Enum(std::string const& type, std::string const& name, uint32 id, Json options);
    void AddClass(Json& classes, std::string const& name, Json bases, Json properties);

    void AddTemplateClasses(Json& classes);
    void AddSpellClasses(Json& classes);
    void AddSpellbookClasses(Json& classes);
    void AddSigilClasses(Json& classes);
    void AddItemClasses(Json& classes, bool withStatEffect = false);
    std::string Dump(Json const& classes);

    std::vector<uint8> WriteBind(PropertyObjectPtr const& object);
    std::vector<uint8> Manifest(TypeCatalogPtr const& catalog, Locations const& locations);
    void WriteRoot(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, Locations const& locations, Files const& files);
}

#endif
