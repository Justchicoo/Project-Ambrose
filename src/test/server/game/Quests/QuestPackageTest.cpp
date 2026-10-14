/*
 * Project Ambrose by Imjustchico
 * Tests the CSR package quest reader on classes and quest files the test invents: a quest's keys, its RequirementList, encoded so it decodes back to the list it was and NULL in the script when the quest lists none or an empty list, with an unknown class or property in it failing the quest, goals, adjectives, client tags, tally, logic and Prep dialog with its madlib become quest table rows, logic waiting on two goals is attached to both, dialog tags and results the tables cannot hold are counted rather than failing the quest, unknown classes in results are only counted while one in a goal fails it, a persona named by its _Persona template is read as the object it names, a missing actor template, a missing locale key, a name another source holds and a goal the logic names but the quest lacks each fail their quest, and the script replaces only the rows the package's source marks.
 */

#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "QuestPackage.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;
    constexpr uint32 ActorId = 5001;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static")
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", type.ends_with('*') }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json bases, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
        };
        auto const props = [](std::vector<std::tuple<std::string, std::string, std::string>> const& list)
        {
            Json json = Json::object();
            uint32 id = 0;
            for (auto const& [type, name, container] : list)
                json[name] = Property(type, name, id++, container);
            return json;
        };
        add("class PropertyClass", Json::array(), Json::object());
        add("enum GoalTemplate::GOAL_TYPE", Json::array(), Json::object());
        add("class Requirement", Json::array({ "PropertyClass" }), Json::object());
        add("class RequirementList", Json::array({ "Requirement", "PropertyClass" }), props({ { "class Requirement*", "m_requirements", "List" } }));
        add("class ReqHasEntry", Json::array({ "Requirement", "PropertyClass" }), props({ { "std::string", "m_entryName", "Static" } }));
        add("class Result", Json::array({ "PropertyClass" }), Json::object());
        add("class ResultList", Json::array({ "PropertyClass" }), props({ { "class Result*", "m_results", "List" } }));
        add("class MadlibArg", Json::array({ "PropertyClass" }), props({ { "std::string", "m_madlibToken", "Static" } }));
        add("MadlibArgT<std::string>", Json::array({ "MadlibArg", "PropertyClass" }), props({ { "std::string", "m_madlibToken", "Static" }, { "std::string", "m_madlibArgument", "Static" } }));
        add("class MadlibBlock", Json::array({ "PropertyClass" }), props({ { "class MadlibArg*", "m_madlibs", "List" }, { "std::string", "m_blockToken", "Static" } }));
        add("class ActorMadlib", Json::array({ "PropertyClass" }), props({ { "class MadlibBlock*", "m_madlibBlock", "Static" }, { "unsigned int", "m_index", "Static" } }));
        add("class ActorDialogEntry", Json::array({ "PropertyClass" }), props({ { "std::string", "m_dialog", "Static" }, { "std::string", "m_picture", "Static" },
            { "std::string", "m_soundFile", "Static" }, { "std::string", "m_action", "Static" }, { "std::string", "m_dialogEvent", "Static" },
            { "unsigned int", "m_actorTemplateID", "Static" }, { "std::string", "m_dialogAnimationList", "List" } }));
        add("class ActorDialog", Json::array({ "PropertyClass" }), props({ { "std::string", "m_dialogTag", "Static" }, { "class ActorDialogEntry*", "m_dialogEntries", "List" },
            { "class ActorMadlib*", "m_madlibs", "List" } }));
        add("class ActorDialogList", Json::array({ "PropertyClass" }), props({ { "class ActorDialog*", "m_dialogs", "List" } }));
        add("class GoalCompleteLogic", Json::array({ "PropertyClass" }), props({ { "std::string", "m_goalsAND", "List" }, { "std::string", "m_goalsOR", "List" },
            { "std::string", "m_goalsToAdd", "List" }, { "bool", "m_completeQuest", "Static" } }));
        add("class TallyCounterTemplate", Json::array({ "PropertyClass" }), props({ { "float", "m_percentChance", "Static" }, { "std::string", "m_descriptor", "Static" },
            { "std::string", "m_descriptor2", "Static" }, { "int", "m_count", "Static" } }));
        std::vector<std::tuple<std::string, std::string, std::string>> goal{ { "std::string", "m_goalName", "Static" }, { "unsigned int", "m_goalNameID", "Static" },
            { "std::string", "m_goalTitle", "Static" }, { "std::string", "m_locationName", "Static" }, { "std::string", "m_destinationZone", "Static" },
            { "std::string", "m_clientTags", "List" }, { "class ActorDialogList*", "m_dialogList", "Static" }, { "enum GoalTemplate::GOAL_TYPE", "m_goalType", "Static" },
            { "class ResultList*", "m_completeResults", "Static" }, { "class TallyCounterTemplate*", "m_tallyCounter", "Static" } };
        Json base = props(goal);
        base["m_goalType"]["enum_options"] = Json{ { "GOAL_TYPE_UNKNOWN", 0 }, { "GOAL_TYPE_BOUNTY", 1 }, { "GOAL_TYPE_PERSONA", 4 }, { "GOAL_TYPE_WAYPOINT", 5 },
            { "GOAL_TYPE_COLLECTAFTERBOUNTY", 15 } };
        add("class GoalTemplate", Json::array({ "PropertyClass" }), base);
        auto const derived = [&](std::string const& name, std::vector<std::tuple<std::string, std::string, std::string>> extra)
        {
            std::vector<std::tuple<std::string, std::string, std::string>> all = goal;
            all.insert(all.end(), extra.begin(), extra.end());
            Json json = props(all);
            json["m_goalType"]["enum_options"] = base["m_goalType"]["enum_options"];
            add(name, Json::array({ "GoalTemplate", "PropertyClass" }), json);
        };
        derived("class PersonaGoalTemplate", { { "std::string", "m_personaName", "Static" }, { "bool", "m_usePatron", "Static" } });
        derived("class WaypointGoalTemplate", { { "bool", "m_zoneEntry", "Static" }, { "std::string", "m_zoneTag", "Static" } });
        derived("class BountyGoalTemplate", { { "std::string", "m_npcAdjectives", "List" }, { "int", "m_bountyTotal", "Static" } });
        add("class QuestTemplate", Json::array({ "PropertyClass" }), props({ { "std::string", "m_questName", "Static" }, { "unsigned int", "m_questNameID", "Static" },
            { "std::string", "m_questTitle", "Static" }, { "std::string", "m_questInfo", "Static" }, { "std::string", "m_startGoals", "List" },
            { "class GoalTemplate*", "m_goals", "List" }, { "class ResultList*", "m_endResults", "Static" }, { "class RequirementList*", "m_requirements", "Static" },
            { "class GoalCompleteLogic*", "m_goalLogic", "List" }, { "int", "m_questLevel", "Static" }, { "int", "m_questRepeat", "Static" }, { "bool", "m_mainline", "Static" },
            { "class ActorDialogList*", "m_dialogList", "Static" } }));
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "quests.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    std::string QuestFile(std::string const& name, std::string const& goalExtra = {}, std::string const& logicMember = "Talk")
    {
        return "\xEF\xBB\xBF<Objects>\r\n"
            "  <Class Name=\"class QuestTemplate\">\r\n"
            "    <m_questName>" + name + "</m_questName>\r\n"
            "    <m_questNameID>77</m_questNameID>\r\n"
            "    <m_questTitle>TestTitle_" + name + "</m_questTitle>\r\n"
            "    <m_questLevel>4</m_questLevel>\r\n"
            "    <m_questRepeat>-1</m_questRepeat>\r\n"
            "    <m_mainline>true</m_mainline>\r\n"
            "    <m_startGoals>Talk</m_startGoals>\r\n"
            "    <m_requirements><Class Name=\"class RequirementList\"><m_requirements><Class Name=\"class ReqHasEntry\"><m_entryName>E</m_entryName></Class></m_requirements></Class></m_requirements>\r\n"
            "    <m_endResults><Class Name=\"class ResultList\"><m_results><Class Name=\"class ResInventedReward\"><m_amount>5</m_amount></Class></m_results></Class></m_endResults>\r\n"
            "    <m_dialogList><Class Name=\"class ActorDialogList\">\r\n"
            "      <m_dialogs><Class Name=\"class ActorDialog\"><m_dialogTag>Prep</m_dialogTag>\r\n"
            "        <m_dialogEntries><Class Name=\"class ActorDialogEntry\"><m_dialog>TestLine_0</m_dialog><m_actorTemplateID>5001</m_actorTemplateID>"
            "<m_dialogAnimationList>Wave</m_dialogAnimationList></Class></m_dialogEntries>\r\n"
            "        <m_madlibs><Class Name=\"class ActorMadlib\"><m_index>0</m_index><m_madlibBlock><Class Name=\"class MadlibBlock\">"
            "<m_madlibs><Class Name=\"MadlibArgT&lt;std::string&gt;\"><m_madlibToken>Name</m_madlibToken><m_madlibArgument>TestNpc</m_madlibArgument></Class></m_madlibs>"
            "</Class></m_madlibBlock></Class></m_madlibs>\r\n"
            "      </Class></m_dialogs>\r\n"
            "      <m_dialogs><Class Name=\"class ActorDialog\"><m_dialogTag>QuestInfo</m_dialogTag></Class></m_dialogs>\r\n"
            "    </Class></m_dialogList>\r\n"
            "    <m_goals><Class Name=\"class PersonaGoalTemplate\"><m_goalName>Talk</m_goalName><m_goalType>GOAL_TYPE_PERSONA</m_goalType>"
            "<m_personaName>TestNpc_Persona</m_personaName><m_clientTags>TagA</m_clientTags></Class></m_goals>\r\n"
            "    <m_goals><Class Name=\"class WaypointGoalTemplate\"><m_goalName>Walk</m_goalName><m_goalType>GOAL_TYPE_WAYPOINT</m_goalType>"
            "<m_destinationZone>Test/Zone</m_destinationZone><m_zoneEntry>true</m_zoneEntry></Class></m_goals>\r\n"
            "    <m_goals><Class Name=\"class BountyGoalTemplate\"><m_goalName>Hunt</m_goalName><m_goalType>GOAL_TYPE_BOUNTY</m_goalType>"
            "<m_npcAdjectives>Ghost</m_npcAdjectives><m_npcAdjectives>Wild</m_npcAdjectives><m_bountyTotal>3</m_bountyTotal>"
            "<m_tallyCounter><Class Name=\"class TallyCounterTemplate\"><m_descriptor>TestTally</m_descriptor><m_count>9</m_count></Class></m_tallyCounter></Class></m_goals>\r\n"
            + goalExtra +
            "    <m_goalLogic><Class Name=\"class GoalCompleteLogic\"><m_goalsAND>" + logicMember + "</m_goalsAND><m_goalsToAdd>Walk</m_goalsToAdd><m_goalsToAdd>Hunt</m_goalsToAdd></Class></m_goalLogic>\r\n"
            "    <m_goalLogic><Class Name=\"class GoalCompleteLogic\"><m_goalsAND>Walk</m_goalsAND><m_goalsAND>Hunt</m_goalsAND><m_completeQuest>true</m_completeQuest></Class></m_goalLogic>\r\n"
            "  </Class>\r\n"
            "</Objects>\r\n";
    }

    std::string WithRequirements(std::string file, std::string const& requirements)
    {
        std::size_t const start = file.find("    <m_requirements>");
        std::size_t const end = file.find('\n', start);
        return file.replace(start, end - start + 1, requirements.empty() ? std::string() : "    <m_requirements>" + requirements + "</m_requirements>\r\n");
    }

    QuestValidator::Context Install(std::vector<std::string> missingKeys = {})
    {
        return QuestValidator::Context::From({ QuestObjectRow{ ActorId, "TestNpc" } }, [missingKeys](std::string_view key)
        {
            return std::find(missingKeys.begin(), missingKeys.end(), key) == missingKeys.end();
        });
    }

    class QuestPackageTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
        }

        TypeCatalogPtr _catalog;
    };
}

TEST_F(QuestPackageTest, AQuestFileBecomesQuestTableRows)
{
    QuestPackageRead read = QuestPackage::Read(_catalog, { QuestPackageEntry{ "QuestData/T/TEST-001.xml", QuestFile("TEST-001") } });
    ASSERT_EQ(read.Quests.size(), 1u);
    QuestPackageQuest const& quest = read.Quests.front();
    EXPECT_TRUE(quest.IsClean()) << (quest.Problems.empty() ? std::string() : quest.Problems.front());
    ASSERT_EQ(quest.Rows.Quests.size(), 1u);
    QuestTemplateRow const& row = quest.Rows.Quests.front();
    EXPECT_EQ(row.Name, "TEST-001");
    EXPECT_EQ(row.NameId, 77u);
    EXPECT_EQ(row.TitleKey, "TestTitle_TEST-001");
    EXPECT_EQ(row.Level, 4u);
    EXPECT_EQ(row.Repeat, 0u);
    EXPECT_TRUE(row.Mainline);
    ASSERT_EQ(quest.Rows.StartGoals.size(), 1u);
    EXPECT_EQ(quest.Rows.StartGoals.front().Goal, "Talk");

    ASSERT_EQ(quest.Rows.Goals.size(), 3u);
    EXPECT_EQ(quest.Rows.Goals[0].Type, "persona");
    EXPECT_EQ(quest.Rows.Goals[0].PersonaName, "TestNpc_Persona");
    EXPECT_EQ(quest.Rows.Goals[1].Type, "waypoint");
    EXPECT_EQ(quest.Rows.Goals[1].DestinationZone, "Test/Zone");
    EXPECT_TRUE(quest.Rows.Goals[1].ZoneEntry);
    EXPECT_EQ(quest.Rows.Goals[2].Type, "bounty");
    EXPECT_EQ(quest.Rows.Goals[2].TallyCount, 3u);
    EXPECT_EQ(quest.Rows.Goals[2].TallyDescriptorKey, "TestTally");
    ASSERT_EQ(quest.Rows.Adjectives.size(), 2u);
    EXPECT_EQ(quest.Rows.Adjectives[1].Adjective, "Wild");
    EXPECT_EQ(quest.Rows.Adjectives[1].Position, 1u);
    ASSERT_EQ(quest.Rows.ClientTags.size(), 1u);
    EXPECT_EQ(quest.Rows.ClientTags.front().Tag, "TagA");

    ASSERT_EQ(quest.Rows.Logic.size(), 3u);
    EXPECT_EQ(quest.Rows.Logic[0].Goal, "Talk");
    EXPECT_FALSE(quest.Rows.Logic[0].CompleteQuest);
    EXPECT_EQ(quest.Rows.Logic[1].Goal, "Hunt");
    EXPECT_EQ(quest.Rows.Logic[2].Goal, "Walk");
    EXPECT_TRUE(quest.Rows.Logic[2].CompleteQuest);
    EXPECT_EQ(quest.Rows.Logic[1].Position, 1u);
    EXPECT_EQ(std::count_if(quest.Rows.LogicMembers.begin(), quest.Rows.LogicMembers.end(), [](QuestGoalLogicMemberRow const& member) { return member.Kind == "ADD"; }), 2);
    EXPECT_EQ(std::count_if(quest.Rows.LogicMembers.begin(), quest.Rows.LogicMembers.end(), [](QuestGoalLogicMemberRow const& member) { return member.Kind == "AND"; }), 5);

    ASSERT_EQ(quest.Rows.Dialogs.size(), 1u);
    EXPECT_EQ(quest.Rows.Dialogs.front().Tag, "Prep");
    EXPECT_EQ(quest.Rows.Dialogs.front().Owner, "quest");
    ASSERT_EQ(quest.Rows.DialogEntries.size(), 1u);
    EXPECT_EQ(quest.Rows.DialogEntries.front().ActorTemplateId, ActorId);
    EXPECT_EQ(quest.Rows.DialogEntries.front().DialogKey, "TestLine_0");
    EXPECT_EQ(quest.Rows.DialogEntries.front().Animation, "Wave");
    ASSERT_EQ(quest.Rows.Madlibs.size(), 1u);
    EXPECT_EQ(quest.Rows.Madlibs.front().Identifier, "Name");
    EXPECT_EQ(quest.Rows.Madlibs.front().Value, "TestNpc");

    EXPECT_EQ(read.RootClasses["QuestTemplate"], 1u);
    EXPECT_EQ(read.RequirementClasses["RequirementList"], 1u);
    EXPECT_EQ(read.RequirementClasses["ReqHasEntry"], 1u);
    EXPECT_EQ(read.UnknownClasses["ResInventedReward"], 1u);
    EXPECT_EQ(read.NotCarried["dialog tag QuestInfo"], 1u);
    EXPECT_FALSE(read.NotCarried.contains("m_requirements"));
    EXPECT_EQ(read.NotCarried["negative m_questRepeat"], 1u);
    EXPECT_TRUE(read.Keys.contains("TestTitle_TEST-001"));
    EXPECT_TRUE(read.Keys.contains("TestLine_0"));
}

TEST_F(QuestPackageTest, CheckReadsPersonaTemplatesAsObjectsAndPassesTheValidator)
{
    QuestPackageRead read = QuestPackage::Read(_catalog, { QuestPackageEntry{ "a.xml", QuestFile("TEST-001") } });
    QuestPackage::Check(read, Install());
    ASSERT_EQ(read.Quests.size(), 1u);
    EXPECT_TRUE(read.Quests.front().IsClean()) << (read.Quests.front().Problems.empty() ? std::string() : read.Quests.front().Problems.front());
    EXPECT_EQ(read.Quests.front().Rows.Goals[0].PersonaName, "TestNpc");
    EXPECT_EQ(read.Adjusted["persona named by its _Persona template, read as the object it names"], 1u);
    EXPECT_EQ(read.GetCleanCount(), 1u);
}

TEST_F(QuestPackageTest, EachProblemFailsOnlyItsOwnQuest)
{
    std::string const unknownGoal = "    <m_goals><Class Name=\"class InventedGoalTemplate\"><m_goalName>Odd</m_goalName></Class></m_goals>\r\n";
    std::string const otherType = "    <m_goals><Class Name=\"class GoalTemplate\"><m_goalName>Odd</m_goalName><m_goalType>GOAL_TYPE_COLLECTAFTERBOUNTY</m_goalType></Class></m_goals>\r\n";
    std::string missingActor = QuestFile("TEST-ACTOR");
    missingActor.replace(missingActor.find("5001"), 4, "6001");
    QuestPackageRead read = QuestPackage::Read(_catalog, { QuestPackageEntry{ "clean.xml", QuestFile("TEST-CLEAN") }, QuestPackageEntry{ "unknown.xml", QuestFile("TEST-UNKNOWN", unknownGoal) },
        QuestPackageEntry{ "type.xml", QuestFile("TEST-TYPE", otherType) }, QuestPackageEntry{ "actor.xml", missingActor }, QuestPackageEntry{ "key.xml", QuestFile("TEST-KEY") },
        QuestPackageEntry{ "held.xml", QuestFile("TEST-HELD") }, QuestPackageEntry{ "logic.xml", QuestFile("TEST-LOGIC", {}, "Missing") },
        QuestPackageEntry{ "broken.xml", "<Objects><Class" } });
    QuestPackage::Check(read, Install({ "TestTitle_TEST-KEY" }), { "TEST-HELD" });
    std::map<std::string, std::string> categories;
    for (QuestPackageQuest const& quest : read.Quests)
        categories[quest.Name] = quest.Category;
    EXPECT_EQ(categories["TEST-CLEAN"], "");
    EXPECT_EQ(categories["TEST-UNKNOWN"], "unknown class InventedGoalTemplate");
    EXPECT_EQ(categories["TEST-TYPE"], "goal type GOAL_TYPE_COLLECTAFTERBOUNTY the tables do not hold");
    EXPECT_EQ(categories["TEST-ACTOR"], "actor template the install lacks");
    EXPECT_EQ(categories["TEST-KEY"], "locale key the install lacks");
    EXPECT_EQ(categories["TEST-HELD"], "quest name another source holds");
    EXPECT_EQ(categories["TEST-LOGIC"], "names a goal the quest does not have");
    EXPECT_EQ(read.Files, 8u);
    EXPECT_EQ(read.Quests.size(), 7u);
    EXPECT_EQ(read.GetCleanCount(), 1u);
    EXPECT_EQ(read.Unread.size(), 1u);
    EXPECT_EQ(QuestPackage::CountMissingKeys(read, Install({ "TestTitle_TEST-KEY" }).HasKey), 1u);
}

TEST_F(QuestPackageTest, TheScriptReplacesOnlyThePackagesRowsAndMarksEachWithItsSource)
{
    QuestPackageRead read = QuestPackage::Read(_catalog, { QuestPackageEntry{ "a.xml", QuestFile("TEST-001") }, QuestPackageEntry{ "b.xml", QuestFile("TEST-002") } });
    QuestPackage::Check(read, Install({ "TestTitle_TEST-002" }));
    std::string const source = QuestPackage::SourceOf("Test.wad");
    EXPECT_EQ(source, "package:Test.wad");
    WorldSqlScript const script = QuestPackage::BuildScript(read, source);
    std::vector<std::string> const& statements = script.GetStatements();
    ASSERT_FALSE(statements.empty());
    EXPECT_EQ(statements.front(), "DELETE FROM `quest_template` WHERE `source` = " + WorldSqlScript::Literal(source));
    EXPECT_EQ(std::count_if(statements.begin(), statements.end(), [](std::string const& statement) { return statement.starts_with("DELETE"); }), 1);
    std::string const marked = WorldSqlScript::Literal(source);
    for (std::size_t index = 1; index < statements.size(); ++index)
    {
        EXPECT_TRUE(statements[index].starts_with("INSERT INTO")) << statements[index];
        EXPECT_NE(statements[index].find("`source`"), std::string::npos) << statements[index];
        EXPECT_NE(statements[index].find(marked), std::string::npos) << statements[index];
    }
    std::string const all = fmt::format("{}", fmt::join(statements, "\n"));
    EXPECT_NE(all.find(WorldSqlScript::Literal(std::string("TEST-001"))), std::string::npos);
    EXPECT_EQ(all.find(WorldSqlScript::Literal(std::string("TEST-002"))), std::string::npos);
    std::vector<std::string_view> const tables = QuestPackage::GetTables();
    EXPECT_EQ(tables.size(), 10u);
}

TEST_F(QuestPackageTest, AQuestsRequirementListIsStoredEncodedAndDecodesBack)
{
    std::string const nested = "<Class Name=\"class RequirementList\"><m_requirements><Class Name=\"class ReqHasEntry\"><m_entryName>First</m_entryName></Class></m_requirements>"
        "<m_requirements><Class Name=\"class RequirementList\"><m_requirements><Class Name=\"class ReqHasEntry\"><m_entryName>Second</m_entryName></Class></m_requirements></Class>"
        "</m_requirements></Class>";
    QuestPackageRead read = QuestPackage::Read(_catalog, { QuestPackageEntry{ "gated.xml", WithRequirements(QuestFile("TEST-GATED"), nested) },
        QuestPackageEntry{ "open.xml", WithRequirements(QuestFile("TEST-OPEN"), {}) },
        QuestPackageEntry{ "empty.xml", WithRequirements(QuestFile("TEST-EMPTY"), "<Class Name=\"class RequirementList\"></Class>") },
        QuestPackageEntry{ "unknown.xml", WithRequirements(QuestFile("TEST-UNKNOWN"), "<Class Name=\"class RequirementList\"><m_requirements><Class Name=\"class ReqInvented\"></Class></m_requirements></Class>") },
        QuestPackageEntry{ "property.xml", WithRequirements(QuestFile("TEST-PROPERTY"), "<Class Name=\"class RequirementList\"><m_requirements><Class Name=\"class ReqHasEntry\"><m_invented>1</m_invented></Class></m_requirements></Class>") } });
    QuestPackage::Check(read, Install());
    std::map<std::string, QuestPackageQuest const*> quests;
    for (QuestPackageQuest const& quest : read.Quests)
        quests[quest.Name] = &quest;
    ASSERT_EQ(quests.size(), 5u);
    EXPECT_EQ(quests["TEST-UNKNOWN"]->Category, "unknown class ReqInvented");
    EXPECT_EQ(quests["TEST-PROPERTY"]->Category, "requirement not fully read (unknown property)");
    for (std::string const name : { "TEST-GATED", "TEST-OPEN", "TEST-EMPTY" })
        ASSERT_TRUE(quests[name]->IsClean()) << name << ": " << (quests[name]->Problems.empty() ? std::string() : quests[name]->Problems.front());
    EXPECT_TRUE(quests["TEST-OPEN"]->Rows.Quests.front().Requirements.empty());
    EXPECT_TRUE(quests["TEST-EMPTY"]->Rows.Quests.front().Requirements.empty());

    std::vector<uint8> const& bytes = quests["TEST-GATED"]->Rows.Quests.front().Requirements;
    ASSERT_FALSE(bytes.empty());
    DecodeResult const decoded = ObjectSerializer::Decode(_catalog, bytes, ZoneExtractor::DataOptions());
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_TRUE(decoded.Object);
    EXPECT_EQ(decoded.Object->GetClass().Name, "class RequirementList");
    PropertyValue::List const* const outer = decoded.Object->Get("m_requirements")->GetList();
    ASSERT_TRUE(outer);
    ASSERT_EQ(outer->size(), 2u);
    PropertyObject const* const first = (*outer)[0].AsObject();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->GetClass().Name, "class ReqHasEntry");
    EXPECT_EQ(first->Get("m_entryName")->GetIf<std::string>() ? *first->Get("m_entryName")->GetIf<std::string>() : std::string(), "First");
    PropertyObject const* const inner = (*outer)[1].AsObject();
    ASSERT_TRUE(inner);
    EXPECT_EQ(inner->GetClass().Name, "class RequirementList");
    PropertyValue::List const* const innerList = inner->Get("m_requirements")->GetList();
    ASSERT_TRUE(innerList);
    ASSERT_EQ(innerList->size(), 1u);
    ASSERT_TRUE((*innerList)[0].AsObject());
    EXPECT_EQ(*(*innerList)[0].AsObject()->Get("m_entryName")->GetIf<std::string>(), "Second");

    std::string const all = fmt::format("{}", fmt::join(QuestPackage::BuildScript(read, QuestPackage::SourceOf("Test.wad")).GetStatements(), "\n"));
    EXPECT_NE(all.find("`requirements`"), std::string::npos);
    EXPECT_NE(all.find(WorldSqlScript::Literal(WorldSqlScript::Value{ std::string(bytes.begin(), bytes.end()) })), std::string::npos);
    EXPECT_EQ(all.find("TEST-UNKNOWN"), std::string::npos);
}
