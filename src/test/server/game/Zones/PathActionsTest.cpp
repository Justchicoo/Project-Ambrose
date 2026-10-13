/*
 * Project Ambrose by Imjustchico
 * Tests reading a PathBehaviorTemplate's path actions: each action class the server runs is read with its path, node, priority, chance and its own fields, a class it does not run is skipped, the actions keyed to a node come back highest priority first, and a walking template carries its actions and its chance and time to pause.
 */

#include "PathActions.h"
#include "PathWalkers.h"
#include "TemplateDumpFixtures.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

namespace
{
    using TemplateDumpFixtures::AddClass;
    using TemplateDumpFixtures::Json;
    using TemplateDumpFixtures::Property;

    Json ActionProperties(Json extra)
    {
        Json properties = { { "m_nPathID", Property("gid", "m_nPathID", 0) }, { "m_nNodeID", Property("int", "m_nNodeID", 1) },
            { "m_nPriority", Property("int", "m_nPriority", 2) }, { "m_nChance", Property("int", "m_nChance", 3) } };
        for (auto& [name, property] : extra.items())
            properties[name] = property;
        return properties;
    }

    TypeCatalogPtr Catalog()
    {
        Json classes = Json::object();
        Json const action = Json::array({ "PathBehaviorTemplate::Action", "PropertyClass" });
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "class PathBehaviorTemplate::Action", Json::array({ "PropertyClass" }), ActionProperties(Json::object()));
        AddClass(classes, "class PauseAction", action,
            ActionProperties({ { "m_fMinPauseTime", Property("float", "m_fMinPauseTime", 4) }, { "m_fMaxPauseTime", Property("float", "m_fMaxPauseTime", 5) } }));
        AddClass(classes, "class PathActionEmote", action, ActionProperties({ { "m_sEmoteName", Property("std::string", "m_sEmoteName", 4) },
            { "m_fMinTime", Property("float", "m_fMinTime", 5) }, { "m_fMaxTime", Property("float", "m_fMaxTime", 6) } }));
        AddClass(classes, "class PathActionDespawn", action, ActionProperties(Json::object()));
        AddClass(classes, "class PathActionZoneEvent", action, ActionProperties({ { "m_zoneEvent", Property("std::string", "m_zoneEvent", 4) } }));
        AddClass(classes, "class PathActionChangeDirection", action, ActionProperties(Json::object()));
        AddClass(classes, "class PathActionChangePath", action, ActionProperties({ { "m_nDestinationPathID", Property("gid", "m_nDestinationPathID", 4) },
            { "m_nDestinationNodeID", Property("int", "m_nDestinationNodeID", 5) } }));
        AddClass(classes, "class TeleportToNode", action, ActionProperties({ { "m_nDestinationNodeID", Property("int", "m_nDestinationNodeID", 4) },
            { "m_bContinuePathing", Property("bool", "m_bContinuePathing", 5) } }));
        AddClass(classes, "class PathActionEnterState", action, ActionProperties({ { "m_sStateName", Property("std::string", "m_sStateName", 4) } }));
        AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), { { "m_behaviorName", Property("std::string", "m_behaviorName", 0) } });
        AddClass(classes, "class PathBehaviorTemplate", Json::array({ "BehaviorTemplate", "PropertyClass" }), { { "m_behaviorName", Property("std::string", "m_behaviorName", 0) },
            { "m_pathID", Property("gid", "m_pathID", 1) }, { "m_nPathDirection", Property("int", "m_nPathDirection", 2) },
            { "m_actionList", Property("class SharedPointer<class PathBehaviorTemplate::Action>", "m_actionList", 3, "List") },
            { "m_pauseChance", Property("unsigned int", "m_pauseChance", 4) }, { "m_timeToPause", Property("float", "m_timeToPause", 5) } });
        AddClass(classes, "class PathMovementBehaviorTemplate", Json::array({ "BehaviorTemplate", "PropertyClass" }), { { "m_behaviorName", Property("std::string", "m_behaviorName", 0) },
            { "m_movementSpeed", Property("float", "m_movementSpeed", 1) }, { "m_movementScale", Property("float", "m_movementScale", 2) } });
        AddClass(classes, "class WizGameObjectTemplate", Json::array({ "PropertyClass" }),
            { { "m_behaviors", Property("class SharedPointer<class BehaviorTemplate>", "m_behaviors", 0, "List") } });
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump(), "paths.json"))
            << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    PropertyObjectPtr Action(TypeCatalogPtr const& catalog, std::string const& className, uint64 path, int32 node, int32 priority, int32 chance)
    {
        PropertyObjectPtr action = PropertyObject::Create(catalog, className);
        EXPECT_TRUE(action) << className;
        EXPECT_EQ(action->Set("m_nPathID", path), PropertySetResult::Ok);
        EXPECT_EQ(action->Set("m_nNodeID", node), PropertySetResult::Ok);
        EXPECT_EQ(action->Set("m_nPriority", priority), PropertySetResult::Ok);
        EXPECT_EQ(action->Set("m_nChance", chance), PropertySetResult::Ok);
        return action;
    }

    PropertyObjectPtr PathBehavior(TypeCatalogPtr const& catalog)
    {
        PropertyObjectPtr pause = Action(catalog, "class PauseAction", 7828185, 6, 50, 100);
        EXPECT_EQ(pause->Set("m_fMinPauseTime", 1.0f), PropertySetResult::Ok);
        EXPECT_EQ(pause->Set("m_fMaxPauseTime", 2.0f), PropertySetResult::Ok);
        PropertyObjectPtr emote = Action(catalog, "class PathActionEmote", 7828185, 6, 100, 100);
        EXPECT_EQ(emote->Set("m_sEmoteName", std::string("Jump")), PropertySetResult::Ok);
        EXPECT_EQ(emote->Set("m_fMinTime", 1.0f), PropertySetResult::Ok);
        EXPECT_EQ(emote->Set("m_fMaxTime", 2.0f), PropertySetResult::Ok);
        PropertyObjectPtr event = Action(catalog, "class PathActionZoneEvent", 7828185, 8, 100, 100);
        EXPECT_EQ(event->Set("m_zoneEvent", std::string("TEST1")), PropertySetResult::Ok);
        PropertyObjectPtr change = Action(catalog, "class PathActionChangePath", 7828185, 3, 100, 50);
        EXPECT_EQ(change->Set("m_nDestinationPathID", uint64{ 8104574 }), PropertySetResult::Ok);
        EXPECT_EQ(change->Set("m_nDestinationNodeID", 2), PropertySetResult::Ok);
        PropertyObjectPtr teleport = Action(catalog, "class TeleportToNode", 7828185, 4, 100, 100);
        EXPECT_EQ(teleport->Set("m_nDestinationNodeID", 0), PropertySetResult::Ok);
        EXPECT_EQ(teleport->Set("m_bContinuePathing", false), PropertySetResult::Ok);
        PropertyObjectPtr state = Action(catalog, "class PathActionEnterState", 7828185, 3, 100, 50);

        PropertyValue::List list;
        list.emplace_back(std::move(pause));
        list.emplace_back(std::move(emote));
        list.emplace_back(std::move(event));
        list.emplace_back(Action(catalog, "class PathActionDespawn", 7828185, 14, 100, 100));
        list.emplace_back(Action(catalog, "class PathActionChangeDirection", 7828185, 11, 100, 35));
        list.emplace_back(std::move(change));
        list.emplace_back(std::move(teleport));
        list.emplace_back(std::move(state));
        PropertyObjectPtr behavior = PropertyObject::Create(catalog, "class PathBehaviorTemplate");
        EXPECT_EQ(behavior->Set("m_behaviorName", std::string("PathBehavior")), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_actionList", std::move(list)), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_pauseChance", uint32{ 10 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_timeToPause", 6.0f), PropertySetResult::Ok);
        return behavior;
    }
}

TEST(PathActionsTest, ReadsEveryActionClassTheServerRunsAndSkipsTheRest)
{
    TypeCatalogPtr const catalog = Catalog();
    ASSERT_TRUE(catalog);
    std::shared_ptr<PathActions const> const actions = PathActions::Read(*PathBehavior(catalog));
    ASSERT_TRUE(actions);

    std::vector<PathAction const*> const atSix = actions->At(7828185, 6);
    ASSERT_EQ(atSix.size(), 2u);
    EXPECT_EQ(atSix[0]->Kind, PathActionKind::Emote) << "priority 100 runs before priority 50";
    EXPECT_EQ(atSix[0]->Name, "Jump");
    EXPECT_FLOAT_EQ(atSix[0]->MinSeconds, 1.0f);
    EXPECT_FLOAT_EQ(atSix[0]->MaxSeconds, 2.0f);
    EXPECT_EQ(atSix[1]->Kind, PathActionKind::Pause);
    EXPECT_FLOAT_EQ(atSix[1]->MaxSeconds, 2.0f);

    ASSERT_EQ(actions->At(7828185, 8).size(), 1u);
    EXPECT_EQ(actions->At(7828185, 8)[0]->Name, "TEST1");
    ASSERT_EQ(actions->At(7828185, 14).size(), 1u);
    EXPECT_EQ(actions->At(7828185, 14)[0]->Kind, PathActionKind::Despawn);
    ASSERT_EQ(actions->At(7828185, 11).size(), 1u);
    EXPECT_EQ(actions->At(7828185, 11)[0]->Chance, 35);

    std::vector<PathAction const*> const atThree = actions->At(7828185, 3);
    ASSERT_EQ(atThree.size(), 1u) << "PathActionEnterState is not run, so it is not read";
    EXPECT_EQ(atThree[0]->Kind, PathActionKind::ChangePath);
    EXPECT_EQ(atThree[0]->DestinationPathId, 8104574u);
    EXPECT_EQ(atThree[0]->DestinationNodeId, 2);

    ASSERT_EQ(actions->At(7828185, 4).size(), 1u);
    EXPECT_EQ(actions->At(7828185, 4)[0]->Kind, PathActionKind::TeleportToNode);
    EXPECT_FALSE(actions->At(7828185, 4)[0]->ContinuePathing);

    EXPECT_TRUE(actions->At(7828185, 5).empty());
    EXPECT_TRUE(actions->At(8104574, 6).empty()) << "an action fires only on the path it names";
}

TEST(PathActionsTest, AWalkingTemplateCarriesItsActionsAndItsChanceAndTimeToPause)
{
    TypeCatalogPtr const catalog = Catalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr movement = PropertyObject::Create(catalog, "class PathMovementBehaviorTemplate");
    ASSERT_EQ(movement->Set("m_movementSpeed", 100.0f), PropertySetResult::Ok);
    ASSERT_EQ(movement->Set("m_movementScale", 1.0f), PropertySetResult::Ok);
    PropertyValue::List behaviors;
    behaviors.emplace_back(PathBehavior(catalog));
    behaviors.emplace_back(std::move(movement));
    PropertyObjectPtr object = PropertyObject::Create(catalog, "class WizGameObjectTemplate");
    ASSERT_EQ(object->Set("m_behaviors", std::move(behaviors)), PropertySetResult::Ok);
    ObjectTemplate walker;
    walker.TemplateId = 1521122;
    walker.Object = std::move(object);

    std::optional<PathWalkBehavior> const behavior = PathWalkers::BehaviorOf(walker);
    ASSERT_TRUE(behavior);
    EXPECT_EQ(behavior->PauseChance, 10u);
    EXPECT_FLOAT_EQ(behavior->PauseSeconds, 6.0f);
    ASSERT_TRUE(behavior->Actions);
    EXPECT_EQ(behavior->Actions->At(7828185, 6).size(), 2u);
}

TEST(PathActionsTest, AnEmptyActionListGivesNoActions)
{
    TypeCatalogPtr const catalog = Catalog();
    ASSERT_TRUE(catalog);
    PropertyObjectPtr behavior = PropertyObject::Create(catalog, "class PathBehaviorTemplate");
    ASSERT_TRUE(behavior);
    EXPECT_FALSE(PathActions::Read(*behavior));
}
