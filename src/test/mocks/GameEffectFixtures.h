/*
 * Project Ambrose by Imjustchico
 * The game effect classes laid out as the client's dump gives them, for tests that run without a client: GameEffectBase with its seven shared properties, NamedEffect deriving from it with its override name, GameEffectContainer with the public list the client receives and the private one it never does, and BaseGameEffectBehavior holding the container.
 */

#ifndef AMBROSE_GAMEEFFECTFIXTURES_H
#define AMBROSE_GAMEEFFECTFIXTURES_H

#include "CharacterTypeFixtures.h"

#include <string>
#include <utility>
#include <vector>

namespace GameEffectFixtures
{
    inline constexpr uint32 Wire = 0x1F;
    inline constexpr uint32 Kept = 0x07;

    inline std::vector<std::pair<std::string, CharacterTypeFixtures::Detail::Json>> BaseProperties()
    {
        using CharacterTypeFixtures::Detail::Property;
        return {
            { "m_currentTickCount", Property("double", "m_currentTickCount", 0, Wire) },
            { "m_effectNameID", Property("unsigned int", "m_effectNameID", 1, Wire) },
            { "m_bIsOnPet", Property("bool", "m_bIsOnPet", 2, Wire) },
            { "m_originatorID", Property("gid", "m_originatorID", 3, Kept) },
            { "m_itemSlotID", Property("unsigned int", "m_itemSlotID", 4, Wire) },
            { "m_internalID", Property("int", "m_internalID", 5, Wire) },
            { "m_endTime", Property("unsigned int", "m_endTime", 6, Wire) } };
    }

    inline void AddClasses(CharacterTypeFixtures::Detail::Json& classes)
    {
        using CharacterTypeFixtures::Detail::AddClass;
        using CharacterTypeFixtures::Detail::Json;
        using CharacterTypeFixtures::Detail::Property;
        AddClass(classes, "class GameEffectBase", Json::array({ "PropertyClass" }), BaseProperties());
        std::vector<std::pair<std::string, Json>> named = BaseProperties();
        named.emplace_back("m_overrideName", Property("std::string", "m_overrideName", 7, Wire));
        AddClass(classes, "class NamedEffect", Json::array({ "GameEffectBase", "PropertyClass" }), std::move(named));
        AddClass(classes, "class GameEffectContainer", Json::array({ "PropertyClass" }), {
            { "m_publicEffects", Property("class SharedPointer<class GameEffectBase>", "m_publicEffects", 0, Wire, "List") },
            { "m_myEffects", Property("class SharedPointer<class GameEffectBase>", "m_myEffects", 1, Kept, "List") } });
        AddClass(classes, "class BaseGameEffectBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, 0x27) },
            { "m_gameEffects", Property("class SharedPointer<class GameEffectContainer>", "m_gameEffects", 1, Wire) } });
    }
}

#endif
