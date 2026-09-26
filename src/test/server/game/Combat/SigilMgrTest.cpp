/*
 * Project Ambrose by Imjustchico
 * Tests the sigil manager on an install the test builds through a type dump it writes: every template the manifest lists under Sigils/ is read and found by name; a combat sigil keeps its circles in order, four monster and four player, its engage radius, battlefield effects and PvE limits, while a minigame sigil has circles and no combat values; a reload that meets sigils that fail keeps the set serving and names each failure; and a sigil is described by its circles and limits.
 */

#include "LogTestDirectory.h"
#include "ObjectTemplateMgr.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"
#include "SigilMgr.h"
#include "StringHash.h"
#include "TemplateDumpFixtures.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace TemplateDumpFixtures;

    std::string SigilDump()
    {
        Json classes = Json::object();
        AddTemplateClasses(classes);
        AddSpellClasses(classes);
        AddSigilClasses(classes);
        return Dump(classes);
    }

    bool Holds(std::vector<std::string> const& errors, std::string const& text)
    {
        for (std::string const& error : errors)
            if (error.find(text) != std::string::npos)
                return true;
        return false;
    }

    class SigilMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _views.Add(TemplateManifestView::Definition);
            _views.Add(TemplateLocationView::Definition);
            _views.Add(SpellEffectView::Definition);
            _views.Add(SigilTemplateView::Definition);
            _views.Add(CombatSigilTemplateView::Definition);
            _views.Add(SigilSubCircleView::Definition);
            sReloadMgr.Clear();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            std::vector<std::string> cleared;
            ASSERT_TRUE(sTypeRegistry.ClearSupplement(cleared));
            ASSERT_TRUE(sTypeRegistry.LoadFromText(SigilDump(), "sigils.json")) << sTypeRegistry.GetErrors().front();
            _catalog = sTypeRegistry.GetCatalog();
            Write(GoodSigils());
            sObjectTemplateMgr.SetInstall(_directory.Path());
            std::vector<std::string> errors;
            ASSERT_TRUE(sObjectTemplateMgr.LoadManifest(errors)) << errors.front();
            _sigils.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            _sigils.Clear();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
            std::vector<std::string> cleared;
            sTypeRegistry.ClearSupplement(cleared);
            sTypeRegistry.SetViews(&sTypedViewRegistry);
        }

        void Write(Files const& files)
        {
            Locations locations;
            for (auto const& [path, bytes] : files)
            {
                std::string const stem = path.substr(path.rfind('/') + 1, path.size() - path.rfind('/') - 5);
                locations.emplace_back(StringHash::KiStringHash(stem), path);
            }
            WriteRoot(_directory.Path() / "Data" / "GameData", _catalog, locations, files);
        }

        PropertyObjectPtr Circle(std::string const& kind, std::string const& slot, float rotation)
        {
            PropertyObjectPtr circle = PropertyObject::Create(_catalog, "class SigilSubCircle");
            EXPECT_EQ(circle->Set("m_locationType", kind), PropertySetResult::Ok);
            EXPECT_EQ(circle->Set("m_locationPreference", slot), PropertySetResult::Ok);
            EXPECT_EQ(circle->Set("m_rotation", rotation), PropertySetResult::Ok);
            EXPECT_EQ(circle->Set("m_radius", 600.0f), PropertySetResult::Ok);
            return circle;
        }

        std::vector<uint8> Sigil(std::string const& className, std::string const& name, std::size_t monsters, std::size_t players)
        {
            PropertyObjectPtr sigil = PropertyObject::Create(_catalog, className);
            EXPECT_TRUE(sigil) << className;
            EXPECT_EQ(sigil->Set("m_sigilName", name), PropertySetResult::Ok);
            EXPECT_EQ(sigil->Set("m_sigilType", std::string(className == "class CombatSigilTemplate" ? "Combat" : "Minigame")), PropertySetResult::Ok);
            PropertyValue::List circles;
            for (std::size_t index = 0; index < monsters; ++index)
                circles.emplace_back(Circle("MonsterCircle", fmt::format("Slot_{:02}", index + 1), 144.0f - 36.0f * static_cast<float>(index)));
            for (std::size_t index = 0; index < players; ++index)
                circles.emplace_back(Circle("PlayerCircle", "", -36.0f - 36.0f * static_cast<float>(index)));
            EXPECT_EQ(sigil->Set("m_subCircles", std::move(circles)), PropertySetResult::Ok);
            if (className == "class CombatSigilTemplate")
            {
                EXPECT_EQ(sigil->Set("m_engageRadius", 800.0f), PropertySetResult::Ok);
                EXPECT_EQ(sigil->Set("m_damageLimitPvE", 2.76f), PropertySetResult::Ok);
                EXPECT_EQ(sigil->Set("m_dK0PvE", 275.0f), PropertySetResult::Ok);
                EXPECT_EQ(sigil->Set("m_resistLimitPvE", 1.25f), PropertySetResult::Ok);
                EXPECT_EQ(sigil->Set("m_rK0PvE", 120.0f), PropertySetResult::Ok);
                PropertyObjectPtr effect = PropertyObject::Create(_catalog, "class SpellEffect");
                EXPECT_EQ(effect->Set("m_effectType", int64{ 1 }), PropertySetResult::Ok);
                EXPECT_EQ(effect->Set("m_effectParam", int32{ 50 }), PropertySetResult::Ok);
                PropertyValue::List effects;
                effects.emplace_back(std::move(effect));
                EXPECT_EQ(sigil->Set("m_battlefieldEffects", std::move(effects)), PropertySetResult::Ok);
            }
            return WriteBind(sigil);
        }

        Files GoodSigils()
        {
            return { { "Sigils/CombatSigil8Actor.xml", Sigil("class CombatSigilTemplate", "CombatSigil8Actor", 4, 4) },
                { "Sigils/MinigameSigil1Actor.xml", Sigil("class MinigameSigilTemplate", "MinigameSigil1Actor", 0, 1) } };
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        TypeCatalogPtr _catalog;
        SigilMgr _sigils;
    };
}

TEST_F(SigilMgrTest, ACombatSigilKeepsItsCirclesInOrderWithItsLimits)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(_sigils.Load(errors)) << errors.front();
    std::shared_ptr<SigilStore const> const sigils = _sigils.GetSigils();
    ASSERT_EQ(sigils->Size(), 2u);
    SigilInfo const* const combat = sigils->FindByName("CombatSigil8Actor");
    ASSERT_NE(combat, nullptr);
    EXPECT_EQ(combat->TemplateId, StringHash::KiStringHash("CombatSigil8Actor"));
    ASSERT_EQ(combat->Circles.size(), 8u);
    EXPECT_EQ(combat->CountCircles("MonsterCircle"), 4u);
    EXPECT_EQ(combat->CountCircles("PlayerCircle"), 4u);
    EXPECT_EQ(combat->Circles.front().LocationPreference, "Slot_01");
    EXPECT_FLOAT_EQ(combat->Circles.front().Rotation, 144.0f);
    EXPECT_TRUE(combat->Combat);
    EXPECT_FLOAT_EQ(combat->EngageRadius, 800.0f);
    EXPECT_FLOAT_EQ(combat->PvE.DamageLimit, 2.76f);
    EXPECT_FLOAT_EQ(combat->PvE.DamageK0, 275.0f);
    EXPECT_FLOAT_EQ(combat->PvE.ResistLimit, 1.25f);
    EXPECT_FLOAT_EQ(combat->PvE.ResistK0, 120.0f);
    ASSERT_EQ(combat->BattlefieldEffects.size(), 1u);
    EXPECT_EQ(combat->BattlefieldEffects.front().Param, 50);

    SigilInfo const* const minigame = sigils->FindByName("minigamesigil1actor");
    ASSERT_NE(minigame, nullptr);
    EXPECT_FALSE(minigame->Combat) << "a minigame sigil has circles and no combat values";
    EXPECT_EQ(minigame->CountCircles("PlayerCircle"), 1u);
}

TEST_F(SigilMgrTest, AReloadThatMeetsFailingSigilsKeepsTheSetServingAndNamesEachFailure)
{
    sObjectTemplateMgr.RegisterReloadTargets();
    _sigils.RegisterReloadTargets();
    ASSERT_TRUE(sReloadMgr.Reload(SigilMgr::Target).Ok);

    Files files = GoodSigils();
    files.emplace_back("Sigils/NotASigil.xml", Manifest(_catalog, {}));
    files.emplace_back("Sigils/Broken.xml", std::vector<uint8>{ 'b', 'r', 'o', 'k', 'e', 'n' });
    Write(files);
    ASSERT_TRUE(sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget).Ok);
    ReloadOutcome const broken = sReloadMgr.Reload(SigilMgr::Target);
    EXPECT_FALSE(broken.Ok);
    ASSERT_EQ(broken.Errors.size(), 3u);
    EXPECT_TRUE(Holds(broken.Errors, "Sigils/NotASigil.xml is a class TemplateManifest, which is not a SigilTemplate"));
    EXPECT_TRUE(Holds(broken.Errors, "Sigils/Broken.xml in Root.wad does not read"));
    EXPECT_TRUE(Holds(broken.Errors, "2 of the 4 sigils under Sigils/ fail to load"));
    EXPECT_EQ(_sigils.GetSigils()->Size(), 2u) << "the set that was serving goes on serving";
}

TEST_F(SigilMgrTest, ASigilIsDescribedByItsCirclesAndLimits)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(_sigils.Load(errors)) << errors.front();
    std::vector<std::string> const lines = _sigils.GetSigils()->FindByName("CombatSigil8Actor")->Describe();
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[0], fmt::format("CombatSigil8Actor, template {}, Sigils/CombatSigil8Actor.xml, a CombatSigilTemplate, type Combat", StringHash::KiStringHash("CombatSigil8Actor")));
    EXPECT_EQ(lines[1], "  8 circle(s), 4 MonsterCircle, 4 PlayerCircle");
    EXPECT_EQ(lines[2], "  engages at 800, 1 battlefield effect(s), shadow threshold factor 0, shadow pip rating factor 0");
    EXPECT_EQ(lines[4], "  PvE damage, resist and pierce scalars 0, 0, 0; damage limit 2.76 (k0 275, n0 0), resist limit 1.25 (k0 120, n0 0)");
}
