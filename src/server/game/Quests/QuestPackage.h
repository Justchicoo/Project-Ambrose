/*
 * Project Ambrose by Imjustchico
 * Reads the quest templates of a CSR package a user kept from an older client, its QuestData plain-XML files, into the rows of the world database's quest tables, one set per quest with its RequirementList encoded into quest_template.requirements, checks each against the install's templates, object names and locale keys and the quest validator, counts what the tables have no place for, the requirement and unknown classes and the locale keys the quests name, and writes the quests that read cleanly as a script that replaces only the rows the package's source marks.
 */

#ifndef AMBROSE_QUESTPACKAGE_H
#define AMBROSE_QUESTPACKAGE_H

#include "QuestRows.h"
#include "QuestValidator.h"
#include "TypeRegistry.h"
#include "WorldSqlScript.h"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

struct QuestPackageEntry
{
    std::string Name;
    std::string Text;
};

struct QuestPackageQuest
{
    std::string Name;
    std::string Entry;
    QuestRows Rows;
    std::string Category;
    std::vector<std::string> Problems;
    std::map<std::string, std::size_t, std::less<>> SkippedResults;
    bool SchoolGated = false;

    bool IsClean() const noexcept { return Category.empty(); }
};

struct QuestFolderCounts
{
    std::size_t Read = 0;
    std::size_t Clean = 0;
    std::size_t Started = 0;
};

struct QuestPackageRead
{
    using Counts = std::map<std::string, std::size_t, std::less<>>;

    std::vector<QuestPackageQuest> Quests;
    std::size_t Files = 0;
    Counts Unread;
    Counts RootClasses;
    Counts RequirementClasses;
    Counts UnknownClasses;
    Counts NotCarried;
    Counts Adjusted;
    std::set<std::string, std::less<>> Keys;

    std::size_t GetCleanCount() const noexcept;
    Counts GetCategories() const;
    Counts GetSkippedResults() const;
};

class QuestPackage
{
public:
    static constexpr std::string_view QuestFolder = "QuestData/";
    static constexpr std::string_view SourcePrefix = "package:";
    static constexpr std::string_view PersonaSuffix = "_Persona";
    static constexpr std::string_view DefaultPackage = "CSR.wad";

    QuestPackage() = delete;

    static std::vector<std::string> GetQuestEntries(KiwadArchive const& archive);
    static QuestPackageRead Read(TypeCatalogPtr const& catalog, std::vector<QuestPackageEntry> const& entries);
    static void Check(QuestPackageRead& read, QuestValidator::Context const& context, std::set<std::string, std::less<>> const& heldElsewhere = {});
    static std::optional<uint32> StarterOf(QuestPackageQuest const& quest);
    static bool HasStarter(QuestPackageQuest const& quest);
    static std::map<std::string, QuestFolderCounts, std::less<>> CountByFolder(QuestPackageRead const& read);
    static std::size_t CountMissingKeys(QuestPackageRead const& read, QuestValidator::KeyLookup const& hasKey);
    static std::string SourceOf(std::string_view packageName);
    static WorldSqlScript BuildScript(QuestPackageRead const& read, std::string const& source);
    static std::vector<std::string_view> GetTables();
};

#endif
