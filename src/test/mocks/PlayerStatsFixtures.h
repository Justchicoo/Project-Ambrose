/*
 * Project Ambrose by Imjustchico
 * What player stat tests share: WizGameStats and ClientMagicSchoolBehavior laid out with the property names, types and transmit flags the client's classes use for every field a wizard's stats fill, and a Fire level table built to a cap, whose level 1 earns a training point as every odd level does, for tests that run without a client or a database.
 */

#ifndef AMBROSE_PLAYERSTATSFIXTURES_H
#define AMBROSE_PLAYERSTATSFIXTURES_H

#include "CharacterTypeFixtures.h"
#include "PlayerLevels.h"
#include "StringHash.h"

#include <string>
#include <utility>
#include <vector>

namespace PlayerStatsFixtures
{
    inline uint32 const Fire = StringHash::KiStringHash("Fire");

    inline std::vector<std::pair<std::string, CharacterTypeFixtures::Detail::Json>> GameStatsProperties()
    {
        using CharacterTypeFixtures::Detail::Property;
        constexpr uint32 Wire = 0x1F;
        std::vector<std::pair<std::string, std::string>> const fields{ { "int", "m_baseHitpoints" }, { "int", "m_baseMana" }, { "int", "m_baseGoldPouch" }, { "int", "m_energyMax" },
            { "int", "m_currentHitpoints" }, { "int", "m_currentGold" }, { "int", "m_currentMana" }, { "int", "m_currentArenaPoints" }, { "float", "m_potionMax" }, { "float", "m_potionCharge" },
            { "int", "m_referenceLevel" }, { "int", "m_highestCharacterLevelOnAccount" }, { "float", "m_powerPipBase" }, { "int", "m_shadowPipMax" }, { "int", "m_pipConversionBaseAllSchools" },
            { "float", "m_shadowPipRating" }, { "float", "m_archmasteryBase" }, { "unsigned int", "m_schoolID" }, { "unsigned int", "m_secondarySchool" } };
        std::vector<std::pair<std::string, CharacterTypeFixtures::Detail::Json>> properties;
        uint32 id = 0;
        for (auto const& [type, name] : fields)
            properties.emplace_back(name, Property(type, name, id++, Wire));
        return properties;
    }

    inline std::vector<std::pair<std::string, CharacterTypeFixtures::Detail::Json>> SchoolBehaviorProperties()
    {
        using CharacterTypeFixtures::Detail::Property;
        constexpr uint32 Wire = 0x1F;
        constexpr uint32 Local = 0x27;
        return { { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) }, { "m_schoolOfFocus", Property("unsigned int", "m_schoolOfFocus", 1, Wire) },
            { "m_experiencePoints", Property("int", "m_experiencePoints", 2, Wire) }, { "m_level", Property("int", "m_level", 3, Wire) },
            { "m_trainingPoints", Property("int", "m_trainingPoints", 4, Wire) }, { "m_overflowXP", Property("int", "m_overflowXP", 5, Wire) },
            { "m_levelLocked", Property("int", "m_levelLocked", 6, Wire) }, { "m_secondarySchool", Property("unsigned int", "m_secondarySchool", 7, Wire) } };
    }

    inline PlayerLevelInfo FireLevel(uint32 level)
    {
        PlayerLevelInfo row;
        row.SchoolId = Fire;
        row.Level = level;
        row.XpToLevel = static_cast<int32>(level * level * 45);
        row.Hitpoints = level == 0 ? 0 : 400 + static_cast<int32>(level) * 15;
        row.Mana = level == 0 ? 0 : 13 + static_cast<int32>(level) * 2;
        row.Gold = level == 0 ? 0 : 300000;
        row.PipChance = level >= 10 ? 0.1f : 0.0f;
        row.TrainingPoints = level % 2 == 1 ? 1 : 0;
        row.PetEnergy = level == 0 ? 0 : 39 + static_cast<int32>(level);
        row.PipConversion[0] = level >= 3 ? 12 : 0;
        row.ShadowPipRating = level >= 5 ? 5.0f : 0.0f;
        row.Archmastery = level >= 5 ? 40.0f : 0.0f;
        row.LevelName = "Levels_Level" + std::to_string(level);
        return row;
    }

    inline PlayerLevelData FireLevels(uint32 cap)
    {
        PlayerLevelData data;
        for (uint32 level = 0; level <= cap; ++level)
            data.Levels.push_back(FireLevel(level));
        data.Schools = { { Fire, "Fire", 1, 1, { "FireWeaving-Level01" } } };
        data.XpConfig = { { std::string(PlayerLevelSet::MaxLevelSetting), static_cast<double>(cap) } };
        return data;
    }
}

#endif
