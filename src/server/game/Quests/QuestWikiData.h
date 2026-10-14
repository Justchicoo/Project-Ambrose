/*
 * Project Ambrose by Imjustchico
 * Reads a wiki quest list a user keeps, one JSON quest per line naming its giver, prequests, gold, experience and item rewards by English name, and matches it to the quests a CSR package read: a wiki quest to the quests whose title text it is, several of them only when each is gated on a school, so school variants of one step stay together; its prequests become a stored requirement that each was completed, an OR over the variants of one step and over prequests that are all school variants, an AND otherwise, joined under an AND with any requirement the package gave; its giver to the one template the client catalog names so, among those the world holds when it is given them, written as the quest's starter template; and its reward items to the one template named so, kept for the tables that will hold them.
 */

#ifndef AMBROSE_QUESTWIKIDATA_H
#define AMBROSE_QUESTWIKIDATA_H

#include "PropertyObject.h"
#include "QuestNpcStacks.h"
#include "QuestPackage.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class ClientCatalog;

struct WikiReward
{
    std::string Type;
    std::string Item;
    std::map<std::string, std::string, std::less<>> PerSchool;
};

struct WikiQuest
{
    std::string Name;
    std::string Giver;
    std::vector<std::string> Prequests;
    std::optional<uint32> Gold;
    std::optional<uint32> Experience;
    std::vector<WikiReward> Rewards;
};

struct WikiQuestRead
{
    std::vector<WikiQuest> Quests;
    std::size_t Lines = 0;
    std::vector<std::string> Problems;
};

struct QuestWikiReward
{
    std::string Quest;
    uint32 Gold = 0;
    uint32 Experience = 0;
};

struct QuestWikiRewardItem
{
    std::string Quest;
    uint32 Position = 0;
    std::string School;
    std::string Type;
    uint32 TemplateId = 0;
    uint32 Count = 1;
};

using QuestLink = std::pair<std::string, std::string>;
using QuestGiverChoices = std::map<std::string, std::vector<uint32>, std::less<>>;

struct QuestWikiMatch
{
    std::size_t Matched = 0;
    std::size_t MatchedVariants = 0;
    std::size_t Unmatched = 0;
    std::size_t Ambiguous = 0;
    std::size_t Contested = 0;
    std::size_t PrequestsResolved = 0;
    std::size_t PrequestsUnresolved = 0;
    std::size_t Gated = 0;
    std::size_t GatedWithPackageRequirements = 0;
    std::size_t GatesNotEncoded = 0;
    std::size_t PrequestsOfOneSchool = 0;
    std::size_t GiversResolved = 0;
    std::size_t GiversUnknown = 0;
    std::size_t GiversAmbiguous = 0;
    std::size_t GiversStacked = 0;
    std::size_t RewardItemsResolved = 0;
    std::size_t RewardItemsUnknown = 0;
    std::size_t RewardItemsAmbiguous = 0;
    std::size_t StartersWritten = 0;
    std::size_t WithStarter = 0;
    std::vector<QuestWikiReward> Rewards;
    std::vector<QuestWikiRewardItem> RewardItems;
    std::vector<QuestLink> Links;
    QuestGiverChoices GiverChoices;
};

class QuestWikiData
{
public:
    using TextLookup = std::function<std::optional<std::string>(std::string_view key)>;
    using TemplateLookup = std::function<bool(uint32 templateId)>;

    static constexpr std::string_view CompleteEntry = "Complete";
    static constexpr std::string_view NpcCategory = "NPCs";
    static constexpr std::string_view DefaultFile = "quests.ndjson";

    QuestWikiData() = delete;

    static WikiQuestRead Parse(std::string_view text);
    static std::string Normalize(std::string_view name);
    static std::vector<std::string_view> CategoriesOf(std::string_view rewardType);
    static PropertyObjectPtr CompletedEntry(TypeCatalogPtr const& catalog, std::string const& quest);
    static PropertyObjectPtr Requirements(TypeCatalogPtr const& catalog, int64 op, std::vector<PropertyObjectPtr> children);
    static bool AddGate(TypeCatalogPtr const& catalog, QuestTemplateRow& row, PropertyObjectPtr gate);
    static QuestWikiMatch Apply(TypeCatalogPtr const& catalog, std::vector<WikiQuest> const& wiki, QuestPackageRead& read, TextLookup const& text, ClientCatalog const& templates,
        TemplateLookup const& inWorld = {}, QuestNpcStacks const* stacks = nullptr);
};

#endif
