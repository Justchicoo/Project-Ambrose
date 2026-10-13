/*
 * Project Ambrose by Imjustchico
 * Builds the client catalog from a template and zone extraction, sorting each template into a category by its class and the behaviors that mark what it is, a mob's deck, a mount, furniture, a pet, and into a group by the folder that holds it, naming each display key through the locale table given, and writes it to a partial folder first that then takes the revision folder's place.
 */

#include "ClientCatalog.h"
#include "ConfigMgr.h"
#include "LocaleStore.h"
#include "TemplateExtractor.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <system_error>
#include <tuple>
#include <utility>

namespace
{
    using Json = nlohmann::ordered_json;

    std::string DisplayOf(LocaleTable const* locale, std::string const& key)
    {
        if (locale == nullptr || key.empty())
            return {};
        std::string const* const text = locale->Find(key);
        return text ? *text : std::string();
    }

    std::string Dump(Json const& value)
    {
        return value.dump(-1, ' ', false, Json::error_handler_t::replace);
    }

    bool WriteFile(std::filesystem::path const& file, std::string const& text, std::string& error)
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << text;
        out.close();
        if (!out)
        {
            error = fmt::format("cannot write {}", ConfigMgr::PathToUtf8(file));
            return false;
        }
        return true;
    }

    std::optional<Json> WriteParts(std::filesystem::path const& folder, std::string_view prefix, std::string const& stem, std::vector<Json> const& rows, std::string& error)
    {
        std::vector<std::string> parts(1);
        for (Json const& row : rows)
        {
            std::string line = Dump(row);
            if (!parts.back().empty() && parts.back().size() + line.size() + 4 > ClientCatalog::MaxFileBytes)
                parts.emplace_back();
            parts.back() += parts.back().empty() ? "[\n" : ",\n";
            parts.back() += line;
        }
        Json files = Json::array();
        for (std::size_t index = 0; index < parts.size(); ++index)
        {
            std::string const name = parts.size() == 1 ? stem + ".json" : fmt::format("{}-{:02}.json", stem, index + 1);
            if (!WriteFile(folder / ConfigMgr::PathFromUtf8(name), parts[index].empty() ? std::string("[]\n") : parts[index] + "\n]\n", error))
                return std::nullopt;
            files.push_back(prefix.empty() ? name : fmt::format("{}/{}", prefix, name));
        }
        return files;
    }
}

std::string ClientCatalog::GroupOf(std::string_view path)
{
    if (path.starts_with(ObjectDataFolder))
        path.remove_prefix(ObjectDataFolder.size());
    std::size_t const last = path.rfind('/');
    return last == std::string_view::npos ? std::string() : std::string(path.substr(0, last));
}

std::string_view ClientCatalog::CategoryOf(std::string_view className, std::vector<std::string> const& behaviors, std::string_view group)
{
    auto const has = [&behaviors](std::string_view name) { return std::find(behaviors.begin(), behaviors.end(), name) != behaviors.end(); };
    auto const hasAny = [&behaviors](std::initializer_list<std::string_view> names)
    {
        return std::any_of(behaviors.begin(), behaviors.end(), [&names](std::string const& behavior) { return std::find(names.begin(), names.end(), behavior) != names.end(); });
    };
    auto const isGroup = [group](std::string_view folder) { return group == folder || group.starts_with(std::string(folder) + "/"); };

    static std::map<std::string_view, std::string_view> const byClass{
        { "SpellTemplate", "Spells" }, { "TieredSpellTemplate", "Spells" }, { "CastleMagicSpellTemplate", "Spells" }, { "CantripsSpellTemplate", "Spells" },
        { "GardenSpellTemplate", "Spells" }, { "WhirlyBurlySpellTemplate", "Spells" }, { "FishingSpellTemplate", "Spells" }, { "SpellFusionTemplate", "Spells" },
        { "DeckTemplate", "Decks" }, { "RecipeTemplate", "Recipes" }, { "ReagentItemTemplate", "Reagents" }, { "PetSnackItemTemplate", "PetSnacks" },
        { "PetTalentTemplate", "PetTalents" }, { "PetDerbyTalentTemplate", "PetTalents" }, { "WizPolymorphTemplate", "Polymorphs" }, { "SoundDefTemplate", "Sounds" },
        { "ItemBundleTemplate", "Bundles" }, { "BoosterPackTemplate", "Bundles" }, { "WizPetTemplate", "Pets" }, { "WizMountTemplate", "Mounts" },
        { "DynamicTriggerTemplate", "Triggers" }, { "SigilDynamicTriggerTemplate", "Triggers" }, { "CombatSigilTemplate", "Sigils" }, { "MinigameSigilTemplate", "Sigils" },
        { "PvPCombatSigilTemplate", "Sigils" }, { "BattlegroundSigilTemplate", "Sigils" }, { "DynamicSigilTemplate", "Sigils" }, { "MatchTemplate", "PvP" },
        { "TournamentTemplate", "PvP" }, { "BracketTemplate", "PvP" }, { "TournamentLevelModifiers", "PvP" }, { "MagicSchoolTemplate", "Schools" },
        { "GoldAmountTemplate", "Currency" }, { "CurrencyTemplate", "Currency" }, { "LunariAmountTemplate", "Currency" }, { "TourneyTokensAmountTemplate", "Currency" },
        { "CinematicDefTemplate", "Cinematics" }, { "CinematicActorTemplate", "Cinematics" }, { "WizCinematicActorTemplate", "Cinematics" },
        { "TutorialQuestTemplate", "Tutorials" }, { "ItemSetBonusTemplate", "SetBonuses" } };
    if (auto const found = byClass.find(className); found != byClass.end())
        return found->second;

    if (className == "WizGameObjectTemplate" || className == "GameObjectTemplate")
        return has("MobDeckBehavior") ? "Mobs" : has("NPCBehavior") ? "NPCs" : "Objects";

    if (className == "WizItemTemplate" || className == "ItemTemplate" || className == "EquipmentTemplate")
    {
        if (has("MountItemBehavior"))
            return "Mounts";
        if (has("FurnitureInfoBehavior") || hasAny({ "HousingTextureBehavior", "HousingMusicBehavior", "HousingMusicPlayerBehavior", "HousingSigilBehavior", "HousingTeleporterBehavior",
            "HousingSignBehavior", "ExtraHousingZoneBehavior", "HousingGameProximityBehavior", "HousingPetBehavior", "HousingHallOfFameBehavior" }))
            return "Housing";
        if (has("PetItemBehavior"))
            return "Pets";
        if (has("PetJewelBehavior") || isGroup("Jewels"))
            return "Jewels";
        if (hasAny({ "ElixirBehavior", "ElixirBenefitBehavior", "WorldElixirBehavior", "LevelUpElixirBehavior" }))
            return "Elixirs";
        if (has("BasicDeckBehavior"))
            return "Decks";
        if (has("JewelSocketBehavior"))
            return "Equipment";
        return "Items";
    }
    return OtherCategory;
}

ClientCatalog ClientCatalog::Build(TemplateExtraction const& templates, ZoneExtraction const& zones, LocaleTable const* locale, std::string revision)
{
    ClientCatalog catalog;
    catalog._revision = std::move(revision);
    catalog._locale = locale ? locale->GetLocale() : std::string();

    std::map<std::string_view, std::size_t> byName;
    for (ExtractedTemplate const& found : templates.Templates)
    {
        CatalogTemplate entry;
        entry.TemplateId = found.TemplateId;
        entry.Group = GroupOf(found.Path);
        entry.ObjectName = found.ObjectName;
        entry.ClassName = found.ClassName.starts_with(ClassPrefix) ? found.ClassName.substr(ClassPrefix.size()) : found.ClassName;
        entry.DisplayKey = found.DisplayKey;
        entry.DisplayName = DisplayOf(locale, found.DisplayKey);
        entry.Archive = found.Archive;
        entry.Path = found.Path;
        for (ExtractedBehavior const& behavior : found.Behaviors)
            if (behavior.Name && !behavior.Name->empty())
                entry.Behaviors.push_back(*behavior.Name);
        std::string_view const category = CategoryOf(entry.ClassName, entry.Behaviors, entry.Group);
        auto [slot, added] = byName.try_emplace(category, catalog._categories.size());
        if (added)
            catalog._categories.push_back({ std::string(category), {} });
        catalog._categories[slot->second].Templates.push_back(std::move(entry));
    }
    std::sort(catalog._categories.begin(), catalog._categories.end(), [](CatalogCategory const& left, CatalogCategory const& right) { return left.Name < right.Name; });
    for (CatalogCategory& category : catalog._categories)
        std::sort(category.Templates.begin(), category.Templates.end(), [](CatalogTemplate const& left, CatalogTemplate const& right)
        {
            return std::tie(left.Group, left.TemplateId) < std::tie(right.Group, right.TemplateId);
        });

    for (ExtractedZone const& found : zones.Zones)
    {
        CatalogZone zone;
        zone.Path = found.Path;
        zone.DisplayKey = found.DisplayNameKey;
        zone.DisplayName = DisplayOf(locale, found.DisplayNameKey);
        for (ExtractedLocation const& location : found.Locations)
            zone.Locations.push_back({ location.Name, location.Location.X, location.Location.Y, location.Location.Z, location.Direction });
        std::set<uint32> placed;
        for (ExtractedObject const& object : found.Objects)
            if (object.TemplateId != 0 && object.TemplateId <= std::numeric_limits<uint32>::max())
                placed.insert(static_cast<uint32>(object.TemplateId));
        zone.Templates.assign(placed.begin(), placed.end());
        catalog._zones.push_back(std::move(zone));
    }
    std::sort(catalog._zones.begin(), catalog._zones.end(), [](CatalogZone const& left, CatalogZone const& right) { return left.Path < right.Path; });
    return catalog;
}

std::size_t ClientCatalog::GetTemplateCount() const noexcept
{
    std::size_t count = 0;
    for (CatalogCategory const& category : _categories)
        count += category.Templates.size();
    return count;
}

std::size_t ClientCatalog::GetNamedCount() const noexcept
{
    std::size_t count = 0;
    for (CatalogCategory const& category : _categories)
        count += static_cast<std::size_t>(std::count_if(category.Templates.begin(), category.Templates.end(), [](CatalogTemplate const& entry) { return !entry.DisplayName.empty(); }));
    return count;
}

bool ClientCatalog::Write(std::filesystem::path const& folder, std::string& error) const
{
    if (_revision.empty() || _revision.find_first_of("/\\") != std::string::npos || _revision == "." || _revision == "..")
    {
        error = fmt::format("'{}' cannot name a catalog folder", _revision);
        return false;
    }
    std::filesystem::path const target = folder / ConfigMgr::PathFromUtf8(_revision);
    std::filesystem::path const partial = folder / ConfigMgr::PathFromUtf8(_revision + ".partial");
    std::error_code code;
    std::filesystem::remove_all(partial, code);
    std::filesystem::create_directories(partial / TemplateFolder, code);
    if (code)
    {
        error = fmt::format("cannot create {}: {}", ConfigMgr::PathToUtf8(partial / TemplateFolder), code.message());
        return false;
    }

    Json categories = Json::array();
    for (CatalogCategory const& category : _categories)
    {
        std::map<std::string, std::size_t> groups;
        std::vector<Json> rows;
        rows.reserve(category.Templates.size());
        for (CatalogTemplate const& entry : category.Templates)
        {
            ++groups[entry.Group];
            rows.push_back(Json{ { "id", entry.TemplateId }, { "name", entry.ObjectName }, { "display", entry.DisplayName }, { "class", entry.ClassName }, { "group", entry.Group },
                { "behaviors", entry.Behaviors }, { "displayKey", entry.DisplayKey }, { "archive", entry.Archive }, { "path", entry.Path } });
        }
        std::optional<Json> files = WriteParts(partial / TemplateFolder, TemplateFolder, category.Name, rows, error);
        if (!files)
            return false;
        Json groupCounts = Json::object();
        for (auto const& [group, count] : groups)
            groupCounts[group] = count;
        categories.push_back(Json{ { "name", category.Name }, { "files", std::move(*files) }, { "count", category.Templates.size() }, { "groups", std::move(groupCounts) } });
    }

    std::vector<Json> zones;
    zones.reserve(_zones.size());
    for (CatalogZone const& zone : _zones)
    {
        Json locations = Json::array();
        for (CatalogLocation const& location : zone.Locations)
            locations.push_back(Json{ { "name", location.Name }, { "x", location.X }, { "y", location.Y }, { "z", location.Z }, { "direction", location.Direction } });
        zones.push_back(Json{ { "path", zone.Path }, { "display", zone.DisplayName }, { "displayKey", zone.DisplayKey }, { "locations", std::move(locations) }, { "templates", zone.Templates } });
    }
    std::optional<Json> zoneFiles = WriteParts(partial, {}, std::string(ZonesStem), zones, error);
    if (!zoneFiles)
        return false;

    Json const index{ { "revision", _revision }, { "locale", _locale }, { "templates", GetTemplateCount() }, { "named", GetNamedCount() }, { "zones", _zones.size() },
        { "zoneFiles", std::move(*zoneFiles) }, { "categories", std::move(categories) } };
    if (!WriteFile(partial / IndexFile, index.dump(2, ' ', false, Json::error_handler_t::replace) + "\n", error))
        return false;

    std::filesystem::remove_all(target, code);
    if (code)
    {
        error = fmt::format("cannot replace {}: {}", ConfigMgr::PathToUtf8(target), code.message());
        return false;
    }
    std::filesystem::rename(partial, target, code);
    if (code)
    {
        error = fmt::format("cannot move {} to {}: {}", ConfigMgr::PathToUtf8(partial), ConfigMgr::PathToUtf8(target), code.message());
        return false;
    }
    return true;
}
