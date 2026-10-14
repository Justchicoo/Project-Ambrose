/*
 * Project Ambrose by Imjustchico
 * Reads each QuestArcData file's QuestArc through the type registry, takes a quest's turn-in NPC from the object the persona goals its completing logic waits on name, read with or without the _Persona suffix, and gates and starts the later quest of each arc or wiki link only where the package and the wiki leave it open, reading copies of one NPC stacked on one spot as that NPC, keeping a starter the wiki's giver names, and never gating or starting a quest every school takes on a quest only one school takes.
 */

#include "QuestChain.h"
#include "KiwadArchive.h"
#include "PropertyObject.h"
#include "XmlObjectReader.h"

#include <fmt/format.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>

namespace
{
    std::string TextOf(PropertyValue const& value)
    {
        auto const* const text = value.GetIf<std::string>();
        return text ? *text : std::string();
    }
}

std::vector<std::string> QuestChain::GetArcEntries(KiwadArchive const& archive)
{
    std::vector<std::string> names;
    for (KiwadEntry const& entry : archive.GetEntries())
    {
        std::string const name = KiwadArchive::NormalizeSlashes(entry.Name);
        if (name.starts_with(ArcFolder) && name.ends_with(".xml"))
            names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

QuestArcRead QuestChain::ReadArcs(TypeCatalogPtr const& catalog, std::vector<QuestPackageEntry> const& entries)
{
    QuestArcRead read;
    ClassInfo const* const arcClass = catalog ? catalog->FindClass(ArcClass) : nullptr;
    for (QuestPackageEntry const& entry : entries)
    {
        ++read.Files;
        if (!arcClass)
        {
            ++read.Unread["the classes read list no QuestArc"];
            continue;
        }
        XmlReadResult const xml = XmlObjectReader::Read(catalog, entry.Text);
        if (!xml.Ok())
        {
            ++read.Unread[fmt::format("not readable XML objects: {}", XmlObjectReader::GetStatusName(xml.Status))];
            continue;
        }
        for (PropertyObjectPtr const& object : xml.Objects)
        {
            if (!object || !object->IsA(*arcClass))
            {
                ++read.Unread["an object that is no QuestArc"];
                continue;
            }
            QuestArc& arc = read.Arcs.emplace_back();
            if (PropertyValue const* const name = object->Get("m_arcName"))
                arc.Name = TextOf(*name);
            if (PropertyValue const* const stages = object->Get("m_stages"))
                if (PropertyValue::List const* const list = stages->GetList())
                    for (PropertyValue const& stage : *list)
                        if (std::string text = TextOf(stage); !text.empty())
                            arc.Stages.push_back(std::move(text));
        }
    }
    return read;
}

std::optional<uint32> QuestChain::TurnInActor(QuestPackageQuest const& quest, ObjectLookup const& objects)
{
    std::set<std::string, std::less<>> completing;
    for (QuestGoalLogicRow const& logic : quest.Rows.Logic)
        if (logic.CompleteQuest)
            completing.insert(logic.Goal);
    if (completing.empty() || !objects)
        return std::nullopt;
    std::set<uint32> actors;
    for (std::string const& goal : completing)
    {
        auto const row = std::find_if(quest.Rows.Goals.begin(), quest.Rows.Goals.end(), [&goal](QuestGoalRow const& candidate) { return candidate.Goal == goal; });
        if (row == quest.Rows.Goals.end() || row->Type != "persona")
            return std::nullopt;
        std::string_view persona = row->PersonaName;
        std::optional<uint32> actor = objects(persona);
        if (!actor && persona.ends_with(QuestPackage::PersonaSuffix))
            actor = objects(persona.substr(0, persona.size() - QuestPackage::PersonaSuffix.size()));
        if (!actor)
            return std::nullopt;
        actors.insert(*actor);
    }
    if (actors.size() != 1)
        return std::nullopt;
    return *actors.begin();
}

QuestChainMatch QuestChain::Apply(TypeCatalogPtr const& catalog, std::vector<QuestArc> const& arcs, std::vector<QuestLink> const& wikiLinks, QuestGiverChoices const& giverChoices,
    QuestPackageRead& read, ObjectLookup const& objects, QuestNpcStacks const* stacks)
{
    QuestChainMatch match;
    auto const stackOf = [stacks](uint32 templateId) { return stacks ? stacks->StackOf(templateId) : templateId; };
    std::unordered_map<std::string, QuestPackageQuest*> clean;
    for (QuestPackageQuest& quest : read.Quests)
        if (quest.IsClean() && !quest.Rows.Quests.empty())
            clean.emplace(quest.Name, &quest);

    std::unordered_map<std::string, std::set<std::string, std::less<>>> wikiPriors;
    for (auto const& [prior, quest] : wikiLinks)
        wikiPriors[quest].insert(prior);

    std::unordered_map<std::string, std::set<std::string, std::less<>>> priors = wikiPriors;
    for (QuestArc const& arc : arcs)
        for (std::size_t index = 1; index < arc.Stages.size(); ++index)
        {
            std::string const& prior = arc.Stages[index - 1];
            std::string const& next = arc.Stages[index];
            ++match.ArcLinks;
            auto const later = clean.find(next);
            if (!clean.contains(prior) || later == clean.end() || prior == next)
                continue;
            ++match.ArcLinksImported;
            if (clean.at(prior)->SchoolGated && !later->second->SchoolGated)
            {
                ++match.ArcLinksFromOneSchool;
                continue;
            }
            priors[next].insert(prior);
            auto const gated = wikiPriors.find(next);
            if (gated != wikiPriors.end() && gated->second.contains(prior))
                ++match.ArcLinksTheWikiAgrees;
            else if (gated != wikiPriors.end())
                ++match.ArcLinksTheWikiGatesOtherwise;
            else
            {
                std::vector<PropertyObjectPtr> gate;
                gate.push_back(QuestWikiData::CompletedEntry(catalog, prior));
                if (QuestWikiData::AddGate(catalog, later->second->Rows.Quests.front(), QuestWikiData::Requirements(catalog, 0, std::move(gate))))
                    ++match.GatesAdded;
                else
                    ++match.GatesNotEncoded;
            }
        }

    for (auto& [name, quest] : clean)
    {
        auto const linked = priors.find(name);
        if (linked == priors.end())
            continue;
        std::map<uint32, uint32> actors;
        for (std::string const& prior : linked->second)
            if (auto const found = clean.find(prior); found != clean.end() && (quest->SchoolGated || !found->second->SchoolGated))
                if (std::optional<uint32> const actor = TurnInActor(*found->second, objects))
                    actors.try_emplace(stackOf(*actor), *actor);
        if (actors.empty())
            continue;
        if (std::optional<uint32> const starter = QuestPackage::StarterOf(*quest))
        {
            if (actors.size() == 1)
            {
                ++match.StartersChecked;
                if (stackOf(*starter) == actors.begin()->first)
                    ++match.StartersAgreeing;
            }
        }
        else if (actors.size() == 1)
        {
            uint32 const actor = actors.begin()->second;
            std::optional<uint32> chosen = actor;
            if (auto const named = giverChoices.find(name); named != giverChoices.end())
            {
                std::vector<uint32> const& choices = named->second;
                chosen.reset();
                if (std::any_of(choices.begin(), choices.end(), [&](uint32 choice) { return stackOf(choice) == stackOf(actor); }))
                {
                    chosen = actor;
                    ++match.StartersFromGiverChoice;
                }
                else if (stacks)
                {
                    std::vector<uint32> nearby;
                    std::copy_if(choices.begin(), choices.end(), std::back_inserter(nearby), [&](uint32 choice) { return stacks->ShareZone(choice, actor); });
                    if (nearby.size() == 1)
                    {
                        chosen = nearby.front();
                        ++match.StartersFromGiverZone;
                    }
                }
                if (!chosen)
                    ++match.StartersLeftToGiver;
            }
            if (chosen)
            {
                quest->Rows.Quests.front().StarterTemplateId = *chosen;
                ++match.StartersFilled;
            }
        }
        else
            ++match.StartersConflicting;
    }
    for (QuestPackageQuest const& quest : read.Quests)
        if (quest.IsClean() && QuestPackage::HasStarter(quest))
            ++match.WithStarter;
    return match;
}
