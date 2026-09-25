/*
 * Project Ambrose by Imjustchico
 * Makes the player object from the catalog's own classes and defaults and sets only what the stored wizard decides: a behavior the template names that behavior_client_class does not know refuses the build rather than being guessed or dropped, because the client reads the behaviors by position and one missing slot shifts every later one, and a slot the template itself leaves empty stays empty; the stats carry the wizard's level as the highest on the account and otherwise the class's defaults, since the level tables vitals come from arrive with phase 8.
 */

#include "PlayerObjectBuilder.h"
#include "AvatarAppearance.h"
#include "Utf.h"

#include <fmt/format.h>

#include <utility>

namespace
{
    class Filler
    {
    public:
        Filler(PropertyObject& object, std::string& problem) : _object(object), _problem(problem)
        {
        }

        Filler& Set(std::string_view name, PropertyValue&& value)
        {
            if (!_problem.empty())
                return *this;
            PropertySetResult const result = _object.Set(name, std::move(value));
            if (result != PropertySetResult::Ok)
                _problem = fmt::format("{} property {} refused its value: {}", _object.GetClass().Name, name, PropertyObject::GetResultName(result));
            return *this;
        }

    private:
        PropertyObject& _object;
        std::string& _problem;
    };

    PropertyObjectPtr Create(TypeCatalogPtr const& catalog, std::string_view className, std::string& problem)
    {
        PropertyObjectPtr object = PropertyObject::Create(catalog, className);
        if (!object && problem.empty())
            problem = fmt::format("the type dump has no property class {}", className);
        return object;
    }

    bool FillBehavior(PropertyObject& behavior, CharacterSummary const& character, std::string& problem)
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
            Filler(behavior, problem)
                .Set("m_wsNameOverride", std::move(custom))
                .Set("m_nameKeys", character.NameIndices)
                .Set("m_eGender", int64{ character.Appearance.Gender })
                .Set("m_eRace", int64{ character.Appearance.Race });
            return problem.empty();
        }
        if (name == "class ClientMagicSchoolBehavior")
        {
            Filler(behavior, problem)
                .Set("m_schoolOfFocus", character.SchoolId)
                .Set("m_level", character.Level)
                .Set("m_experiencePoints", character.Experience);
            return problem.empty();
        }
        return true;
    }
}

PropertyObjectPtr PlayerObjectBuilder::Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, BehaviorClientClasses const& behaviors, ObjectTemplate const& playerTemplate,
    CharacterSummary const& character, PlayerPlacement const& placement, std::string& problem)
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
    PropertyObjectPtr stats = Create(catalog, StatsClass, problem);
    if (!player || !stats)
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
        if (!behavior || !FillBehavior(*behavior, character, problem))
            return nullptr;
        inactive.emplace_back(std::move(behavior));
    }

    Filler(*stats, problem).Set("m_highestCharacterLevelOnAccount", character.Level);
    Filler(*player, problem)
        .Set("m_inactiveBehaviors", std::move(inactive))
        .Set("m_globalID.m_full", character.Guid)
        .Set("m_permID", uint64{ 0 })
        .Set("m_location", PropertyTypes::Vector3D{ placement.X, placement.Y, placement.Z })
        .Set("m_orientation", PropertyTypes::Vector3D{ 0.0f, 0.0f, placement.Yaw })
        .Set("m_fScale", 1.0f)
        .Set("m_templateID.m_full", uint64{ playerTemplate.TemplateId })
        .Set("m_nMobileID", placement.MobileId)
        .Set("m_characterId", character.Guid)
        .Set("m_gameStats", std::move(stats));
    if (!problem.empty())
        return nullptr;
    return player;
}
