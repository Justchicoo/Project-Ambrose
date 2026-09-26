/*
 * Project Ambrose by Imjustchico
 * Makes the player object from the catalog's own classes and defaults and sets only what the stored wizard decides: a behavior the template names that behavior_client_class does not know refuses the build rather than being guessed or dropped, because the client reads the behaviors by position and one missing slot shifts every later one, and a slot the template itself leaves empty stays empty; the school behavior and the stats come from the wizard's stats, with its level as the highest on the account, the spellbook holds a SpellIDTracker for each spell the wizard knows, which the client's spellbook reads when the object arrives, and every other field keeps the class's default.
 */

#include "PlayerObjectBuilder.h"
#include "AvatarAppearance.h"
#include "PropertyFiller.h"
#include "Utf.h"

#include <fmt/format.h>

#include <utility>

namespace
{
    PropertyObjectPtr Create(TypeCatalogPtr const& catalog, std::string_view className, std::string& problem)
    {
        PropertyObjectPtr object = PropertyObject::Create(catalog, className);
        if (!object && problem.empty())
            problem = fmt::format("the type dump has no property class {}", className);
        return object;
    }

    bool FillSpellbook(PropertyObject& behavior, std::vector<SpellTracker> const& spells, std::string& problem)
    {
        PropertyValue::List trackers;
        trackers.reserve(spells.size());
        for (SpellTracker const& spell : spells)
        {
            PropertyObjectPtr tracker = Create(behavior.GetCatalog(), "class SpellIDTracker", problem);
            if (!tracker)
                return false;
            PropertyFiller(*tracker, problem)
                .Set("m_spellID", spell.SpellId)
                .Set("m_isRetired", spell.Retired)
                .Set("m_tieredSpellGroupIndex", spell.TieredGroupIndex);
            trackers.emplace_back(std::move(tracker));
        }
        PropertyFiller(behavior, problem).Set("m_spellIDList", std::move(trackers));
        return problem.empty();
    }

    bool FillBehavior(PropertyObject& behavior, CharacterSummary const& character, PlayerStats const& stats, std::vector<SpellTracker> const& spells, std::string& problem)
    {
        std::string_view const name = behavior.GetClass().Name;
        if (name == "class WizardCharacterBehavior")
            return AvatarAppearance::Write(behavior, character.Appearance, problem);
        if (name == "class ClientWizPlayerNameBehavior")
        {
            std::u16string custom;
            if (character.CustomName)
            {
                std::optional<std::u16string> converted = Utf::Utf8ToUtf16(*character.CustomName, Utf::InvalidPolicy::Reject);
                if (!converted)
                {
                    problem = fmt::format("character {} has a custom name that is not UTF-8", character.Guid);
                    return false;
                }
                custom = std::move(*converted);
            }
            PropertyFiller(behavior, problem)
                .Set("m_wsNameOverride", std::move(custom))
                .Set("m_nameKeys", character.NameIndices)
                .Set("m_eGender", int64{ character.Appearance.Gender })
                .Set("m_eRace", int64{ character.Appearance.Race });
            return problem.empty();
        }
        if (name == "class ClientMagicSchoolBehavior")
            return stats.WriteSchool(behavior, problem);
        if (name == "class ClientSpellbookBehavior")
            return FillSpellbook(behavior, spells, problem);
        return true;
    }
}

PropertyObjectPtr PlayerObjectBuilder::Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, BehaviorClientClasses const& behaviors, ObjectTemplate const& playerTemplate,
    CharacterSummary const& character, PlayerStats const& stats, std::vector<SpellTracker> const& spells, PlayerPlacement const& placement, std::string& problem)
{
    problem.clear();
    if (!catalog)
    {
        problem = "no type dump is loaded";
        return nullptr;
    }
    if (!playerTemplate.IsLoaded())
    {
        problem = "the player's template has not been read from the install";
        return nullptr;
    }
    PropertyObjectPtr player = Create(catalog, PlayerClass, problem);
    PropertyObjectPtr gameStats = Create(catalog, StatsClass, problem);
    if (!player || !gameStats)
        return nullptr;
    CoreObjectType const* const pair = types.FindByClass(player->GetClass().Hash);
    if (!pair)
    {
        problem = fmt::format("core_object_type gives {} no block and type, so the client could not create it", PlayerClass);
        return nullptr;
    }
    player->SetCoreHeader(CoreObjectHeader{ pair->Block, pair->Type, playerTemplate.TemplateId });

    PropertyValue::List inactive;
    inactive.reserve(playerTemplate.Behaviors.size());
    for (std::string const& behaviorName : playerTemplate.Behaviors)
    {
        if (behaviorName.empty())
        {
            inactive.emplace_back(PropertyObjectPtr());
            continue;
        }
        BehaviorClientClass const* const row = behaviors.Find(behaviorName);
        if (!row)
        {
            problem = fmt::format("template {} names behavior {}, which behavior_client_class does not list, so the class the client builds for it is not known",
                playerTemplate.TemplateId, behaviorName);
            return nullptr;
        }
        if (!row->ClassName)
        {
            inactive.emplace_back(PropertyObjectPtr());
            continue;
        }
        PropertyObjectPtr behavior = Create(catalog, *row->ClassName, problem);
        if (!behavior || !FillBehavior(*behavior, character, stats, spells, problem))
            return nullptr;
        inactive.emplace_back(std::move(behavior));
    }

    if (!stats.WriteGameStats(*gameStats, problem))
        return nullptr;
    PropertyFiller(*gameStats, problem).Set("m_highestCharacterLevelOnAccount", character.Level);
    PropertyFiller(*player, problem)
        .Set("m_inactiveBehaviors", std::move(inactive))
        .Set("m_globalID.m_full", character.Guid)
        .Set("m_permID", uint64{ 0 })
        .Set("m_location", PropertyTypes::Vector3D{ placement.X, placement.Y, placement.Z })
        .Set("m_orientation", PropertyTypes::Vector3D{ 0.0f, 0.0f, placement.Yaw })
        .Set("m_fScale", 1.0f)
        .Set("m_templateID.m_full", uint64{ playerTemplate.TemplateId })
        .Set("m_nMobileID", placement.MobileId)
        .Set("m_characterId", character.Guid)
        .Set("m_gameStats", std::move(gameStats));
    if (!problem.empty())
        return nullptr;
    return player;
}
