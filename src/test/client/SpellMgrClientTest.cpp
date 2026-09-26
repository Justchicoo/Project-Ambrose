/*
 * Project Ambrose by Imjustchico
 * Reads every spell of the user's own r806919 install through the spell manager, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it: all 18173 templates the manifest lists under Spells/ load with no failure, each one's template id the hash of its name, so the name-hash and id lookups agree for every spell; Fire Cat - Amulet deals Fire damage to one enemy through the random effect that chooses its amount; and Fire Cat is described by its school, rank, accuracy and effects, the damage it deals chosen among five amounts by a random effect, and filed with the twelve tiers above it under tiered spell group 4, not retired, while its amulet is no tiered spell.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectTemplateMgr.h"
#include "SpellMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    bool Reaches(std::vector<SpellEffectInfo> const& effects, int64 type, std::string const& damage, int64 target)
    {
        for (SpellEffectInfo const& effect : effects)
            if ((effect.Type == type && effect.DamageType == damage && effect.Target == target) || Reaches(effect.Effects, type, damage, target))
                return true;
        return false;
    }

    int64 Option(TypeCatalog const& catalog, std::string const& property, std::string const& name)
    {
        ClassInfo const* const effect = catalog.FindClass("class SpellEffect");
        PropertyInfo const* const found = effect != nullptr ? effect->FindProperty(property) : nullptr;
        std::optional<int64> const value = found != nullptr ? found->FindOptionValue(name) : std::nullopt;
        EXPECT_TRUE(value) << property << " has no option " << name;
        return value.value_or(-1);
    }

    class SpellMgrClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            ASSERT_TRUE(sTypeRegistry.LoadFromFile(LogConfig::Utf8Path(*dump)));
            sObjectTemplateMgr.SetInstall(LogConfig::Utf8Path(*client));
            std::vector<std::string> errors;
            ASSERT_TRUE(sObjectTemplateMgr.LoadManifest(errors)) << errors.front();
            s_spells = std::make_unique<SpellMgr>();
            s_spells->SetInstall(LogConfig::Utf8Path(*client));
            auto const started = std::chrono::steady_clock::now();
            s_loaded = s_spells->Load(errors);
            s_took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
            s_errors = errors;
        }

        static void TearDownTestSuite()
        {
            s_spells.reset();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
        }

        void SetUp() override
        {
            if (!s_spells)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            ASSERT_TRUE(s_loaded) << (s_errors.empty() ? std::string("the spells did not load") : s_errors.front());
        }

        static inline std::unique_ptr<SpellMgr> s_spells;
        static inline bool s_loaded = false;
        static inline std::vector<std::string> s_errors;
        static inline std::chrono::milliseconds s_took{ 0 };
    };
}

TEST_F(SpellMgrClientTest, EverySpellUnderSpellsLoadsWithNoFailure)
{
    std::shared_ptr<SpellStore const> const spells = s_spells->GetSpells();
    EXPECT_EQ(spells->Size(), 18173u);
    std::size_t effects = 0;
    for (SpellInfo const& spell : spells->GetAll())
        effects += spell.CountEffects();
    std::cout << fmt::format("{} spells with {} effects read in {} ms\n", spells->Size(), effects, s_took.count());
}

TEST_F(SpellMgrClientTest, EverySpellIsFoundByTheHashOfItsNameAsByItsId)
{
    std::shared_ptr<SpellStore const> const spells = s_spells->GetSpells();
    std::size_t disagreeing = 0;
    for (SpellInfo const& spell : spells->GetAll())
        if (spells->Find(StringHash::KiStringHash(spell.Name)) != &spell || spells->FindByName(spell.Name) != &spell)
            ++disagreeing;
    EXPECT_EQ(disagreeing, 0u);
    SpellInfo const* const cat = spells->FindByName("Fire Cat");
    ASSERT_NE(cat, nullptr);
    EXPECT_EQ(cat->TemplateId, 103007158u);
    EXPECT_EQ(cat->File, "Spells/Tiered Spells/Fire Cat.xml");
}

TEST_F(SpellMgrClientTest, FireCatAmuletDealsFireDamageToOneEnemy)
{
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    SpellInfo const* const amulet = s_spells->GetSpells()->FindByName("Fire Cat - Amulet");
    ASSERT_NE(amulet, nullptr);
    EXPECT_EQ(amulet->TemplateId, 957065192u);
    EXPECT_EQ(amulet->School, "Fire");
    EXPECT_EQ(amulet->Accuracy, 75);
    EXPECT_EQ(amulet->Pips.Rank, 1);
    EXPECT_TRUE(Reaches(amulet->Effects, Option(*catalog, "m_effectType", "kDamage"), "Fire", Option(*catalog, "m_effectTarget", "kEnemySingle")));
    ASSERT_EQ(amulet->Effects.size(), 1u);
    EXPECT_EQ(amulet->Effects.front().Class, "class RandomSpellEffect") << "the amulet's damage is chosen among several amounts";
}

TEST_F(SpellMgrClientTest, FireCatIsDescribedByItsSchoolRankAccuracyAndEffects)
{
    SpellInfo const* const cat = s_spells->GetSpells()->FindByName("Fire Cat");
    ASSERT_NE(cat, nullptr);
    std::vector<std::string> const lines = cat->Describe(sTypeRegistry.GetCatalog().get());
    for (std::string const& line : lines)
        std::cout << line << "\n";
    ASSERT_GE(lines.size(), 3u);
    EXPECT_EQ(lines[0], "Fire Cat, template 103007158, Spells/Tiered Spells/Fire Cat.xml, a TieredSpellTemplate, in tiered spell group 4");
    EXPECT_NE(lines[1].find("school Fire"), std::string::npos) << lines[1];
    EXPECT_NE(lines[1].find("rank 1"), std::string::npos) << lines[1];
    EXPECT_NE(lines[1].find("accuracy"), std::string::npos) << lines[1];
    EXPECT_NE(lines[2].find("a RandomSpellEffect of:"), std::string::npos) << lines[2] << " is the random effect that chooses Fire Cat's damage";
    EXPECT_NE(std::find(lines.begin(), lines.end(), "    kDamage 80 Fire to kEnemySingle"), lines.end()) << "the least it can deal, under the random effect";
}

TEST_F(SpellMgrClientTest, FireCatIsATieredSpellInGroupFourLikeEveryTierAboveIt)
{
    std::shared_ptr<SpellStore const> const spells = s_spells->GetSpells();
    SpellInfo const* const cat = spells->FindByName("Fire Cat");
    ASSERT_NE(cat, nullptr);
    EXPECT_TRUE(cat->Tiered);
    EXPECT_FALSE(cat->Retired);
    EXPECT_EQ(cat->TieredGroupIndex, 4) << "TieredSpellsGroupInfo.xml files Fire Cat under group 4";
    std::size_t tiers = 0;
    for (SpellInfo const& spell : spells->GetAll())
    {
        if (!spell.File.starts_with("Spells/Tiered Spells/Fire Cat - T"))
            continue;
        ++tiers;
        EXPECT_TRUE(spell.Tiered) << spell.Name;
        EXPECT_EQ(spell.TieredGroupIndex, 4) << spell.Name;
    }
    EXPECT_EQ(tiers, 12u) << "Fire Cat's higher tiers are spells of their own, while its tiered treasure cards and pet spells are plain ones";
    SpellInfo const* const amulet = spells->FindByName("Fire Cat - Amulet");
    ASSERT_NE(amulet, nullptr);
    EXPECT_FALSE(amulet->Tiered);
    EXPECT_EQ(amulet->TieredGroupIndex, SpellInfo::NoTieredGroup);
}
