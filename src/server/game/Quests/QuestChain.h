/*
 * Project Ambrose by Imjustchico
 * Reads the quest arcs a CSR package keeps beside its quests, each the ordered stages of one storyline, and chains the quests they and the wiki prequests link: a stage the wiki does not gate is gated on the stage before it, and a quest no Prep line or giver starts is started by the NPC its prior quest is turned in to, the one object every completing persona goal of that quest names, kept where the wiki giver names several templates only when it or a copy of it on the same spot is among them, else the one of them placed in its zone, counting how often the package's own starters already agree; a quest every school takes is never gated on or started from a quest only one school takes.
 */

#ifndef AMBROSE_QUESTCHAIN_H
#define AMBROSE_QUESTCHAIN_H

#include "QuestNpcStacks.h"
#include "QuestWikiData.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

struct QuestArc
{
    std::string Name;
    std::vector<std::string> Stages;
};

struct QuestArcRead
{
    std::vector<QuestArc> Arcs;
    std::size_t Files = 0;
    std::map<std::string, std::size_t, std::less<>> Unread;
};

struct QuestChainMatch
{
    std::size_t ArcLinks = 0;
    std::size_t ArcLinksImported = 0;
    std::size_t ArcLinksFromOneSchool = 0;
    std::size_t ArcLinksTheWikiAgrees = 0;
    std::size_t ArcLinksTheWikiGatesOtherwise = 0;
    std::size_t GatesAdded = 0;
    std::size_t GatesNotEncoded = 0;
    std::size_t StartersChecked = 0;
    std::size_t StartersAgreeing = 0;
    std::size_t StartersFilled = 0;
    std::size_t StartersConflicting = 0;
    std::size_t StartersFromGiverChoice = 0;
    std::size_t StartersFromGiverZone = 0;
    std::size_t StartersLeftToGiver = 0;
    std::size_t WithStarter = 0;
};

class QuestChain
{
public:
    using ObjectLookup = std::function<std::optional<uint32>(std::string_view objectName)>;

    static constexpr std::string_view ArcFolder = "QuestArcData/";
    static constexpr std::string_view ArcClass = "class QuestArc";

    QuestChain() = delete;

    static std::vector<std::string> GetArcEntries(KiwadArchive const& archive);
    static QuestArcRead ReadArcs(TypeCatalogPtr const& catalog, std::vector<QuestPackageEntry> const& entries);
    static std::optional<uint32> TurnInActor(QuestPackageQuest const& quest, ObjectLookup const& objects);
    static QuestChainMatch Apply(TypeCatalogPtr const& catalog, std::vector<QuestArc> const& arcs, std::vector<QuestLink> const& wikiLinks, QuestGiverChoices const& giverChoices,
        QuestPackageRead& read, ObjectLookup const& objects, QuestNpcStacks const* stacks = nullptr);
};

#endif
