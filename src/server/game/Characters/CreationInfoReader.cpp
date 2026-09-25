/*
 * Project Ambrose by Imjustchico
 * Reads a creation request in the order that refuses the cheapest thing first: the account's free slot, then the class it claims to be, then its school against the rows loaded from the world, then the appearance one property at a time against the width and the options the type dump gives each one, then the name, and only then the starting state the new wizard is given. A value that is not there, is not a number, or does not fit its property is refused by the name of that property, because an operator reading the log has to know which field a client sent wrong. The guid is left at zero: which one this wizard gets is the caller's to decide, after it has decided it wants it at all.
 */

#include "CreationInfoReader.h"
#include "CharacterNames.h"
#include "PropertyObject.h"
#include "Utf.h"

#include <fmt/format.h>

#include <array>
#include <limits>
#include <optional>
#include <utility>

namespace
{
    constexpr std::string_view Behavior = "m_avatarBehavior";

    struct AppearanceField
    {
        std::string_view Property;
        uint8 CharacterAppearance::* Small = nullptr;
        uint16 CharacterAppearance::* Wide = nullptr;
        uint32 CharacterAppearance::* Whole = nullptr;
    };

    constexpr std::array<AppearanceField, 21> Fields{ {
        { "m_nHeadHandsModel", &CharacterAppearance::HeadHandsModel, nullptr, nullptr },
        { "m_nHairModel", &CharacterAppearance::HairModel, nullptr, nullptr },
        { "m_nHatModel", &CharacterAppearance::HatModel, nullptr, nullptr },
        { "m_nTorsoModel", &CharacterAppearance::TorsoModel, nullptr, nullptr },
        { "m_nFeetModel", &CharacterAppearance::FeetModel, nullptr, nullptr },
        { "m_nWandModel", &CharacterAppearance::WandModel, nullptr, nullptr },
        { "m_nSkinColor", &CharacterAppearance::SkinColor, nullptr, nullptr },
        { "m_nSkinDecal", &CharacterAppearance::SkinDecal, nullptr, nullptr },
        { "m_nHairColor", &CharacterAppearance::HairColor, nullptr, nullptr },
        { "m_nHatColor", &CharacterAppearance::HatColor, nullptr, nullptr },
        { "m_nHatDecal", &CharacterAppearance::HatDecal, nullptr, nullptr },
        { "m_nTorsoColor", &CharacterAppearance::TorsoColor, nullptr, nullptr },
        { "m_nTorsoDecal", &CharacterAppearance::TorsoDecal, nullptr, nullptr },
        { "m_nTorsoDecal2", &CharacterAppearance::TorsoDecal2, nullptr, nullptr },
        { "m_nFeetColor", &CharacterAppearance::FeetColor, nullptr, nullptr },
        { "m_nFeetDecal", &CharacterAppearance::FeetDecal, nullptr, nullptr },
        { "m_afterCombatDance", &CharacterAppearance::AfterCombatDance, nullptr, nullptr },
        { "m_extendedHairColor", &CharacterAppearance::ExtendedHairColor, nullptr, nullptr },
        { "m_nSkinDecal2", nullptr, &CharacterAppearance::SkinDecal2, nullptr },
        { "m_extendedSkinDecal", nullptr, &CharacterAppearance::ExtendedSkinDecal, nullptr },
        { "m_afterCombatVictoryDance", nullptr, nullptr, &CharacterAppearance::AfterCombatVictoryDance },
    } };

    std::optional<int64> Signed(PropertyValue const& value)
    {
        if (auto const* const held = value.GetIf<bool>())
            return *held ? 1 : 0;
        if (auto const* const held = value.GetIf<int8>())
            return *held;
        if (auto const* const held = value.GetIf<uint8>())
            return *held;
        if (auto const* const held = value.GetIf<int16>())
            return *held;
        if (auto const* const held = value.GetIf<uint16>())
            return *held;
        if (auto const* const held = value.GetIf<int32>())
            return *held;
        if (auto const* const held = value.GetIf<uint32>())
            return *held;
        if (auto const* const held = value.GetIf<int64>())
            return *held;
        if (auto const* const held = value.GetIf<uint64>())
            return static_cast<int64>(*held);
        return std::nullopt;
    }

    std::optional<uint64> Unsigned(PropertyValue const& value)
    {
        if (auto const* const held = value.GetIf<uint64>())
            return *held;
        std::optional<int64> const number = Signed(value);
        if (!number || *number < 0)
            return std::nullopt;
        return static_cast<uint64>(*number);
    }

    class Reading
    {
    public:
        explicit Reading(PropertyObject const& object) : _object(object)
        {
        }

        std::optional<uint64> Whole(std::string_view name, std::string& refusal) const
        {
            PropertyValue const* const value = _object.Get(name);
            if (value == nullptr)
            {
                refusal = fmt::format("{} carries no {}", _object.GetClass().Name, name);
                return std::nullopt;
            }
            std::optional<uint64> const number = Unsigned(*value);
            if (!number)
            {
                refusal = fmt::format("{} of {} is not a whole number", name, _object.GetClass().Name);
                return std::nullopt;
            }
            PropertyInfo const* const property = _object.GetClass().FindProperty(name);
            if (property != nullptr && property->BitWidth != 0 && property->BitWidth < 64)
            {
                uint64 const largest = (uint64{ 1 } << property->BitWidth) - 1;
                if (*number > largest)
                {
                    refusal = fmt::format("{} is {}, which does not fit the {} bits its property holds", name, *number, property->BitWidth);
                    return std::nullopt;
                }
            }
            return number;
        }

        std::optional<std::string_view> Option(std::string_view name, std::string& refusal) const
        {
            PropertyValue const* const value = _object.Get(name);
            PropertyInfo const* const property = _object.GetClass().FindProperty(name);
            if (value == nullptr || property == nullptr)
            {
                refusal = fmt::format("{} carries no {}", _object.GetClass().Name, name);
                return std::nullopt;
            }
            std::optional<int64> const number = Signed(*value);
            if (!number)
            {
                refusal = fmt::format("{} of {} is not a number", name, _object.GetClass().Name);
                return std::nullopt;
            }
            std::optional<std::string_view> const option = property->FindOptionName(*number);
            if (!option)
            {
                refusal = fmt::format("{} is {}, which is not one of the values {} names", name, *number, property->TypeName);
                return std::nullopt;
            }
            return option;
        }

    private:
        PropertyObject const& _object;
    };

    CreationOutcome Refuse(std::string reason)
    {
        CreationOutcome outcome;
        outcome.Refusal = std::move(reason);
        return outcome;
    }
}

CreationOutcome CreationInfoReader::Read(PropertyObject const& info, CharacterCreateSet const& rows, CharacterNameSet const& names,
    CreationLimits const& limits, CreationRules const& rules)
{
    uint64 const allowed = uint64{ limits.MaxPerAccount } + limits.PurchasedSlots;
    if (limits.ExistingCharacters >= allowed)
        return Refuse(fmt::format("the account already holds {} wizards and is allowed {}", limits.ExistingCharacters, allowed));
    if (!info.IsA("class WizardCharacterCreationInfo"))
        return Refuse(fmt::format("the creation info is a {} rather than a WizardCharacterCreationInfo", info.GetClass().Name));

    std::string refusal;
    Reading const reading(info);
    std::optional<uint64> const school = reading.Whole("m_schoolOfFocus", refusal);
    if (!school)
        return Refuse(std::move(refusal));
    if (*school > std::numeric_limits<uint32>::max() || !rows.HasSchool(static_cast<uint32>(*school)))
        return Refuse(fmt::format("school {} is not a school a wizard may be given", *school));

    PropertyValue const* const behaviorValue = info.Get(Behavior);
    PropertyObject const* const behavior = behaviorValue == nullptr ? nullptr : behaviorValue->AsObject();
    if (behavior == nullptr)
        return Refuse("the creation info carries no appearance");
    if (!behavior->IsA("class WizardCharacterBehavior"))
        return Refuse(fmt::format("the appearance is a {} rather than a WizardCharacterBehavior", behavior->GetClass().Name));

    Reading const look(*behavior);
    std::optional<std::string_view> const gender = look.Option("m_eGender", refusal);
    if (!gender)
        return Refuse(std::move(refusal));
    if (*gender == NeutralGender)
        return Refuse("the appearance asks for the neutral gender, which a wizard is not created with");
    std::optional<std::string_view> const race = look.Option("m_eRace", refusal);
    if (!race)
        return Refuse(std::move(refusal));
    if (*race != HumanRace)
        return Refuse(fmt::format("the appearance asks to be a {} rather than a {}", *race, HumanRace));

    CharacterSummary character;
    CharacterAppearance& appearance = character.Appearance;
    for (AppearanceField const& field : Fields)
    {
        std::optional<uint64> const value = look.Whole(field.Property, refusal);
        if (!value)
            return Refuse(std::move(refusal));
        uint64 const largest = field.Small != nullptr ? std::numeric_limits<uint8>::max()
            : field.Wide != nullptr                   ? std::numeric_limits<uint16>::max()
                                                      : std::numeric_limits<uint32>::max();
        if (*value > largest)
            return Refuse(fmt::format("{} is {}, which is larger than this server stores", field.Property, *value));
        if (field.Small != nullptr)
            appearance.*field.Small = static_cast<uint8>(*value);
        else if (field.Wide != nullptr)
            appearance.*field.Wide = static_cast<uint16>(*value);
        else
            appearance.*field.Whole = static_cast<uint32>(*value);
    }

    std::optional<uint64> const templateName = look.Whole("m_behaviorTemplateNameID", refusal);
    if (!templateName || *templateName > std::numeric_limits<uint32>::max())
        return Refuse(refusal.empty() ? std::string("the appearance names no behavior template") : std::move(refusal));
    appearance.BehaviorTemplateNameId = static_cast<uint32>(*templateName);

    PropertyInfo const* const genderProperty = behavior->GetClass().FindProperty("m_eGender");
    PropertyInfo const* const raceProperty = behavior->GetClass().FindProperty("m_eRace");
    std::optional<int64> const genderValue = genderProperty == nullptr ? std::nullopt : genderProperty->FindOptionValue(*gender);
    std::optional<int64> const raceValue = raceProperty == nullptr ? std::nullopt : raceProperty->FindOptionValue(*race);
    if (!genderValue || !raceValue || *genderValue < 0 || *raceValue < 0)
        return Refuse("the type dump does not name the gender or the race this server stores");
    appearance.Gender = static_cast<uint32>(*genderValue);
    appearance.Race = static_cast<uint32>(*raceValue);

    std::optional<uint64> const indices = reading.Whole("m_nameIndices", refusal);
    if (!indices || *indices > std::numeric_limits<uint32>::max())
        return Refuse(refusal.empty() ? std::string("the creation info carries no name") : std::move(refusal));
    character.NameIndices = static_cast<uint32>(*indices);

    PropertyValue const* const nameValue = info.Get("m_name");
    std::u16string const wide = nameValue == nullptr || nameValue->GetIf<std::u16string>() == nullptr ? std::u16string()
                                                                                                     : *nameValue->GetIf<std::u16string>();
    if (!wide.empty())
    {
        if (!rules.AllowCustomName)
            return Refuse("the request carries a name of its own, which this server does not let an account choose");
        if (wide.size() > MaxCustomNameCharacters)
            return Refuse(fmt::format("the name of its own is {} characters and at most {} are allowed", wide.size(), MaxCustomNameCharacters));
        std::optional<std::string> narrow = Utf::Utf16ToUtf8(wide, Utf::InvalidPolicy::Reject);
        if (!narrow)
            return Refuse("the name of its own is not text this server can store");
        character.CustomName = std::move(*narrow);
    }
    else
    {
        NameCheck const checked = names.Check(character.NameIndices, appearance.Gender, rules.Locale);
        if (checked != NameCheck::Ok)
            return Refuse(fmt::format("the name it asks for is refused: {} (name indices 0x{:08X}, first {}, middle {}, last {}, in {})",
                CharacterNameSet::GetCheckName(checked), character.NameIndices, (character.NameIndices >> 16) & 0xFFu,
                (character.NameIndices >> 8) & 0xFFu, character.NameIndices & 0xFFu, rules.Locale));
    }

    CharacterStartState const* const start = rows.GetStart(static_cast<uint32>(*school));
    if (start == nullptr)
        return Refuse(fmt::format("school {} has no starting state, so a wizard made from it would have nowhere to stand", *school));

    character.SchoolId = static_cast<uint32>(*school);
    character.Level = start->Level;
    character.Experience = start->Experience;
    character.World = start->World;
    character.Zone = start->Zone;
    character.ZoneDisplay = start->ZoneDisplay.empty() ? start->Zone : start->ZoneDisplay;
    character.PositionX = start->PositionX;
    character.PositionY = start->PositionY;
    character.PositionZ = start->PositionZ;
    character.Orientation = start->Orientation;

    CreationOutcome outcome;
    outcome.Ok = true;
    outcome.Character = std::move(character);
    return outcome;
}
