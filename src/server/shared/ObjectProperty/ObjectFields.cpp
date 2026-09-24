/*
 * Project Ambrose by Imjustchico
 * Lists the ObjectProperty message fields the server reads or writes: badges travel enveloped, and the character list and creation messages carry an unwrapped WizardCharacterCreationInfo.
 */

#include "ObjectFields.h"

#include <array>

namespace
{
    constexpr std::array<std::string_view, 1> BadgeInfoClasses{ "class BadgeInfoList" };
    constexpr std::array<std::string_view, 1> BadgeFilterClasses{ "class BadgeFilterInfoList" };
    constexpr std::array<std::string_view, 1> CreationClasses{ "class WizardCharacterCreationInfo" };

    constexpr std::array<ObjectField, 4> Fields{ {
        { "MSG_BADGES", "BadgeInfo", BadgeInfoClasses, true, false },
        { "MSG_BADGES", "BadgeFilterInfo", BadgeFilterClasses, true, false },
        { "MSG_CHARACTERINFO", "CharacterInfo", CreationClasses, false, false },
        { "MSG_CREATECHARACTER", "CreationInfo", CreationClasses, false, false },
    } };
}

std::span<ObjectField const> ObjectFields::GetAll() noexcept
{
    return Fields;
}

ObjectField const* ObjectFields::Find(std::string_view message, std::string_view field) noexcept
{
    for (ObjectField const& entry : Fields)
        if (entry.Message == message && entry.Field == field)
            return &entry;
    return nullptr;
}
