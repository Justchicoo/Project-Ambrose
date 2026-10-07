/*
 * Project Ambrose by Imjustchico
 * Reads TemplateManifest.xml from Root.wad and every template it lists in id order, each from the archive the manifest names, opened once and kept for the run. A template is a BINd file whose root is a CoreTemplate; a GameObjectTemplate, WizItemTemplate among them, also gives its object name, display and description keys, visual id, icon, object type, adjectives and loot table, and any other CoreTemplate gives the name its ObjectName property holds; an item template is read into its item record and an item set bonus template into its set bonus record, either failing the run when it holds a class nothing describes. Each behavior slot keeps its place: a behavior the dump describes gives its m_behaviorName, an empty slot gives an empty name, and a behavior of a class nothing describes gives the class hash the decoder reports for that slot and the m_behaviorName its skipped bytes hold, which every behavior template inherits and writes under the same hash, or no name when they hold none.
 */

#include "TemplateExtractor.h"
#include "BindFile.h"
#include "ItemTemplateRecord.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "ObjectViews.h"
#include "SkippedValue.h"
#include "StringHash.h"
#include "TemplateManifest.h"

#include <fmt/format.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
    std::string ObjectNameOf(PropertyObject const& object)
    {
        std::vector<PropertyInfo> const& properties = object.GetClass().Properties;
        for (std::size_t ordinal = 0; ordinal < properties.size(); ++ordinal)
        {
            if (!properties[ordinal].HasFlag(PropertyFlag::ObjectName))
                continue;
            PropertyValue const* const value = object.GetAt(ordinal);
            if (std::string const* const text = value ? value->GetIf<std::string>() : nullptr)
                return *text;
        }
        return {};
    }

    std::string TextOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? *text : std::string();
    }
}

void TemplateExtraction::AddError(std::string error)
{
    ++ErrorCount;
    if (Errors.size() < MaxReportedProblems)
        Errors.push_back(std::move(error));
}

void TemplateExtraction::AddUnread(std::string problem)
{
    ++UnreadCount;
    if (Unread.size() < MaxReportedProblems)
        Unread.push_back(std::move(problem));
}

std::size_t TemplateExtraction::GetAdjectiveCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedTemplate const& found : Templates)
        count += found.Adjectives.size();
    return count;
}

std::size_t TemplateExtraction::GetBehaviorCount() const noexcept
{
    std::size_t count = 0;
    for (ExtractedTemplate const& found : Templates)
        count += found.Behaviors.size();
    return count;
}

std::size_t TemplateExtraction::GetUnknownBehaviorCount() const noexcept
{
    std::size_t count = 0;
    for (auto const& [hash, seen] : UnknownBehaviorClasses)
        count += seen;
    return count;
}

std::size_t TemplateExtraction::GetNpcTemplateCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Templates.begin(), Templates.end(), [](ExtractedTemplate const& found)
    {
        return std::any_of(found.Behaviors.begin(), found.Behaviors.end(), [](ExtractedBehavior const& behavior) { return behavior.Name == NpcBehavior; });
    }));
}

std::size_t TemplateExtraction::GetItemCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Templates.begin(), Templates.end(), [](ExtractedTemplate const& found) { return found.Item.has_value(); }));
}

std::size_t TemplateExtraction::GetSetBonusCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Templates.begin(), Templates.end(), [](ExtractedTemplate const& found) { return found.SetBonus.has_value(); }));
}

ExtractedTemplate const* TemplateExtraction::Find(uint32 templateId) const noexcept
{
    auto const found = std::lower_bound(Templates.begin(), Templates.end(), templateId, [](ExtractedTemplate const& row, uint32 id) { return row.TemplateId < id; });
    return found != Templates.end() && found->TemplateId == templateId ? &*found : nullptr;
}

bool TemplateExtractor::ReadTemplate(TypeCatalogPtr const& catalog, uint32 templateId, TemplateLocation const& location, std::span<uint8 const> bytes, TemplateExtraction& extraction)
{
    BindReadResult const read = BindFile::Read(catalog, bytes, std::nullopt, true);
    if (!read.Ok() || !read.Decoded.Object)
    {
        extraction.AddUnread(fmt::format("{} {} in {}: {}", templateId, location.Path, location.Archive, read.Detail.empty() ? std::string(BindFile::GetStatusName(read.Status)) : read.Detail));
        return false;
    }
    PropertyObject const& object = *read.Decoded.Object;
    std::optional<CoreTemplateView> const core = CoreTemplateView::From(object);
    if (!core)
    {
        extraction.AddUnread(fmt::format("{} {} in {} is a {}, which is not a CoreTemplate", templateId, location.Path, location.Archive, object.GetClass().Name));
        return false;
    }

    ExtractedTemplate row;
    row.TemplateId = templateId;
    row.ClassName = object.GetClass().Name;
    row.ClassHash = object.GetClass().Hash;
    row.Archive = location.Archive;
    row.Path = location.Path;
    if (std::optional<GameObjectTemplateView> const game = GameObjectTemplateView::From(object))
    {
        row.ObjectName = game->GetObjectName();
        row.DisplayKey = game->GetDisplayName();
        row.DescriptionKey = game->GetDescription();
        row.Icon = game->GetIcon();
        row.VisualId = game->GetVisualId();
        row.ObjectType = game->GetObjectType();
        for (PropertyValue const& adjective : game->GetAdjectiveList())
            if (std::string const* const text = adjective.GetIf<std::string>())
                row.Adjectives.push_back(*text);
        row.LootTable = TextOf(object, "m_lootTable");
    }
    else
        row.ObjectName = ObjectNameOf(object);

    static uint32 const behaviorName = StringHash::PropertyHash("std::string", "m_behaviorName");
    std::map<std::string, uint32, std::less<>> unknown;
    std::map<std::string, std::string, std::less<>> unknownNames;
    for (DecodeIssue const& issue : read.Decoded.Issues)
    {
        if (issue.Kind == DecodeIssueKind::UnknownClass)
            unknown.emplace(issue.Path, issue.Hash);
        else if (issue.Kind == DecodeIssueKind::UnknownClassProperty && issue.Hash == behaviorName)
            if (std::optional<std::string> text = SkippedValue::Text(issue.Bits, issue.Value))
                unknownNames.emplace(issue.Path, std::move(*text));
    }
    PropertyValue::List const& behaviors = core->GetBehaviors();
    for (std::size_t position = 0; position < behaviors.size(); ++position)
    {
        ExtractedBehavior behavior;
        if (PropertyObject const* const found = behaviors[position].AsObject())
        {
            behavior.ClassHash = found->GetClass().Hash;
            behavior.Name = TextOf(*found, "m_behaviorName");
        }
        else if (auto const hash = unknown.find(fmt::format("{}.m_behaviors[{}]", row.ClassName, position)); hash != unknown.end())
        {
            behavior.ClassHash = hash->second;
            if (auto const name = unknownNames.find(hash->first); name != unknownNames.end())
                behavior.Name = name->second;
            ++extraction.UnknownBehaviorClasses[hash->second];
        }
        else
            behavior.Name = std::string();
        row.Behaviors.push_back(std::move(behavior));
    }
    if (ItemTemplateRecord::IsItem(object))
    {
        std::string error;
        row.Item = ItemTemplateRecord::Read(object, templateId, location.Path, read.Decoded.Issues, error);
        if (!row.Item)
            extraction.AddError(fmt::format("{} in {}: {}", templateId, location.Archive, error));
    }
    else if (ItemSetBonusRecord::IsSetBonus(object))
    {
        std::string error;
        row.SetBonus = ItemSetBonusRecord::Read(object, templateId, location.Path, read.Decoded.Issues, error);
        if (!row.SetBonus)
            extraction.AddError(fmt::format("{} in {}: {}", templateId, location.Archive, error));
    }
    extraction.Templates.push_back(std::move(row));
    return true;
}

TemplateExtraction TemplateExtractor::Extract(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog)
{
    TemplateExtraction extraction;
    std::string error;
    std::unique_ptr<KiwadArchive> root = KiwadArchive::Open(gameData / TemplateManifest::RootArchive, error);
    if (!root)
    {
        extraction.AddError(fmt::format("cannot open {}: {}", TemplateManifest::RootArchive, error));
        return extraction;
    }
    std::vector<std::string> errors;
    std::shared_ptr<TemplateManifest const> const manifest = TemplateManifest::Read(*root, catalog, errors);
    for (std::string& problem : errors)
        extraction.AddError(std::move(problem));
    if (!manifest)
        return extraction;

    std::vector<std::pair<uint32, TemplateLocation const*>> ordered;
    ordered.reserve(manifest->Size());
    for (auto const& [id, location] : manifest->GetLocations())
        ordered.emplace_back(id, &location);
    std::sort(ordered.begin(), ordered.end(), [](auto const& left, auto const& right) { return left.first < right.first; });
    extraction.ManifestEntries = ordered.size();
    extraction.Templates.reserve(ordered.size());

    std::map<std::string, std::unique_ptr<KiwadArchive>, std::less<>> archives;
    archives.emplace(std::string(TemplateManifest::RootArchive), std::move(root));
    for (auto const& [id, location] : ordered)
    {
        bool const objectData = location->Archive == TemplateManifest::RootArchive && location->Path.starts_with(TemplateExtraction::ObjectDataFolder);
        if (objectData)
            ++extraction.ObjectDataEntries;
        auto archive = archives.find(location->Archive);
        if (archive == archives.end())
        {
            std::unique_ptr<KiwadArchive> opened = KiwadArchive::Open(gameData / location->Archive, error);
            if (!opened)
            {
                extraction.AddUnread(fmt::format("{} {}: cannot open {}: {}", id, location->Path, location->Archive, error));
                continue;
            }
            archive = archives.emplace(location->Archive, std::move(opened)).first;
        }
        KiwadReadResult const bytes = archive->second->Read(location->Path, MaxEntryBytes);
        if (!bytes.Succeeded())
        {
            extraction.AddUnread(fmt::format("{} {} in {}: {}", id, location->Path, location->Archive, bytes.Error));
            continue;
        }
        if (ReadTemplate(catalog, id, *location, bytes.Data, extraction) && objectData)
            ++extraction.ObjectDataRead;
    }
    return extraction;
}

std::optional<TemplateExtraction> TemplateExtractor::ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error,
    TypeDumpLoader::RawDump supplement)
{
    std::filesystem::path const gameData = clientDir / "Data" / "GameData";
    if (!std::filesystem::is_directory(gameData))
    {
        error = fmt::format("{} is not a folder", ConfigMgr::PathToUtf8(gameData));
        return std::nullopt;
    }
    TypedViewRegistry views;
    ObjectViews::RegisterAll(views);
    TypeRegistry registry(&views);
    std::vector<std::string> refused;
    if (!supplement.Classes.empty() && !registry.SetSupplement(std::move(supplement), "the server classes", refused))
    {
        error = fmt::format("the server classes cannot join the type dump{}{}", refused.empty() ? "" : ": ", refused.empty() ? std::string() : refused.front());
        return std::nullopt;
    }
    if (!registry.LoadFromFile(typeDump))
    {
        std::vector<std::string> const problems = registry.GetErrors();
        error = fmt::format("cannot load the type dump {}{}{}", ConfigMgr::PathToUtf8(typeDump), problems.empty() ? "" : ": ", problems.empty() ? std::string() : problems.front());
        return std::nullopt;
    }
    return Extract(gameData, registry.GetCatalog());
}
