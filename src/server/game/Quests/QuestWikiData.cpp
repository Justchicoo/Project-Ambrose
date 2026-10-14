/*
 * Project Ambrose by Imjustchico
 * Parses the wiki quest lines by key, reads gold and experience written with thousands separators, matches names after folding case, quotes, markup and spacing, and builds each gate as ReqHasEntry objects on the quest registry's Complete entry, encoded the way zone data holds requirements; a quest more than one wiki line claims is left alone, and a school OR missing one of its variants is not written, since it would shut out the wizards of that school.
 */

#include "QuestWikiData.h"
#include "ClientCatalog.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "QuestStore.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_map>
#include <utility>

namespace
{
    using Json = nlohmann::json;

    std::string TextAt(Json const& object, char const* key)
    {
        auto const found = object.find(key);
        return found != object.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    std::optional<uint32> NumberOf(Json const& object, char const* key)
    {
        std::string digits;
        for (char const c : TextAt(object, key))
        {
            if (std::isdigit(static_cast<unsigned char>(c)))
                digits += c;
            else if (c != ',')
                return std::nullopt;
        }
        if (digits.empty() || digits.size() > 9)
            return std::nullopt;
        return static_cast<uint32>(std::stoul(digits));
    }

    enum class Found : uint8
    {
        None,
        One,
        Variants,
        Ambiguous
    };

    struct Resolved
    {
        Found Kind = Found::None;
        std::vector<QuestPackageQuest*> Quests;
    };

    struct NameIndex
    {
        std::unordered_map<std::string, std::vector<uint32>> Ids;

        void Add(std::string_view display, uint32 id)
        {
            if (!display.empty())
                Ids[QuestWikiData::Normalize(display)].push_back(id);
        }

        std::vector<uint32> const* Find(std::string_view name) const
        {
            auto const found = Ids.find(QuestWikiData::Normalize(name));
            return found == Ids.end() ? nullptr : &found->second;
        }
    };
}

PropertyObjectPtr QuestWikiData::CompletedEntry(TypeCatalogPtr const& catalog, std::string const& quest)
{
    PropertyObjectPtr entry = PropertyObject::Create(catalog, "class ReqHasEntry");
    if (!entry || entry->Set("m_isQuestRegistry", true) != PropertySetResult::Ok || entry->Set("m_questName", std::string(quest)) != PropertySetResult::Ok
        || entry->Set("m_entryName", std::string(CompleteEntry)) != PropertySetResult::Ok)
        return nullptr;
    return entry;
}

PropertyObjectPtr QuestWikiData::Requirements(TypeCatalogPtr const& catalog, int64 op, std::vector<PropertyObjectPtr> children)
{
    PropertyObjectPtr list = PropertyObject::Create(catalog, "class RequirementList");
    if (!list || list->Set("m_operator", int64{ op }) != PropertySetResult::Ok)
        return nullptr;
    PropertyValue::List values;
    for (PropertyObjectPtr& child : children)
    {
        if (!child)
            return nullptr;
        values.emplace_back(std::move(child));
    }
    if (list->Set("m_requirements", std::move(values)) != PropertySetResult::Ok)
        return nullptr;
    return list;
}

bool QuestWikiData::AddGate(TypeCatalogPtr const& catalog, QuestTemplateRow& row, PropertyObjectPtr gate)
{
    if (gate && !row.Requirements.empty())
    {
        DecodeResult decoded = ObjectSerializer::Decode(catalog, row.Requirements, ZoneExtractor::DataOptions());
        std::vector<PropertyObjectPtr> both;
        both.push_back(decoded.Ok() ? std::move(decoded.Object) : nullptr);
        both.push_back(std::move(gate));
        gate = Requirements(catalog, 0, std::move(both));
    }
    EncodeResult encoded = gate ? ObjectSerializer::Encode(gate.get(), ZoneExtractor::DataOptions()) : EncodeResult{};
    if (!gate || !encoded.Ok())
        return false;
    row.Requirements = std::move(encoded.Bytes);
    return true;
}

WikiQuestRead QuestWikiData::Parse(std::string_view text)
{
    WikiQuestRead read;
    while (!text.empty())
    {
        std::size_t const end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.remove_suffix(1);
        if (line.empty())
            continue;
        ++read.Lines;
        Json const object = Json::parse(line, nullptr, false);
        if (object.is_discarded() || !object.is_object())
        {
            read.Problems.push_back(fmt::format("line {} is not a JSON object", read.Lines));
            continue;
        }
        WikiQuest quest;
        quest.Name = TextAt(object, "name");
        if (quest.Name.empty())
        {
            read.Problems.push_back(fmt::format("line {} names no quest", read.Lines));
            continue;
        }
        quest.Giver = TextAt(object, "giver");
        if (auto const prequests = object.find("prequests"); prequests != object.end() && prequests->is_array())
            for (Json const& prequest : *prequests)
                if (prequest.is_string() && !prequest.get<std::string>().empty())
                    quest.Prequests.push_back(prequest.get<std::string>());
        quest.Gold = NumberOf(object, "rewgold");
        quest.Experience = NumberOf(object, "rewxp");
        if (auto const rewards = object.find("rewards"); rewards != object.end() && rewards->is_array())
            for (Json const& entry : *rewards)
            {
                if (!entry.is_object())
                    continue;
                WikiReward reward;
                reward.Type = TextAt(entry, "type");
                reward.Item = TextAt(entry, "value");
                if (auto const schools = entry.find("per_school"); schools != entry.end() && schools->is_object())
                    for (auto const& [school, item] : schools->items())
                        if (item.is_string() && !item.get<std::string>().empty())
                            reward.PerSchool[school] = item.get<std::string>();
                if (!reward.Item.empty() || !reward.PerSchool.empty())
                    quest.Rewards.push_back(std::move(reward));
            }
        read.Quests.push_back(std::move(quest));
    }
    return read;
}

std::string QuestWikiData::Normalize(std::string_view name)
{
    std::string folded;
    bool space = false;
    bool tag = false;
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        unsigned char c = static_cast<unsigned char>(name[index]);
        if (c == '<')
        {
            tag = true;
            continue;
        }
        if (tag)
        {
            tag = c != '>';
            continue;
        }
        if (c == 0xE2 && index + 2 < name.size() && static_cast<unsigned char>(name[index + 1]) == 0x80)
        {
            unsigned char const third = static_cast<unsigned char>(name[index + 2]);
            if (third == 0x98 || third == 0x99)
                c = '\'';
            else if (third == 0x9C || third == 0x9D)
                c = '"';
            else if (third == 0x93 || third == 0x94)
                c = '-';
            else if (third == 0xA6)
            {
                folded += space && !folded.empty() ? " ..." : "...";
                space = false;
                index += 2;
                continue;
            }
            else
                c = 0;
            if (c != 0)
                index += 2;
            else
                c = 0xE2;
        }
        if (std::isspace(c))
        {
            space = true;
            continue;
        }
        if (space && !folded.empty())
            folded += ' ';
        space = false;
        folded += static_cast<char>(c < 0x80 ? std::tolower(c) : c);
    }
    return folded;
}

std::vector<std::string_view> QuestWikiData::CategoriesOf(std::string_view rewardType)
{
    static std::map<std::string_view, std::vector<std::string_view>, std::less<>> const categories{
        { "Spellement", { "Spells" } }, { "Spell", { "Spells" } }, { "Treasure Card", { "Spells", "Items" } }, { "Furniture", { "Housing" } }, { "Decoration", { "Housing" } },
        { "Castle", { "Housing" } }, { "Seed", { "Housing", "Items" } }, { "Reagent", { "Reagents" } }, { "Snack", { "PetSnacks" } }, { "Pet", { "Pets", "Items" } },
        { "Jewel", { "Jewels", "Items" } }, { "Hat", { "Equipment", "Items" } }, { "Robe", { "Equipment", "Items" } }, { "Boots", { "Equipment", "Items" } },
        { "Wand", { "Equipment", "Items" } }, { "Athame", { "Equipment", "Items" } }, { "Amulet", { "Equipment", "Items" } }, { "Ring", { "Equipment", "Items" } },
        { "Deck", { "Decks", "Items" } }, { "Elixir", { "Elixirs", "Items" } }, { "Mount", { "Mounts", "Items" } } };
    auto const found = categories.find(rewardType);
    return found == categories.end() ? std::vector<std::string_view>{} : found->second;
}

QuestWikiMatch QuestWikiData::Apply(TypeCatalogPtr const& catalog, std::vector<WikiQuest> const& wiki, QuestPackageRead& read, TextLookup const& text, ClientCatalog const& templates, TemplateLookup const& inWorld, QuestNpcStacks const* stacks)
{
    QuestWikiMatch match;
    std::unordered_map<std::string, std::vector<QuestPackageQuest*>> byTitle;
    for (QuestPackageQuest& quest : read.Quests)
        if (quest.IsClean() && !quest.Rows.Quests.empty() && text)
            if (std::optional<std::string> const title = text(quest.Rows.Quests.front().TitleKey); title && !title->empty())
                byTitle[Normalize(*title)].push_back(&quest);

    auto const resolve = [&byTitle](std::string_view name)
    {
        Resolved resolved;
        auto const found = byTitle.find(Normalize(name));
        if (found == byTitle.end())
            return resolved;
        resolved.Quests = found->second;
        if (resolved.Quests.size() == 1)
            resolved.Kind = Found::One;
        else if (std::all_of(resolved.Quests.begin(), resolved.Quests.end(), [](QuestPackageQuest const* quest) { return quest->SchoolGated; }))
            resolved.Kind = Found::Variants;
        else
            resolved.Kind = Found::Ambiguous;
        return resolved;
    };

    NameIndex npcs;
    std::map<std::string, NameIndex, std::less<>> items;
    std::set<uint32> placed;
    for (CatalogZone const& zone : templates.GetZones())
        placed.insert(zone.Templates.begin(), zone.Templates.end());
    for (CatalogCategory const& category : templates.GetCategories())
        for (CatalogTemplate const& entry : category.Templates)
        {
            if (category.Name == NpcCategory)
                npcs.Add(entry.DisplayName, entry.TemplateId);
            else
                items[category.Name].Add(entry.DisplayName, entry.TemplateId);
        }

    std::vector<Resolved> targets;
    targets.reserve(wiki.size());
    std::unordered_map<std::string, std::size_t> claims;
    for (WikiQuest const& quest : wiki)
    {
        Resolved& target = targets.emplace_back(resolve(quest.Name));
        if (target.Kind == Found::None)
            ++match.Unmatched;
        else if (target.Kind == Found::Ambiguous)
            ++match.Ambiguous;
        else
        {
            ++(target.Kind == Found::One ? match.Matched : match.MatchedVariants);
            for (QuestPackageQuest const* claimed : target.Quests)
                ++claims[claimed->Name];
        }
    }

    for (std::size_t index = 0; index < wiki.size(); ++index)
    {
        WikiQuest const& quest = wiki[index];
        Resolved const& target = targets[index];
        if (target.Kind != Found::One && target.Kind != Found::Variants)
            continue;

        std::vector<Resolved> steps;
        for (std::string const& prequest : quest.Prequests)
        {
            Resolved step = resolve(prequest);
            if (step.Kind == Found::One || step.Kind == Found::Variants)
            {
                ++match.PrequestsResolved;
                steps.push_back(std::move(step));
            }
            else
            {
                ++match.PrequestsUnresolved;
                steps.emplace_back();
            }
        }
        auto const bySchool = [](Resolved const& step)
        {
            return std::all_of(step.Quests.begin(), step.Quests.end(), [](QuestPackageQuest const* prior) { return prior->SchoolGated; });
        };
        bool const allBySchool = steps.size() > 1 && std::all_of(steps.begin(), steps.end(), bySchool);
        bool const missingChoice = allBySchool && std::any_of(steps.begin(), steps.end(), [](Resolved const& step) { return step.Kind == Found::None; });
        bool const schoolChoice = allBySchool && !missingChoice;
        if (missingChoice)
            steps.clear();

        std::optional<uint32> giver;
        std::vector<uint32> choices;
        if (!quest.Giver.empty())
        {
            std::vector<uint32> const* const ids = npcs.Find(quest.Giver);
            std::vector<uint32> candidates = ids ? *ids : std::vector<uint32>{};
            if (inWorld)
                std::erase_if(candidates, [&inWorld](uint32 id) { return !inWorld(id); });
            if (candidates.size() > 1)
            {
                std::vector<uint32> inZones = candidates;
                std::erase_if(inZones, [&placed](uint32 id) { return !placed.contains(id); });
                if (!inZones.empty())
                    candidates = std::move(inZones);
            }
            if (candidates.empty())
                ++match.GiversUnknown;
            else if (candidates.size() == 1)
            {
                ++match.GiversResolved;
                giver = candidates.front();
            }
            else if (stacks && stacks->OneOf(candidates))
            {
                ++match.GiversStacked;
                giver = *std::min_element(candidates.begin(), candidates.end());
            }
            else
            {
                ++match.GiversAmbiguous;
                choices = std::move(candidates);
            }
        }

        for (QuestPackageQuest* claimed : target.Quests)
        {
            if (claims[claimed->Name] != 1)
            {
                ++match.Contested;
                continue;
            }
            std::string const& name = claimed->Name;
            for (Resolved const& step : steps)
                for (QuestPackageQuest const* prior : step.Quests)
                    if (prior->Name != name && (schoolChoice || claimed->SchoolGated || !prior->SchoolGated))
                        match.Links.emplace_back(prior->Name, name);
            std::vector<PropertyObjectPtr> gates;
            if (schoolChoice)
            {
                std::vector<PropertyObjectPtr> anyOf;
                for (Resolved const& step : steps)
                    for (QuestPackageQuest const* prior : step.Quests)
                        if (prior->Name != name)
                            anyOf.push_back(CompletedEntry(catalog, prior->Name));
                if (!anyOf.empty())
                    gates.push_back(Requirements(catalog, 1, std::move(anyOf)));
            }
            else
                for (Resolved const& step : steps)
                {
                    if (step.Kind == Found::One && step.Quests.front()->SchoolGated && !claimed->SchoolGated)
                        ++match.PrequestsOfOneSchool;
                    else if (step.Kind == Found::One && step.Quests.front()->Name != name)
                        gates.push_back(CompletedEntry(catalog, step.Quests.front()->Name));
                    else if (step.Kind == Found::Variants)
                    {
                        std::vector<PropertyObjectPtr> anyOf;
                        for (QuestPackageQuest const* prior : step.Quests)
                            if (prior->Name != name)
                                anyOf.push_back(CompletedEntry(catalog, prior->Name));
                        if (anyOf.size() == step.Quests.size())
                            gates.push_back(Requirements(catalog, 1, std::move(anyOf)));
                    }
                }

            QuestTemplateRow& row = claimed->Rows.Quests.front();
            if (!gates.empty())
            {
                bool const hadRequirements = !row.Requirements.empty();
                if (AddGate(catalog, row, Requirements(catalog, 0, std::move(gates))))
                {
                    ++match.Gated;
                    if (hadRequirements)
                        ++match.GatedWithPackageRequirements;
                }
                else
                    ++match.GatesNotEncoded;
            }

            if (giver)
            {
                row.StarterTemplateId = *giver;
                ++match.StartersWritten;
            }
            else if (!choices.empty())
                match.GiverChoices[name] = choices;
            if (quest.Gold || quest.Experience)
                match.Rewards.push_back(QuestWikiReward{ name, quest.Gold.value_or(0), quest.Experience.value_or(0) });
            uint32 position = 0;
            for (WikiReward const& reward : quest.Rewards)
            {
                std::vector<std::pair<std::string, std::string>> named;
                if (!reward.Item.empty())
                    named.emplace_back(std::string(), reward.Item);
                for (auto const& [school, item] : reward.PerSchool)
                    named.emplace_back(school, item);
                for (auto const& [school, item] : named)
                {
                    std::set<uint32> ids;
                    for (std::string_view const category : CategoriesOf(reward.Type))
                        if (auto const byName = items.find(category); byName != items.end())
                            if (std::vector<uint32> const* const found = byName->second.Find(item))
                                ids.insert(found->begin(), found->end());
                    if (ids.empty())
                        ++match.RewardItemsUnknown;
                    else if (ids.size() > 1)
                        ++match.RewardItemsAmbiguous;
                    else
                    {
                        ++match.RewardItemsResolved;
                        match.RewardItems.push_back(QuestWikiRewardItem{ name, position, school, reward.Type, *ids.begin(), 1 });
                    }
                }
                ++position;
            }
        }
    }
    for (QuestPackageQuest const& quest : read.Quests)
        if (quest.IsClean() && QuestPackage::HasStarter(quest))
            ++match.WithStarter;
    return match;
}
