/*
 * Project Ambrose by Imjustchico
 * Makes the player object from the catalog's own classes and defaults and sets only what the stored wizard decides: a behavior the template names that behavior_client_class does not know refuses the build rather than being guessed or dropped, because the client reads the behaviors by position and one missing slot shifts every later one, and a slot the template itself leaves empty stays empty; the name behavior's m_chatPermissions is the account's permissions, the same the session hands the client in MSG_LOGINCOMPLETE, because the client draws no chat mark beside a name whose low four bits are all set, the filtered balloon beside one that holds menu chat and its display without all four, and the word balloon beside any other, so leaving it at 0 drew the word balloon beside every wizard; the school behavior and stats come from the wizard's stats, the spellbook holds a SpellIDTracker for each spell the wizard knows, and the radial-emote behavior holds the owned templates in its versioned first page.
 */

#include "PlayerObjectBuilder.h"
#include "AvatarAppearance.h"
#include "PropertyFiller.h"
#include "Utf.h"

#include <fmt/format.h>

#include <limits>
#include <optional>
#include <type_traits>
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

    template<typename T>
    void AppendLittleEndian(std::string& bytes, T value)
    {
        static_assert(std::is_unsigned_v<T>);
        for (std::size_t index = 0; index < sizeof(T); ++index)
            bytes.push_back(static_cast<char>((value >> (index * 8)) & 0xFF));
    }

    std::optional<std::string> BuildEmotePage(std::vector<uint32> const& templateIds, std::string& problem)
    {
        if (templateIds.size() > std::numeric_limits<uint16>::max())
        {
            problem = "the wizard owns more custom-emote templates than one radial page can encode";
            return std::nullopt;
        }
        std::string bytes;
        if (templateIds.empty())
            return bytes;
        bytes.reserve(10 + templateIds.size() * 14);
        AppendLittleEndian(bytes, uint32{ 0xFACEACE8 });
        AppendLittleEndian(bytes, uint32{ 1 });
        AppendLittleEndian(bytes, static_cast<uint16>(templateIds.size()));
        for (std::size_t index = 0; index < templateIds.size(); ++index)
        {
            if (templateIds[index] == 0)
            {
                problem = "a custom-emote template has id zero and cannot be placed in the radial menu";
                return std::nullopt;
            }
            AppendLittleEndian(bytes, static_cast<uint16>(index));
            AppendLittleEndian(bytes, static_cast<uint64>(index + 1));
            AppendLittleEndian(bytes, templateIds[index]);
        }
        return bytes;
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

    bool FillBehavior(PropertyObject& behavior, CharacterSummary const& character, PlayerStats const& stats, std::vector<SpellTracker> const& spells,
        std::vector<uint32> const& emoteTemplateIds, uint32 permissions, std::string& problem)
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
                .Set("m_eRace", int64{ character.Appearance.Race })
                .Set("m_chatPermissions", permissions);
            return problem.empty();
        }
        if (name == "class ClientMagicSchoolBehavior")
            return stats.WriteSchool(behavior, problem);
        if (name == "class ClientSpellbookBehavior")
            return FillSpellbook(behavior, spells, problem);
        if (name == "class EmotesRadialMenuBehavior")
        {
            std::optional<std::string> page = BuildEmotePage(emoteTemplateIds, problem);
            if (!page)
                return false;
            PropertyFiller(behavior, problem).Set("m_radialMenuData", std::move(*page));
            return problem.empty();
        }
        return true;
    }
}

PropertyObjectPtr PlayerObjectBuilder::Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, BehaviorClientClasses const& behaviors, ObjectTemplate const& playerTemplate,
    CharacterSummary const& character, PlayerStats const& stats, std::vector<SpellTracker> const& spells, std::vector<uint32> const& emoteTemplateIds, PlayerPlacement const& placement,
    uint32 permissions, std::string& problem)
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
    std::optional<CoreObjectHeader> const header = playerTemplate.Object ? types.HeaderFor(playerTemplate.Object->GetClass(), playerTemplate.TemplateId) : std::nullopt;
    if (!header)
    {
        problem = fmt::format("core_template_type gives the player's template class {} no core type, so the client could not create the player from it",
            playerTemplate.Object ? playerTemplate.Object->GetClass().Name : std::string("(none)"));
        return nullptr;
    }
    if (!types.Builds(header->Block, player->GetClass().Hash))
    {
        problem = fmt::format("the player's template gives core type {}, which core_object_type does not say builds {}", header->Block, PlayerClass);
        return nullptr;
    }
    player->SetCoreHeader(*header);

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
        if (!behavior || !FillBehavior(*behavior, character, stats, spells, emoteTemplateIds, permissions, problem))
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
