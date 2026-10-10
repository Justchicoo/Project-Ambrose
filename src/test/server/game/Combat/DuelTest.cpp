/*
 * Project Ambrose by Imjustchico
 * Tests a duel on a type dump the test writes with the duel classes laid out as the client's dump gives them: a wizard and a creature are seated on the first player and monster circles at the places SubCircle gives them, with each circle's angle and radius copied into them, and a side with no free circle is refused; the circle's WizardClientDuelBehavior carrying its Duel, and each CombatParticipant MSG_COMBATADD carries, decode back to identical objects; the circle's behaviors are filled with the duel and its state categories' start states under the global id it is given; a creature's health, level and school are read from its NPCBehaviorTemplate; and the duel manager finds a duel by participant and sigil and hands an ended one back once.
 */

#include "Duel.h"
#include "DuelMgr.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PropertyFlags.h"
#include "StringHash.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 4 | 8 | 16;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string const& container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Wire }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, std::vector<std::pair<std::string, std::string>> const& fields, std::vector<std::string> const& lists = {})
    {
        Json properties = Json::object();
        uint32 id = 0;
        for (auto const& [type, field] : fields)
        {
            bool const list = std::find(lists.begin(), lists.end(), field) != lists.end();
            properties[field] = Property(type, field, id++, list ? "List" : "Static");
        }
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    std::string Dump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), {});
        AddClass(classes, "class Vector3D", Json::array(), {});
        std::vector<std::pair<std::string, std::string>> const instance{ { "unsigned int", "m_behaviorTemplateNameID" } };
        AddClass(classes, "class BehaviorInstance", Json::array({ "PropertyClass" }), instance);
        AddClass(classes, "class ObjectStateBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }),
            { { "unsigned int", "m_behaviorTemplateNameID" }, { "unsigned int", "m_stateList" }, { "std::string", "m_stateSetOverride" } }, { "m_stateList" });
        std::vector<std::pair<std::string, std::string>> const duelBehavior{ { "unsigned int", "m_behaviorTemplateNameID" }, { "class SharedPointer<class Duel>", "m_pDuel" },
            { "unsigned int", "m_sigilTemplateID" } };
        AddClass(classes, "class DuelBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), duelBehavior);
        AddClass(classes, "class ClientDuelBehavior", Json::array({ "DuelBehavior", "BehaviorInstance", "PropertyClass" }), duelBehavior);
        AddClass(classes, "class WizardClientDuelBehavior", Json::array({ "ClientDuelBehavior", "DuelBehavior", "BehaviorInstance", "PropertyClass" }), duelBehavior);
        AddClass(classes, "class Duel", Json::array({ "PropertyClass" }),
            { { "class SharedPointer<class CombatParticipant>", "m_flatParticipantList" }, { "unsigned __int64", "m_duelID.m_full" }, { "class Vector3D", "m_position" },
                { "float", "m_yaw" }, { "int", "m_firstTeamToAct" }, { "bool", "m_bPVP" }, { "int", "m_roundNum" }, { "int", "m_originalFirstTeamToAct" } },
            { "m_flatParticipantList" });
        AddClass(classes, "class CombatParticipant", Json::array({ "PropertyClass" }),
            { { "unsigned __int64", "m_ownerID.m_full" }, { "unsigned __int64", "m_templateID.m_full" }, { "bool", "m_isPlayer" }, { "unsigned __int64", "m_zoneID.m_full" },
                { "int", "m_teamID" }, { "int", "m_primaryMagicSchoolID" }, { "int", "m_originalTeam" }, { "int", "m_playerHealth" }, { "int", "m_maxPlayerHealth" },
                { "int", "m_curMaxHP" }, { "float", "m_rotation" }, { "float", "m_radius" }, { "int", "m_subcircle" }, { "unsigned int", "m_isMonster" }, { "int", "m_mobLevel" } });
        AddClass(classes, "class Circle", Json::array({ "PropertyClass" }), { { "class SharedPointer<class BehaviorInstance>", "m_inactiveBehaviors" } }, { "m_inactiveBehaviors" });
        AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), { { "std::string", "m_behaviorName" } });
        AddClass(classes, "class NPCBehaviorTemplate", Json::array({ "BehaviorTemplate", "PropertyClass" }),
            { { "std::string", "m_behaviorName" }, { "int", "m_nStartingHealth" }, { "int", "m_nLevel" }, { "std::string", "m_schoolOfFocus" } });
        AddClass(classes, "class Mob", Json::array({ "PropertyClass" }), { { "class BehaviorTemplate*", "m_behaviors" } }, { "m_behaviors" });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    SigilInfo EightCircles()
    {
        SigilInfo sigil;
        sigil.TemplateId = StringHash::KiStringHash("TestRingOfEight");
        sigil.Name = "TestRingOfEight";
        sigil.Combat = true;
        for (int index = 0; index < 4; ++index)
            sigil.Circles.push_back({ "MonsterCircle", "", 144.0f - 36.0f * static_cast<float>(index), 600.0f });
        for (int index = 0; index < 4; ++index)
            sigil.Circles.push_back({ "PlayerCircle", "", -36.0f - 36.0f * static_cast<float>(index), 600.0f });
        return sigil;
    }

    DuelCombatant Wizard()
    {
        DuelCombatant wizard;
        wizard.OwnerId = 0x2000000000001ull;
        wizard.TemplateId = 1;
        wizard.IsPlayer = true;
        wizard.SchoolId = 72;
        wizard.Health = 415;
        wizard.MaxHealth = 500;
        wizard.Level = 5;
        return wizard;
    }

    DuelCombatant Creature()
    {
        DuelCombatant creature;
        creature.OwnerId = 0x3000000000002ull;
        creature.TemplateId = 4242;
        creature.SchoolId = 78;
        creature.Health = 55;
        creature.MaxHealth = 55;
        creature.Level = 1;
        return creature;
    }

    class DuelTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _registry = std::make_unique<TypeRegistry>();
            ASSERT_TRUE(_registry->LoadFromText(Dump(), "duel.json")) << _registry->GetErrors().front();
            _catalog = _registry->GetCatalog();
        }

        Duel Seated()
        {
            Duel duel(7, 41, { 1000.0f, -2000.0f, 50.0f }, 0.5f, EightCircles());
            std::string problem;
            EXPECT_TRUE(duel.Seat(Wizard(), problem)) << problem;
            EXPECT_TRUE(duel.Seat(Creature(), problem)) << problem;
            return duel;
        }

        std::unique_ptr<TypeRegistry> _registry;
        TypeCatalogPtr _catalog;
    };
}

TEST_F(DuelTest, AWizardAndACreatureSitOnTheFirstCircleOfTheirSidesWhereSubCircleSaysAndAFullSideIsRefused)
{
    Duel duel = Seated();
    ASSERT_EQ(duel.GetParticipants().size(), 2u);
    DuelParticipant const& wizard = duel.GetParticipants()[0];
    EXPECT_EQ(wizard.Circle, 4) << "the first player circle follows the four monster circles";
    EXPECT_EQ(wizard.Team, Duel::PlayerTeam);
    EXPECT_FLOAT_EQ(wizard.Rotation, -36.0f);
    EXPECT_FLOAT_EQ(wizard.Radius, 600.0f);
    EXPECT_NEAR(wizard.Place.Position.X, 1256.908f, 0.01f);
    EXPECT_NEAR(wizard.Place.Position.Y, -2542.216f, 0.01f);
    EXPECT_NEAR(wizard.Place.Yaw, 2.69911f, 0.0001f);
    DuelParticipant const& creature = duel.GetParticipants()[1];
    EXPECT_EQ(creature.Circle, 0);
    EXPECT_EQ(creature.Team, Duel::MonsterTeam);
    EXPECT_NEAR(creature.Place.Position.X, 743.092f, 0.01f);
    EXPECT_NEAR(creature.Place.Position.Y, -1457.784f, 0.01f);
    EXPECT_EQ(duel.FindParticipant(Creature().OwnerId), &creature);

    std::string problem;
    for (int seat = 0; seat < 3; ++seat)
        EXPECT_TRUE(duel.Seat(Wizard(), problem)) << problem;
    EXPECT_FALSE(duel.NextSeat(true).has_value());
    EXPECT_FALSE(duel.Seat(Wizard(), problem));
    EXPECT_NE(problem.find("PlayerCircle"), std::string::npos) << problem;
}

TEST_F(DuelTest, TheCirclesDuelBehaviorAndEachParticipantDecodeBackToIdenticalObjects)
{
    Duel duel = Seated();
    duel.SetId(0x4000000000099ull);
    PropertyObjectPtr const behavior = PropertyObject::Create(_catalog, Duel::BehaviorClass);
    ASSERT_TRUE(behavior);
    std::string problem;
    ASSERT_TRUE(duel.FillBehavior(*behavior, problem)) << problem;

    SerializerOptions options;
    options.Mask = SerializerOptions::PublicMask;
    EncodeResult const encoded = ObjectSerializer::Encode(behavior.get(), options);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    DecodeResult const decoded = ObjectSerializer::Decode(_catalog, encoded.Bytes, options);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_TRUE(decoded.Object);
    EXPECT_TRUE(*decoded.Object == *behavior);
    PropertyObject const* const carried = decoded.Object->Get("m_pDuel")->AsObject();
    ASSERT_NE(carried, nullptr);
    EXPECT_EQ(*carried->Get("m_duelID.m_full")->GetIf<uint64>(), 0x4000000000099ull) << "the duel id is the circle's global id";
    EXPECT_FLOAT_EQ(*carried->Get("m_yaw")->GetIf<float>(), 0.5f);
    EXPECT_EQ(*decoded.Object->Get("m_sigilTemplateID")->GetIf<uint32>(), StringHash::KiStringHash("TestRingOfEight"));

    ASSERT_TRUE(duel.EncodeParticipants(_catalog, problem)) << problem;
    ASSERT_EQ(duel.GetEncodedParticipants().size(), 2u);
    ObjectField const* const field = ObjectFields::Find("MSG_COMBATADD", "ParticipantData");
    ASSERT_NE(field, nullptr);
    SerializerOptions participantOptions;
    participantOptions.Mask = PropertyFlags::Bit(PropertyFlag::Public);
    for (std::size_t index = 0; index < 2; ++index)
    {
        std::string const& data = duel.GetEncodedParticipants()[index];
        std::vector<uint8> const bytes(data.begin(), data.end());
        DecodeResult const participant = ObjectSerializer::DecodeField(_catalog, *field, bytes, participantOptions);
        ASSERT_TRUE(participant.Ok()) << participant.Detail;
        PropertyObjectPtr const expected = duel.BuildParticipant(_catalog, duel.GetParticipants()[index], problem);
        ASSERT_TRUE(expected) << problem;
        EXPECT_TRUE(*participant.Object == *expected) << index;
    }
    DecodeResult const creature = ObjectSerializer::DecodeField(_catalog, *field,
        std::vector<uint8>(duel.GetEncodedParticipants()[1].begin(), duel.GetEncodedParticipants()[1].end()), participantOptions);
    EXPECT_EQ(*creature.Object->Get("m_ownerID.m_full")->GetIf<uint64>(), Creature().OwnerId);
    EXPECT_EQ(*creature.Object->Get("m_playerHealth")->GetIf<int32>(), 55);
    EXPECT_EQ(*creature.Object->Get("m_isMonster")->GetIf<uint32>(), 1u);
    EXPECT_FLOAT_EQ(*creature.Object->Get("m_rotation")->GetIf<float>(), 144.0f) << "the client places a participant from its own rotation and radius";
}

TEST_F(DuelTest, TheCircleIsGivenItsDuelAndItsStartStatesUnderTheGlobalIdItIsHanded)
{
    Duel duel = Seated();
    PropertyObjectPtr const circle = PropertyObject::Create(_catalog, "class Circle");
    ASSERT_TRUE(circle);
    PropertyValue::List behaviors;
    behaviors.emplace_back(PropertyObject::Create(_catalog, "class ObjectStateBehavior"));
    behaviors.emplace_back(PropertyObjectPtr());
    behaviors.emplace_back(PropertyObject::Create(_catalog, Duel::BehaviorClass));
    ASSERT_EQ(circle->Set("m_inactiveBehaviors", std::move(behaviors)), PropertySetResult::Ok);
    std::string problem;
    ASSERT_TRUE(duel.DecorateCircle(*circle, 0x4000000000123ull, problem)) << problem;
    EXPECT_EQ(duel.GetId(), 0x4000000000123ull);
    PropertyValue::List const& filled = *circle->Get("m_inactiveBehaviors")->GetList();
    PropertyValue::List const& states = *filled[0].AsObject()->Get("m_stateList")->GetList();
    ASSERT_EQ(states.size(), 2u);
    EXPECT_EQ(*states[0].GetIf<uint32>(), StringHash::StringId("OnAdd"));
    EXPECT_EQ(*states[1].GetIf<uint32>(), StringHash::StringId("NotInteracting"));
    PropertyObject const* const carried = filled[2].AsObject()->Get("m_pDuel")->AsObject();
    ASSERT_NE(carried, nullptr);
    EXPECT_EQ(*carried->Get("m_duelID.m_full")->GetIf<uint64>(), 0x4000000000123ull);

    PropertyObjectPtr const bare = PropertyObject::Create(_catalog, "class Circle");
    EXPECT_FALSE(duel.DecorateCircle(*bare, 1, problem));
    EXPECT_NE(problem.find(Duel::BehaviorClass), std::string::npos) << problem;
}

TEST_F(DuelTest, ACreaturesHealthLevelAndSchoolComeFromItsNpcBehaviorTemplate)
{
    PropertyObjectPtr const mob = PropertyObject::Create(_catalog, "class Mob");
    PropertyObjectPtr npc = PropertyObject::Create(_catalog, Duel::NpcBehaviorClass);
    ASSERT_TRUE(mob && npc);
    ASSERT_EQ(npc->Set("m_nStartingHealth", int32{ 80 }), PropertySetResult::Ok);
    ASSERT_EQ(npc->Set("m_nLevel", int32{ 3 }), PropertySetResult::Ok);
    ASSERT_EQ(npc->Set("m_schoolOfFocus", std::string("Death")), PropertySetResult::Ok);
    PropertyValue::List list;
    list.emplace_back(PropertyObject::Create(_catalog, "class BehaviorTemplate"));
    list.emplace_back(std::move(npc));
    ASSERT_EQ(mob->Set("m_behaviors", std::move(list)), PropertySetResult::Ok);
    std::string problem;
    std::optional<CreatureCombatStats> const stats = Duel::ReadCreature(*mob, problem);
    ASSERT_TRUE(stats) << problem;
    EXPECT_EQ(stats->Health, 80);
    EXPECT_EQ(stats->Level, 3);
    EXPECT_EQ(stats->School, "Death");
    EXPECT_EQ(static_cast<uint32>(stats->SchoolId), StringHash::StringId("Death"));

    PropertyObjectPtr const plain = PropertyObject::Create(_catalog, "class Mob");
    EXPECT_FALSE(Duel::ReadCreature(*plain, problem));
    EXPECT_NE(problem.find(Duel::NpcBehaviorClass), std::string::npos) << problem;
}

TEST_F(DuelTest, TheManagerFindsADuelByParticipantAndSigilAndHandsAnEndedOneBackOnce)
{
    sDuelMgr.Clear();
    Duel duel = Seated();
    duel.SetId(0x4000000000200ull);
    sDuelMgr.Add(std::move(duel));
    ASSERT_NE(sDuelMgr.FindByParticipant(Wizard().OwnerId), nullptr);
    EXPECT_EQ(sDuelMgr.FindByParticipant(Creature().OwnerId)->GetId(), 0x4000000000200ull);
    EXPECT_NE(sDuelMgr.FindBySigil(7, 41), nullptr);
    EXPECT_EQ(sDuelMgr.FindBySigil(8, 41), nullptr);
    EXPECT_EQ(sDuelMgr.InMap(7).size(), 1u);
    EXPECT_TRUE(sDuelMgr.TakeEnded().empty());

    EXPECT_EQ(sDuelMgr.EndFor(Wizard().OwnerId, Duel::PlayerTeam), 0x4000000000200ull);
    EXPECT_EQ(sDuelMgr.FindByParticipant(Wizard().OwnerId), nullptr) << "an ended duel holds no one";
    ASSERT_NE(sDuelMgr.Find(0x4000000000200ull), nullptr) << "an ended duel stays until the sessions shown it are told";
    EXPECT_TRUE(sDuelMgr.Find(0x4000000000200ull)->IsEnded());
    std::vector<Duel> const ended = sDuelMgr.TakeEnded();
    ASSERT_EQ(ended.size(), 1u);
    EXPECT_EQ(ended.front().GetWinningTeam(), Duel::PlayerTeam);
    EXPECT_TRUE(sDuelMgr.TakeEnded().empty());
    EXPECT_EQ(sDuelMgr.GetCount(), 0u);
}
