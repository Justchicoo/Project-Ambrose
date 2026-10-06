/*
 * Project Ambrose by Imjustchico
 * An install for the item template tests: a writer's type dump holding a stat effect class and a shop behavior class the reader's dump leaves out, and a Root.wad whose manifest lists a hat, a robe with a behavior of a class the reader lacks, an equip requirement of Ice magic level 5 or more and a max health equip effect, an NPC that is no item, the robe's item set bonus 42 granting two bonuses without stacking, and a spell outside ObjectData, written again with an edited hat or with a robe whose equip effect or requirement is of a class the reader lacks.
 */

#ifndef AMBROSE_ITEMTEMPLATEFIXTURES_H
#define AMBROSE_ITEMTEMPLATEFIXTURES_H

#include "TemplateDumpFixtures.h"
#include "TypeRegistry.h"
#include "TypedView.h"
#include "Types.h"

#include <filesystem>
#include <string>

class ItemTemplateFixtures
{
public:
    static constexpr uint32 HatId = 1652259;
    static constexpr uint32 RobeId = 1652300;
    static constexpr uint32 NpcId = 38232;
    static constexpr uint32 SpellId = 77;
    static constexpr uint32 SetBonusId = 42;
    static constexpr char const* UnknownEffect = "class StatisticEffectInfo";
    static constexpr char const* UnknownBehavior = "class ShopBehaviorTemplate";
    static constexpr char const* UnknownRequirement = "class ReqHasBadge";

    enum class Robe { Plain, UnknownEffect, UnknownRequirement };

    ItemTemplateFixtures();

    static std::string ReaderDump();
    void Write(std::filesystem::path const& gameData, float hatCost = 125.0f, Robe robe = Robe::Plain);

private:
    PropertyObjectPtr Create(std::string const& type);
    PropertyObjectPtr Item(std::string const& name, std::string const& display, std::string const& school, float cost, int32 rank, int32 limit, uint32 setBonus);

    TypedViewRegistry _views;
    TypeRegistry _writer;
};

#endif
