/*
 * Project Ambrose by Imjustchico
 * Tests the spell manager on an install the test builds through a type dump it writes: every template the manifest lists under Spells/ is read, and no other, found by template id, by exact name through its hash and by name whatever its case, and searched by the text a name holds; a spell's effects come out as a tree, a random effect holding the effects it chooses among and a conditional one its effect behind its requirements; a spell whose id is not the hash of its name is refused; a reload that meets spells that fail keeps the set serving and names each way they fail once with how many more fail it; and a spell is described by its school, rank, accuracy and effects.
 */

#include "LogTestDirectory.h"
#include "ObjectTemplateMgr.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"
#include "SpellMgr.h"
#include "StringHash.h"
#include "TemplateDumpFixtures.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace TemplateDumpFixtures;
    using Entries = Files;

    std::string SpellDump()
    {
        Json classes = Json::object();
        AddTemplateClasses(classes);
        AddSpellClasses(classes);
        return Dump(classes);
    }

    bool Holds(std::vector<std::string> const& errors, std::string const& text)
    {
        for (std::string const& error : errors)
            if (error.find(text) != std::string::npos)
                return true;
        return false;
    }

    class SpellMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _views.Add(TemplateManifestView::Definition);
            _views.Add(TemplateLocationView::Definition);
            _views.Add(CoreTemplateView::Definition);
            _views.Add(SpellTemplateView::Definition);
            _views.Add(SpellEffectView::Definition);
            _views.Add(SpellRankView::Definition);
            sReloadMgr.Clear();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            std::vector<std::string> cleared;
            ASSERT_TRUE(sTypeRegistry.ClearSupplement(cleared));
            ASSERT_TRUE(sTypeRegistry.LoadFromText(SpellDump(), "spells.json")) << sTypeRegistry.GetErrors().front();
            _catalog = sTypeRegistry.GetCatalog();

            std::filesystem::create_directories(GameData());
            WriteRoot(GoodSpells());
            sObjectTemplateMgr.SetInstall(_directory.Path());
            std::vector<std::string> errors;
            ASSERT_TRUE(sObjectTemplateMgr.LoadManifest(errors)) << errors.front();
            _spells.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            _spells.Clear();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
            std::vector<std::string> cleared;
            sTypeRegistry.ClearSupplement(cleared);
            sTypeRegistry.SetViews(&sTypedViewRegistry);
        }

        std::filesystem::path GameData() const
        {
            return _directory.Path() / "Data" / "GameData";
        }

        void WriteRoot(Entries const& files)
        {
            Locations locations;
            for (auto const& [path, bytes] : files)
                locations.emplace_back(_ids.at(path), path);
            TemplateDumpFixtures::WriteRoot(GameData(), _catalog, locations, files);
        }

        Entries GoodSpells()
        {
            PropertyValue::List amulet;
            amulet.emplace_back(Effect("class RandomSpellEffect", 0, 0, "", 8, Choices(Effect("class SpellEffect", 1, 95, "Fire", 8), Effect("class SpellEffect", 1, 135, "Fire", 8))));
            PropertyValue::List cat;
            cat.emplace_back(Effect("class SpellEffect", 1, 80, "Fire", 8));
            PropertyValue::List heal;
            heal.emplace_back(Guarded(Effect("class SpellEffect", 3, 100, "", 11)));
            Entries spells{ { "Spells/Fire Cat.xml", Spell("class TieredSpellTemplate", "Fire Cat", "Fire", 75, 1, std::move(cat)) },
                { "Spells/Fire Cat - Amulet.xml", Spell("class SpellTemplate", "Fire Cat - Amulet", "Fire", 75, 1, std::move(amulet)) },
                { "Spells/Life/Guarded Heal.xml", Spell("class SpellTemplate", "Guarded Heal", "Life", 90, 2, std::move(heal)) },
                { "ObjectData/Not A Spell.xml", Spell("class SpellTemplate", "Not A Spell", "Fire", 0, 0, {}) } };
            _ids = { { "Spells/Fire Cat.xml", StringHash::KiStringHash("Fire Cat") }, { "Spells/Fire Cat - Amulet.xml", StringHash::KiStringHash("Fire Cat - Amulet") },
                { "Spells/Life/Guarded Heal.xml", StringHash::KiStringHash("Guarded Heal") }, { "ObjectData/Not A Spell.xml", 7 }, { "Spells/Broken One.xml", 100 },
                { "Spells/Broken Two.xml", 101 }, { "Spells/Garbage.xml", 102 } };
            return spells;
        }

        std::vector<uint8> Manifest(Locations const& locations)
        {
            return TemplateDumpFixtures::Manifest(_catalog, locations);
        }

        PropertyObjectPtr Effect(std::string const& className, int64 type, int32 param, std::string const& damage, int64 target, std::optional<PropertyValue::List> children = std::nullopt)
        {
            PropertyObjectPtr effect = PropertyObject::Create(_catalog, className);
            EXPECT_TRUE(effect) << className;
            EXPECT_EQ(effect->Set("m_effectType", type), PropertySetResult::Ok);
            EXPECT_EQ(effect->Set("m_effectParam", param), PropertySetResult::Ok);
            EXPECT_EQ(effect->Set("m_sDamageType", damage), PropertySetResult::Ok);
            EXPECT_EQ(effect->Set("m_effectTarget", target), PropertySetResult::Ok);
            if (children)
            {
                EXPECT_EQ(effect->Set("m_effectList", std::move(*children)), PropertySetResult::Ok);
            }
            return effect;
        }

        template<typename... Effects>
        static PropertyValue::List Choices(Effects... effects)
        {
            PropertyValue::List list;
            (list.emplace_back(std::move(effects)), ...);
            return list;
        }

        PropertyObjectPtr Guarded(PropertyObjectPtr inner)
        {
            PropertyObjectPtr element = PropertyObject::Create(_catalog, "class ConditionalSpellElement");
            EXPECT_EQ(element->Set("m_pEffect", std::move(inner)), PropertySetResult::Ok);
            PropertyValue::List elements;
            elements.emplace_back(std::move(element));
            PropertyObjectPtr conditional = Effect("class ConditionalSpellEffect", 0, 0, "", 11);
            EXPECT_EQ(conditional->Set("m_elements", std::move(elements)), PropertySetResult::Ok);
            return conditional;
        }

        std::vector<uint8> Spell(std::string const& className, std::string const& name, std::string const& school, int32 accuracy, uint8 rank, PropertyValue::List effects)
        {
            PropertyObjectPtr spell = PropertyObject::Create(_catalog, className);
            EXPECT_TRUE(spell) << className;
            EXPECT_EQ(spell->Set("m_name", name), PropertySetResult::Ok);
            EXPECT_EQ(spell->Set("m_sMagicSchoolName", school), PropertySetResult::Ok);
            EXPECT_EQ(spell->Set("m_sTypeName", std::string("Damage")), PropertySetResult::Ok);
            EXPECT_EQ(spell->Set("m_accuracy", accuracy), PropertySetResult::Ok);
            PropertyObjectPtr pips = PropertyObject::Create(_catalog, "class SpellRank");
            EXPECT_EQ(pips->Set("m_spellRank", rank), PropertySetResult::Ok);
            EXPECT_EQ(spell->Set("m_spellRank", std::move(pips)), PropertySetResult::Ok);
            EXPECT_EQ(spell->Set("m_effects", std::move(effects)), PropertySetResult::Ok);
            return WriteBind(spell);
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        TypeCatalogPtr _catalog;
        SpellMgr _spells;
        std::map<std::string, uint32> _ids;
    };
}

TEST_F(SpellMgrTest, EverySpellUnderSpellsIsReadAndFoundByIdByNameAndBySearch)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(_spells.Load(errors)) << errors.front();
    std::shared_ptr<SpellStore const> const spells = _spells.GetSpells();
    ASSERT_EQ(spells->Size(), 3u) << "a template outside Spells/ is not a spell the manager reads";
    SpellInfo const* const cat = spells->Find(StringHash::KiStringHash("Fire Cat"));
    ASSERT_NE(cat, nullptr);
    EXPECT_EQ(cat->Name, "Fire Cat");
    EXPECT_EQ(cat->Class, "class TieredSpellTemplate");
    EXPECT_EQ(cat->File, "Spells/Fire Cat.xml");
    EXPECT_EQ(cat->School, "Fire");
    EXPECT_EQ(cat->Accuracy, 75);
    EXPECT_EQ(cat->Pips.Rank, 1);
    EXPECT_EQ(spells->FindByName("Fire Cat"), cat) << "the name's hash is the id the client looks it up by";
    EXPECT_EQ(spells->FindByName("fIRE cAT"), cat) << "a name is found whatever its case";
    EXPECT_EQ(spells->FindByName("Fire Dog"), nullptr);
    std::vector<SpellInfo const*> const found = spells->Search("cat");
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0]->Name, "Fire Cat");
    EXPECT_EQ(found[1]->Name, "Fire Cat - Amulet");
}

TEST_F(SpellMgrTest, ASpellsEffectsAreATreeAndOnesBehindRequirementsAreMarked)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(_spells.Load(errors)) << errors.front();
    std::shared_ptr<SpellStore const> const spells = _spells.GetSpells();
    SpellInfo const* const amulet = spells->FindByName("Fire Cat - Amulet");
    ASSERT_NE(amulet, nullptr);
    ASSERT_EQ(amulet->Effects.size(), 1u);
    SpellEffectInfo const& random = amulet->Effects.front();
    EXPECT_EQ(random.Class, "class RandomSpellEffect");
    EXPECT_EQ(random.Target, 8);
    ASSERT_EQ(random.Effects.size(), 2u) << "a random effect holds the effects it chooses among";
    for (SpellEffectInfo const& choice : random.Effects)
    {
        EXPECT_EQ(choice.Type, 1);
        EXPECT_EQ(choice.DamageType, "Fire");
        EXPECT_EQ(choice.Target, 8);
        EXPECT_FALSE(choice.Conditional);
    }
    EXPECT_EQ(random.Effects[0].Param, 95);
    EXPECT_EQ(random.Effects[1].Param, 135);
    EXPECT_EQ(amulet->CountEffects(), 3u);

    SpellInfo const* const heal = spells->FindByName("Guarded Heal");
    ASSERT_NE(heal, nullptr);
    ASSERT_EQ(heal->Effects.size(), 1u);
    ASSERT_EQ(heal->Effects.front().Effects.size(), 1u) << "a conditional effect's element holds its effect";
    EXPECT_TRUE(heal->Effects.front().Effects.front().Conditional);
    EXPECT_EQ(heal->Effects.front().Effects.front().Type, 3);
    EXPECT_EQ(heal->Pips.Rank, 2);
}

TEST_F(SpellMgrTest, ASpellWhoseIdIsNotTheHashOfItsNameIsRefused)
{
    std::vector<SpellInfo> spells(2);
    spells[0].TemplateId = StringHash::KiStringHash("Fire Cat");
    spells[0].Name = "Fire Cat";
    spells[0].File = "Spells/Fire Cat.xml";
    spells[1].TemplateId = 12345;
    spells[1].Name = "Renamed";
    spells[1].File = "Spells/Old Name.xml";
    std::vector<std::string> errors;
    EXPECT_FALSE(SpellStore::Build(spells, errors));
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors.front(), fmt::format("Spells/Old Name.xml is listed as template 12345, but its name Renamed hashes to {}, the id the client looks it up by", StringHash::KiStringHash("Renamed")));
    spells.pop_back();
    errors.clear();
    EXPECT_TRUE(SpellStore::Build(spells, errors));
}

TEST_F(SpellMgrTest, AReloadThatMeetsFailingSpellsKeepsTheSetServingAndNamesEachWayTheyFail)
{
    sObjectTemplateMgr.RegisterReloadTargets();
    _spells.RegisterReloadTargets();
    ReloadOutcome const first = sReloadMgr.Reload(SpellMgr::Target);
    ASSERT_TRUE(first.Ok) << first.Errors.front();
    ASSERT_EQ(_spells.GetSpells()->Size(), 3u);

    Entries files = GoodSpells();
    files.emplace_back("Spells/Broken One.xml", Manifest({}));
    files.emplace_back("Spells/Broken Two.xml", Manifest({}));
    files.emplace_back("Spells/Garbage.xml", std::vector<uint8>{ 'n', 'o', 't', ' ', 'B', 'I', 'N', 'd' });
    WriteRoot(files);
    ASSERT_TRUE(sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget).Ok);
    ReloadOutcome const broken = sReloadMgr.Reload(SpellMgr::Target);
    EXPECT_FALSE(broken.Ok);
    ASSERT_EQ(broken.Errors.size(), 3u);
    EXPECT_TRUE(Holds(broken.Errors, "Spells/Broken One.xml is a class TemplateManifest, which is not a SpellTemplate; 1 more fail the same way, such as Spells/Broken Two.xml"));
    EXPECT_TRUE(Holds(broken.Errors, "Spells/Garbage.xml in Root.wad does not read"));
    EXPECT_TRUE(Holds(broken.Errors, "3 of the 6 spells under Spells/ fail to load"));
    EXPECT_EQ(_spells.GetSpells()->Size(), 3u) << "the set that was serving goes on serving";
    EXPECT_NE(_spells.GetSpells()->FindByName("Fire Cat"), nullptr);
}

TEST_F(SpellMgrTest, ASpellIsDescribedByItsSchoolRankAccuracyAndEffects)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(_spells.Load(errors)) << errors.front();
    SpellInfo const* const amulet = _spells.GetSpells()->FindByName("Fire Cat - Amulet");
    ASSERT_NE(amulet, nullptr);
    std::vector<std::string> const lines = amulet->Describe(_catalog.get());
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[0], fmt::format("Fire Cat - Amulet, template {}, Spells/Fire Cat - Amulet.xml, a SpellTemplate", StringHash::KiStringHash("Fire Cat - Amulet")));
    EXPECT_EQ(lines[1], "  school Fire, rank 1, accuracy 75%, type Damage");
    EXPECT_EQ(lines[2], "  kInvalidSpellEffect 0 to kEnemySingle, a RandomSpellEffect of:");
    EXPECT_EQ(lines[3], "    kDamage 95 Fire to kEnemySingle");
    EXPECT_EQ(lines[4], "    kDamage 135 Fire to kEnemySingle");

    SpellInfo const* const heal = _spells.GetSpells()->FindByName("Guarded Heal");
    ASSERT_NE(heal, nullptr);
    std::vector<std::string> const guarded = heal->Describe(_catalog.get());
    ASSERT_EQ(guarded.size(), 4u);
    EXPECT_EQ(guarded[3], "    kHeal 100 to kSelf, when its requirements are met");
}
