/*
 * Project Ambrose by Imjustchico
 * Extracts every template TemplateManifest.xml lists from the user's own install into rows the world database keeps: its class, object name, display and description keys, visual id, icon, object type, loot table, where it lives, its adjectives and its behaviors in the order the template lists them, and for an item template its item fields. A behavior of a class nothing describes keeps its place and the class hash the file gives it, and an entry that does not decode, or is not a CoreTemplate, is counted and named up to a cap without stopping the rest; only a manifest that cannot be read, or an item template holding an object of a class the type dump does not list outside its behaviors, is an error.
 */

#ifndef AMBROSE_TEMPLATEEXTRACTOR_H
#define AMBROSE_TEMPLATEEXTRACTOR_H

#include "ItemTemplateRecord.h"
#include "TypeDumpLoader.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct TemplateLocation;

struct ExtractedBehavior
{
    uint32 ClassHash = 0;
    std::optional<std::string> Name;
};

struct ExtractedTemplate
{
    uint32 TemplateId = 0;
    std::string ClassName;
    uint32 ClassHash = 0;
    std::string Archive;
    std::string Path;
    std::string ObjectName;
    std::string DisplayKey;
    std::string DescriptionKey;
    std::string Icon;
    std::string LootTable;
    std::optional<uint32> VisualId;
    std::optional<int64> ObjectType;
    std::vector<std::string> Adjectives;
    std::vector<ExtractedBehavior> Behaviors;
    std::optional<ItemTemplateRecord> Item;
};

struct TemplateExtraction
{
    static constexpr std::size_t MaxReportedProblems = 100;
    static constexpr std::string_view ObjectDataFolder = "ObjectData/";
    static constexpr std::string_view NpcBehavior = "NPCBehavior";

    std::vector<ExtractedTemplate> Templates;
    std::size_t ManifestEntries = 0;
    std::size_t ObjectDataEntries = 0;
    std::size_t ObjectDataRead = 0;
    std::map<uint32, std::size_t> UnknownBehaviorClasses;
    std::vector<std::string> Unread;
    std::size_t UnreadCount = 0;
    std::vector<std::string> Errors;
    std::size_t ErrorCount = 0;

    bool Ok() const noexcept { return ErrorCount == 0; }
    void AddError(std::string error);
    void AddUnread(std::string problem);
    std::size_t GetAdjectiveCount() const noexcept;
    std::size_t GetBehaviorCount() const noexcept;
    std::size_t GetUnknownBehaviorCount() const noexcept;
    std::size_t GetNpcTemplateCount() const noexcept;
    std::size_t GetItemCount() const noexcept;
    ExtractedTemplate const* Find(uint32 templateId) const noexcept;
};

class TemplateExtractor
{
public:
    static constexpr std::size_t MaxEntryBytes = 64 * 1024 * 1024;

    TemplateExtractor() = delete;

    static TemplateExtraction Extract(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog);
    static std::optional<TemplateExtraction> ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error,
        TypeDumpLoader::RawDump supplement = {});
    static bool ReadTemplate(TypeCatalogPtr const& catalog, uint32 templateId, TemplateLocation const& location, std::span<uint8 const> bytes, TemplateExtraction& extraction);
};

#endif
