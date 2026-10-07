/*
 * Project Ambrose by Imjustchico
 * Lists the ObjectProperty message fields the server reads or writes: MSG_ADDEFFECT carries one unwrapped GameEffectBase in CoreObject form, its core type zero so a plain class hash follows, which the client's MSG_AddEffect handler loads and hands to the object's effect behavior, since a nonzero first byte has it look for a core template, badges travel enveloped, the character list and creation messages carry an unwrapped WizardCharacterCreationInfo, MSG_LOGINCOMPLETE carries the player's own game object enveloped and in CoreObject form, as every one the client has accepted there did, and the ids of the objects the client waits for as an unwrapped CriticalObjectList, which the client's MSG_LoginComplete handler loads straight from the field with the plain serializer, while it decompresses Data first, MSG_NEWOBJECT carries any game object unwrapped in CoreObject form, which the client's MSG_NewObject handler hands to the serializer Data is read with, without decompressing it, and MSG_TIMEDACCESSPASSES and MSG_SUBSCRIBERONLYITEMS carry an unwrapped ActiveTimedAccessPassList and SubscriberOnlyItemsList, the classes the client's own handlers load them into with a serializer that reads no envelope and no flags word.
 */

#include "ObjectFields.h"

#include <array>

namespace
{
    constexpr std::array<std::string_view, 1> GameEffectClasses{ "class GameEffectBase" };
    constexpr std::array<std::string_view, 1> BadgeInfoClasses{ "class BadgeInfoList" };
    constexpr std::array<std::string_view, 1> BadgeFilterClasses{ "class BadgeFilterInfoList" };
    constexpr std::array<std::string_view, 1> CreationClasses{ "class WizardCharacterCreationInfo" };
    constexpr std::array<std::string_view, 1> PlayerObjectClasses{ "class WizClientObject" };
    constexpr std::array<std::string_view, 1> GameObjectClasses{ "class CoreObject" };
    constexpr std::array<std::string_view, 1> CriticalObjectClasses{ "class CriticalObjectList" };
    constexpr std::array<std::string_view, 1> SubscriberOnlyItemClasses{ "class SubscriberOnlyItemsList" };
    constexpr std::array<std::string_view, 1> TimedAccessPassClasses{ "class ActiveTimedAccessPassList" };

    constexpr std::array<ObjectField, 10> Fields{ {
        { "MSG_ADDEFFECT", "EffectData", GameEffectClasses, false, false, true },
        { "MSG_BADGES", "BadgeInfo", BadgeInfoClasses, true, false },
        { "MSG_BADGES", "BadgeFilterInfo", BadgeFilterClasses, true, false },
        { "MSG_CHARACTERINFO", "CharacterInfo", CreationClasses, false, false },
        { "MSG_CREATECHARACTER", "CreationInfo", CreationClasses, false, false },
        { "MSG_LOGINCOMPLETE", "Data", PlayerObjectClasses, true, false, true },
        { "MSG_LOGINCOMPLETE", "CriticalObjects", CriticalObjectClasses, false, false },
        { "MSG_NEWOBJECT", "Data", GameObjectClasses, false, false, true },
        { "MSG_SUBSCRIBERONLYITEMS", "Data", SubscriberOnlyItemClasses, false, false },
        { "MSG_TIMEDACCESSPASSES", "Data", TimedAccessPassClasses, false, false },
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
