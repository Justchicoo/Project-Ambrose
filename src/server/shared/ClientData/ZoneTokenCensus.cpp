/*
 * Project Ambrose by Imjustchico
 * Takes the zone token census of extracted zones in two passes: the first decodes every token container and learns each token's name, the second counts the triggers and every string of a decoded requirement or result that names one, walking no deeper than a fixed depth. A hook is named by its property path from the token, so a time trigger's is m_timeTriggers.m_onTimeLeft; a hook result of a class the dump lacks decodes as null and is counted once, by the hash its skipped part gives; and the server-only zone token classes are named by matching the string hash of each name.
 */

#include "ZoneTokenCensus.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "SpawnRequirementCensus.h"
#include "StringHash.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <span>

namespace
{
    constexpr std::string_view ClassPrefix = "class ";
    constexpr std::string_view ResultListClass = "class ResultList";
    constexpr std::size_t MaxDepth = 16;

    std::string TypeOf(std::string_view className)
    {
        if (className.starts_with(ClassPrefix))
            className.remove_prefix(ClassPrefix.size());
        return std::string(className);
    }

    constexpr std::string_view ServerTokenClasses[] = { "class ReqZoneToken", "class ResZoneTokenEnable", "class ResZoneTokenDisable", "class ResZoneTokenModify" };

    std::string HashName(uint32 hash)
    {
        auto const named = std::find_if(std::begin(ServerTokenClasses), std::end(ServerTokenClasses), [hash](std::string_view name) { return StringHash::KiStringHash(name) == hash; });
        if (named != std::end(ServerTokenClasses))
            return fmt::format("#{} {}", hash, TypeOf(*named));
        return fmt::format("#{}", hash);
    }

    using NameSet = std::set<std::string, std::less<>>;

    template <typename Row>
    class Counter
    {
    public:
        Row& Add(std::string const& key, std::size_t zone, std::string const& path)
        {
            auto [found, added] = _rows.try_emplace(key);
            Row& row = found->second.Value;
            if (added)
            {
                row.Type = key;
                row.Example = path;
            }
            if (added || found->second.LastZone != zone)
            {
                ++row.Zones;
                found->second.LastZone = zone;
            }
            return row;
        }

        template <typename Order>
        std::vector<Row> Sorted(Order order) const
        {
            std::vector<Row> rows;
            for (auto const& [key, held] : _rows)
                rows.push_back(held.Value);
            std::stable_sort(rows.begin(), rows.end(), order);
            return rows;
        }

    private:
        struct Held
        {
            Row Value;
            std::size_t LastZone = 0;
        };
        std::map<std::string, Held, std::less<>> _rows;
    };

    bool ByCount(ClassUsage const& left, ClassUsage const& right)
    {
        return left.Count > right.Count;
    }

    PropertyObjectPtr Decode(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes)
    {
        DecodeResult decoded = ObjectSerializer::Decode(catalog, bytes, ZoneExtractor::DataOptions());
        if (!decoded.Ok())
            return nullptr;
        return std::move(decoded.Object);
    }

    std::vector<PropertyObject const*> ResultsOf(PropertyObject const& list)
    {
        std::vector<PropertyObject const*> results;
        PropertyValue const* const held = list.Get("m_results");
        if (PropertyValue::List const* const entries = held ? held->GetList() : nullptr)
            for (PropertyValue const& entry : *entries)
                results.push_back(entry.AsObject());
        return results;
    }

    void ForEachResultList(PropertyObject const& object, std::string const& prefix, std::size_t depth,
        std::function<void(std::string const&, PropertyObject const&)> const& found)
    {
        for (PropertyInfo const& property : object.GetClass().Properties)
        {
            PropertyValue const* const value = object.Get(property.Hash);
            if (!value)
                continue;
            std::string const path = prefix + property.Name;
            auto const visit = [&](PropertyObject const* held)
            {
                if (!held)
                    return;
                if (held->IsA(ResultListClass))
                    found(path, *held);
                else if (depth < MaxDepth)
                    ForEachResultList(*held, path + ".", depth + 1, found);
            };
            if (PropertyValue::List const* const entries = value->GetList())
            {
                for (PropertyValue const& entry : *entries)
                    visit(entry.AsObject());
            }
            else
                visit(value->AsObject());
        }
    }

    void ForEachText(PropertyObject const& object, std::size_t depth, std::function<void(PropertyObject const&, PropertyInfo const&, std::string const&)> const& found)
    {
        for (PropertyInfo const& property : object.GetClass().Properties)
        {
            PropertyValue const* const value = object.Get(property.Hash);
            if (!value)
                continue;
            auto const visit = [&](PropertyValue const& held)
            {
                if (std::string const* const text = held.GetIf<std::string>())
                    found(object, property, *text);
                else if (PropertyObject const* const child = held.AsObject(); child && depth < MaxDepth)
                    ForEachText(*child, depth + 1, found);
            };
            if (PropertyValue::List const* const entries = value->GetList())
            {
                for (PropertyValue const& entry : *entries)
                    visit(entry);
            }
            else
                visit(*value);
        }
    }

    std::string WithoutIndices(std::string_view path)
    {
        std::string hook;
        bool inIndex = false;
        for (char const letter : path)
        {
            if (letter == '[')
                inIndex = true;
            else if (letter == ']')
                inIndex = false;
            else if (!inIndex)
                hook.push_back(letter);
        }
        return hook;
    }
}

ZoneTokenCensus ZoneTokenCensus::Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog)
{
    ZoneTokenCensus census;
    Counter<ClassUsage> tokenClasses;
    std::map<std::string, TokenHookUsage, std::less<>> hooks;
    std::map<std::string, Counter<ClassUsage>, std::less<>> hookResults;
    std::map<std::string, std::size_t, std::less<>> hookLastZone;
    NameSet names;
    std::vector<std::vector<PropertyObjectPtr>> hookLists(zones.Zones.size());

    for (std::size_t zoneIndex = 0; zoneIndex < zones.Zones.size(); ++zoneIndex)
    {
        ExtractedZone const& zone = zones.Zones[zoneIndex];
        ZoneTokens held{ zone.Path, 0, {} };
        for (ExtractedObject const& object : zone.Objects)
        {
            if (!object.TokenContainer)
                continue;
            ++census._placements;
            ++held.Placements;
            PropertyObjectPtr container = Decode(catalog, *object.TokenContainer);
            PropertyValue const* const list = container ? container->Get("m_tokens") : nullptr;
            if (!list || !list->GetList())
            {
                ++census._undecoded;
                continue;
            }
            for (PropertyValue const& entry : *list->GetList())
            {
                ++census._tokens;
                PropertyObject const* const token = entry.AsObject();
                ZoneToken row{ token ? TypeOf(token->GetClass().Name) : std::string(UnknownClass), {}, object.ObjectId, {} };
                ++tokenClasses.Add(row.Type, zoneIndex, zones.Zones[zoneIndex].Path).Count;
                if (!token)
                {
                    held.Tokens.push_back(std::move(row));
                    continue;
                }
                if (PropertyValue const* const name = token->Get("m_debugName"))
                    if (std::string const* const text = name->GetIf<std::string>())
                        row.Name = *text;
                if (!row.Name.empty())
                    names.insert(row.Name);
                ForEachResultList(*token, std::string(), 0, [&](std::string const& hook, PropertyObject const& results)
                {
                    std::vector<PropertyObject const*> const entries = ResultsOf(results);
                    if (entries.empty())
                        return;
                    row.Hooks.push_back(hook);
                    TokenHookUsage& usage = hooks[hook];
                    usage.Hook = hook;
                    ++usage.Lists;
                    if (auto const [last, added] = hookLastZone.try_emplace(hook, zoneIndex); added || last->second != zoneIndex)
                    {
                        ++usage.Zones;
                        last->second = zoneIndex;
                    }
                    for (PropertyObject const* const result : entries)
                        if (result)
                            ++hookResults[hook].Add(TypeOf(result->GetClass().Name), zoneIndex, zones.Zones[zoneIndex].Path).Count;
                });
                held.Tokens.push_back(std::move(row));
            }
            if (container)
                hookLists[zoneIndex].push_back(std::move(container));
        }
        if (held.Placements != 0)
            census._zones.push_back(std::move(held));
    }

    std::map<std::string, std::size_t, std::less<>> zoneIndexByPath;
    for (std::size_t zoneIndex = 0; zoneIndex < zones.Zones.size(); ++zoneIndex)
        zoneIndexByPath.emplace(zones.Zones[zoneIndex].Path, zoneIndex);
    for (SkippedZonePart const& skipped : zones.Skipped)
    {
        auto const zone = zoneIndexByPath.find(skipped.Zone);
        std::string_view const path = skipped.Path;
        std::size_t const container = path.find(".m_pContainer.");
        std::size_t const results = path.rfind(".m_results[");
        if (zone == zoneIndexByPath.end() || container == std::string_view::npos || results == std::string_view::npos || results < container)
            continue;
        std::string_view inner = path.substr(container, results - container);
        if (std::size_t const tokens = inner.find(".m_tokens["); tokens != std::string_view::npos)
            if (std::size_t const after = inner.find("].", tokens); after != std::string_view::npos)
                inner = inner.substr(after + 2);
        ++hookResults[WithoutIndices(inner)].Add(HashName(skipped.ClassHash), zone->second, skipped.Zone).Count;
    }

    Counter<TriggerResultUsage> triggerResults;
    Counter<ClassUsage> triggerRequirements;
    Counter<ClassUsage> references;
    for (std::size_t zoneIndex = 0; zoneIndex < zones.Zones.size(); ++zoneIndex)
    {
        ExtractedZone const& zone = zones.Zones[zoneIndex];
        auto const look = [&](PropertyObject const& object, std::string_view where)
        {
            ForEachText(object, 0, [&](PropertyObject const& holder, PropertyInfo const& property, std::string const& text)
            {
                if (holder.IsA("class ZoneTokenBase") || !names.contains(text))
                    return;
                ++references.Add(fmt::format("{}  {}.{}", where, TypeOf(holder.GetClass().Name), property.Name), zoneIndex, zones.Zones[zoneIndex].Path).Count;
            });
        };
        auto const lookAt = [&](std::optional<std::vector<uint8>> const& bytes, std::string_view where)
        {
            if (!bytes)
                return;
            if (PropertyObjectPtr const object = Decode(catalog, *bytes))
                look(*object, where);
        };

        for (PropertyObjectPtr const& container : hookLists[zoneIndex])
            ForEachResultList(*container, std::string(), 0, [&](std::string const&, PropertyObject const& results) { look(results, "token result"); });
        for (ExtractedObject const& object : zone.Objects)
            lookAt(object.SpawnRequirements, "spawn requirement");
        for (ExtractedSpawner const& spawner : zone.Spawners)
            for (ExtractedSpawnItem const& item : spawner.Items)
                lookAt(item.Object.SpawnRequirements, "spawn requirement");
        for (ExtractedVolume const& volume : zone.Volumes)
            lookAt(volume.SpawnRequirements, "volume requirement");
        for (ExtractedTrigger const& trigger : zone.Triggers)
        {
            ++census._triggers;
            if (trigger.Requirements)
            {
                if (PropertyObjectPtr const requirements = Decode(catalog, *trigger.Requirements))
                {
                    for (std::string const& type : SpawnRequirementCensus::TypesOf(*requirements))
                        ++triggerRequirements.Add(type == SpawnRequirementCensus::UnnamedRequirement ? std::string(UnknownClass) : type, zoneIndex, zones.Zones[zoneIndex].Path).Count;
                    look(*requirements, "trigger requirement");
                }
                else
                    ++triggerRequirements.Add(std::string(UnknownClass), zoneIndex, zones.Zones[zoneIndex].Path).Count;
            }
            auto const count = [&](std::vector<ExtractedTriggerResult> const& list, bool cooldown)
            {
                for (ExtractedTriggerResult const& result : list)
                {
                    TriggerResultUsage& usage = triggerResults.Add(result.ClassName ? TypeOf(*result.ClassName) : HashName(result.ClassHash), zoneIndex, zones.Zones[zoneIndex].Path);
                    ++(cooldown ? usage.CooldownResults : usage.Results);
                    lookAt(result.Data, cooldown ? "trigger cooldown result" : "trigger result");
                }
            };
            count(trigger.Results, false);
            count(trigger.CooldownResults, true);
        }
    }
    for (SkippedZonePart const& skipped : zones.Skipped)
    {
        auto const zone = zoneIndexByPath.find(skipped.Zone);
        if (zone != zoneIndexByPath.end() && skipped.Path.starts_with("class TriggerList.") && skipped.Path.find(".m_requirements") != std::string::npos)
            ++triggerRequirements.Add(HashName(skipped.ClassHash), zone->second, skipped.Zone).Count;
    }

    census._tokenClasses = tokenClasses.Sorted(ByCount);
    for (auto& [hook, usage] : hooks)
    {
        if (auto const results = hookResults.find(hook); results != hookResults.end())
            usage.Results = results->second.Sorted(ByCount);
        census._hooks.push_back(std::move(usage));
    }
    std::stable_sort(census._hooks.begin(), census._hooks.end(), [](TokenHookUsage const& left, TokenHookUsage const& right) { return left.Lists > right.Lists; });
    census._triggerResults = triggerResults.Sorted([](TriggerResultUsage const& left, TriggerResultUsage const& right)
    {
        return left.Results + left.CooldownResults > right.Results + right.CooldownResults;
    });
    census._triggerRequirements = triggerRequirements.Sorted(ByCount);
    census._references = references.Sorted(ByCount);
    return census;
}
