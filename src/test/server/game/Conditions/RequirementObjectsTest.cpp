/*
 * Project Ambrose by Imjustchico
 * Tests reading requirement objects through a type dump the test writes: a RequirementList holding a quest requirement and a nested NOT list with a goal requirement reads as a tree with its operators, names and status and survives the zone data encoding the extractor stores it in, a goal the wizard does not hold counts as not held so a NOT over it passes, a class project requirement reads its project's name and passes only while that project runs, a quest's stored list decodes and gates its offer while bytes with no type dump, that do not decode or that hold no RequirementList close it with the reason, and something that is not a RequirementList gives no tree.
 */

#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "QuestMgr.h"
#include "QuestOffers.h"
#include "RequirementMgr.h"
#include "RequirementObjects.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"
#include "ZoneExtractor.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 7;

    void AddClass(Json& classes, std::string const& name, std::vector<std::string> const& bases, std::vector<std::pair<std::string, std::string>> const& fields)
    {
        Json properties = Json::object();
        uint32 id = 0;
        for (auto const& [type, field] : fields)
        {
            bool const list = field == "m_requirements";
            bool const pointer = type.ends_with('*');
            properties[field] = Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", list ? "List" : "Static" },
                { "dynamic", list }, { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, field) } };
            if (type == "enum Requirement::Operator")
                properties[field]["enum_options"] = Json{ { "ROP_AND", 0 }, { "ROP_OR", 1 } };
            else if (type == "enum GoalStatusRequirement")
                properties[field]["enum_options"] = Json{ { "DontCare", 0 }, { "Complete", 1 }, { "Incomplete", 2 } };
            ++id;
        }
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", bases }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    std::string RequirementDump()
    {
        Json classes = Json::object();
        for (char const* name : { "class PropertyClass", "enum Requirement::Operator", "enum GoalStatusRequirement" })
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", Json::array() }, { "hash", StringHash::KiStringHash(name) },
                { "properties", Json::object() } };
        std::pair<std::string, std::string> const applyNot{ "bool", "m_applyNOT" };
        std::pair<std::string, std::string> const listOperator{ "enum Requirement::Operator", "m_operator" };
        AddClass(classes, "class Requirement", { "class PropertyClass" }, { applyNot, listOperator });
        AddClass(classes, "class RequirementList", { "class Requirement", "class PropertyClass" }, { applyNot, listOperator, { "class Requirement*", "m_requirements" } });
        AddClass(classes, "class ReqHasQuest", { "class Requirement", "class PropertyClass" }, { applyNot, listOperator, { "std::string", "m_questName" } });
        AddClass(classes, "class ReqHasGoal", { "class Requirement", "class PropertyClass" },
            { applyNot, listOperator, { "std::string", "m_questName" }, { "std::string", "m_goalName" }, { "enum GoalStatusRequirement", "m_requiredStatus" } });
        AddClass(classes, "class ReqClassProjectActive", { "class Requirement", "class PropertyClass" }, { applyNot, listOperator, { "std::string", "m_classProjectName" } });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    class QuestFacts : public RequirementContext
    {
    public:
        std::set<std::string, std::less<>> Quests;
        std::set<std::string, std::less<>> CompletedGoals;
        std::set<std::string, std::less<>> ClassProjects;

        std::optional<bool> HasQuest(std::string_view questName) const override { return Quests.contains(questName); }
        std::optional<RequirementGoalStatus> GetGoalStatus(std::string_view, std::string_view goalName) const override
        {
            return CompletedGoals.contains(goalName) ? RequirementGoalStatus::Complete : RequirementGoalStatus::Absent;
        }
        std::optional<bool> IsClassProjectActive(std::string_view projectName) const override { return ClassProjects.contains(projectName); }
    };

    class RequirementObjectsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(_types.LoadFromText(RequirementDump(), "requirements.json")) << _types.GetErrors().front();
        }

        PropertyObjectPtr Create(std::string const& type)
        {
            PropertyObjectPtr object = PropertyObject::Create(_types.GetCatalog(), type);
            EXPECT_TRUE(object) << type;
            return object;
        }

        PropertyObjectPtr GateList()
        {
            PropertyObjectPtr quest = Create("class ReqHasQuest");
            EXPECT_EQ(quest->Set("m_questName", std::string("WC-MAIN-C01-001")), PropertySetResult::Ok);
            PropertyObjectPtr goal = Create("class ReqHasGoal");
            EXPECT_EQ(goal->Set("m_questName", std::string("WC-MAIN-C01-001")), PropertySetResult::Ok);
            EXPECT_EQ(goal->Set("m_goalName", std::string("OpenGate")), PropertySetResult::Ok);
            EXPECT_EQ(goal->Set("m_requiredStatus", int64{ 1 }), PropertySetResult::Ok);
            PropertyObjectPtr notYet = Create("class RequirementList");
            EXPECT_EQ(notYet->Set("m_applyNOT", true), PropertySetResult::Ok);
            PropertyValue::List inner;
            inner.emplace_back(std::move(goal));
            EXPECT_EQ(notYet->Set("m_requirements", std::move(inner)), PropertySetResult::Ok);
            PropertyObjectPtr list = Create("class RequirementList");
            PropertyValue::List outer;
            outer.emplace_back(std::move(quest));
            outer.emplace_back(std::move(notYet));
            EXPECT_EQ(list->Set("m_requirements", std::move(outer)), PropertySetResult::Ok);
            return list;
        }

        void TearDown() override
        {
            sQuestMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
        }

        TypeRegistry _types;
        TypedViewRegistry _noViews;
    };
}

TEST_F(RequirementObjectsTest, AListWithANestedNotListReadsAsATreeAndSurvivesTheZoneDataEncoding)
{
    PropertyObjectPtr const list = GateList();
    EncodeResult const encoded = ObjectSerializer::Encode(list.get(), ZoneExtractor::DataOptions());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    DecodeResult const decoded = ObjectSerializer::Decode(_types.GetCatalog(), encoded.Bytes, ZoneExtractor::DataOptions());
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;

    std::optional<RequirementTree> const tree = RequirementObjects::TreeOf(decoded.Object.get());
    ASSERT_TRUE(tree.has_value());
    EXPECT_EQ(tree->List.Operator, "AND");
    EXPECT_FALSE(tree->List.ApplyNot);
    ASSERT_EQ(tree->Requirements.size(), 1u);
    EXPECT_EQ(tree->Requirements[0].Type, "ReqHasQuest");
    EXPECT_EQ(tree->Requirements[0].QuestName, "WC-MAIN-C01-001");
    ASSERT_EQ(tree->Children.size(), 1u) << "a RequirementList inside a list is a nested list, not a requirement of an unknown type";
    RequirementTree const& child = tree->Children[0];
    EXPECT_TRUE(child.List.ApplyNot);
    ASSERT_EQ(child.Requirements.size(), 1u);
    EXPECT_EQ(child.Requirements[0].Type, "ReqHasGoal");
    EXPECT_EQ(child.Requirements[0].GoalName, "OpenGate");
    EXPECT_EQ(child.Requirements[0].RequiredStatus, std::optional<uint8>(1));
}

TEST_F(RequirementObjectsTest, AGoalTheWizardDoesNotHoldCountsAsNotHeldSoANotOverItPasses)
{
    std::optional<RequirementTree> const tree = RequirementObjects::TreeOf(GateList().get());
    ASSERT_TRUE(tree.has_value());
    QuestFacts wizard;
    EXPECT_FALSE(sRequirementMgr.EvaluateTree(*tree, wizard)) << "a wizard without the quest does not qualify";
    wizard.Quests.insert("WC-MAIN-C01-001");
    EXPECT_TRUE(sRequirementMgr.EvaluateTree(*tree, wizard)) << "the goal is not held yet, so NOT completed passes";
    wizard.CompletedGoals.insert("OpenGate");
    EXPECT_FALSE(sRequirementMgr.EvaluateTree(*tree, wizard)) << "once the goal is complete the object is no longer for this wizard";
}

TEST_F(RequirementObjectsTest, AClassProjectRequirementReadsItsProjectAndPassesOnlyWhileItRuns)
{
    PropertyObjectPtr project = Create("class ReqClassProjectActive");
    ASSERT_EQ(project->Set("m_classProjectName", std::string("DuelArena")), PropertySetResult::Ok);
    PropertyObjectPtr const list = Create("class RequirementList");
    PropertyValue::List held;
    held.emplace_back(std::move(project));
    ASSERT_EQ(list->Set("m_requirements", std::move(held)), PropertySetResult::Ok);

    std::optional<RequirementTree> const tree = RequirementObjects::TreeOf(list.get());
    ASSERT_TRUE(tree.has_value());
    ASSERT_EQ(tree->Requirements.size(), 1u);
    EXPECT_EQ(tree->Requirements[0].Type, "ReqClassProjectActive");
    EXPECT_EQ(tree->Requirements[0].ClassProjectName, "DuelArena");
    QuestFacts realm;
    EXPECT_FALSE(sRequirementMgr.EvaluateTree(*tree, realm)) << "no project runs, so the object stays hidden";
    realm.ClassProjects.insert("DuelArena");
    EXPECT_TRUE(sRequirementMgr.EvaluateTree(*tree, realm));
}

TEST_F(RequirementObjectsTest, StoredBytesThatCannotBeReadGiveAClosedListAndSayWhy)
{
    EncodeResult const encoded = ObjectSerializer::Encode(GateList().get(), ZoneExtractor::DataOptions());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    std::string problem;
    RequirementTree const read = RequirementObjects::Decode(_types.GetCatalog(), encoded.Bytes, problem);
    EXPECT_TRUE(problem.empty()) << problem;
    ASSERT_EQ(read.Children.size(), 1u);

    QuestFacts wizard;
    wizard.Quests.insert("WC-MAIN-C01-001");
    for (auto const& [catalog, bytes, reason] : { std::tuple{ TypeCatalogPtr(), encoded.Bytes, "no type dump" }, std::tuple{ _types.GetCatalog(), std::vector<uint8>{ 1, 2, 3 }, "do not decode" } })
    {
        problem.clear();
        RequirementTree const closed = RequirementObjects::Decode(catalog, bytes, problem);
        EXPECT_NE(problem.find(reason), std::string::npos) << problem;
        EXPECT_FALSE(sRequirementMgr.EvaluateTree(closed, wizard)) << reason;
    }
    PropertyObjectPtr const quest = Create("class ReqHasQuest");
    EncodeResult const single = ObjectSerializer::Encode(quest.get(), ZoneExtractor::DataOptions());
    ASSERT_TRUE(single.Ok()) << single.Detail;
    problem.clear();
    EXPECT_FALSE(sRequirementMgr.EvaluateTree(RequirementObjects::Decode(_types.GetCatalog(), single.Bytes, problem), wizard));
    EXPECT_NE(problem.find("not a RequirementList"), std::string::npos) << problem;
}

TEST_F(RequirementObjectsTest, AQuestIsOfferedOnlyWhileItsStoredRequirementsPass)
{
    EncodeResult const encoded = ObjectSerializer::Encode(GateList().get(), ZoneExtractor::DataOptions());
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    sTypeRegistry.Clear();
    sTypeRegistry.SetViews(&_noViews);
    ASSERT_TRUE(sTypeRegistry.LoadFromText(RequirementDump(), "requirements.json")) << sTypeRegistry.GetErrors().front();
    auto const rows = std::make_shared<QuestRows>();
    for (auto const& [name, bytes] : { std::pair{ "Gated", encoded.Bytes }, std::pair{ "Unreadable", std::vector<uint8>{ 1, 2, 3 } } })
    {
        QuestTemplateRow quest;
        quest.Name = name;
        quest.TitleKey = std::string("QuestTitle_") + name;
        quest.Level = 1;
        quest.Requirements = bytes;
        rows->Quests.push_back(std::move(quest));
    }
    sQuestMgr.Clear();
    sQuestMgr.SetRowSource([rows](QuestRows& out, std::vector<std::string>&)
    {
        out = *rows;
        return true;
    });
    QuestLoadResult const loaded = sQuestMgr.LoadSkippingInvalid();
    ASSERT_TRUE(loaded.Loaded);
    ASSERT_EQ(loaded.Quests, 2u) << (loaded.Errors.empty() ? "" : loaded.Errors.front());

    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    QuestInfo const* const gated = quests->Find("Gated");
    QuestInfo const* const unreadable = quests->Find("Unreadable");
    ASSERT_NE(gated, nullptr);
    ASSERT_NE(unreadable, nullptr);
    QuestFacts wizard;
    EXPECT_FALSE(QuestOffers::Qualifies(*gated, wizard)) << "a wizard without the quest does not qualify";
    wizard.Quests.insert("WC-MAIN-C01-001");
    EXPECT_TRUE(QuestOffers::Qualifies(*gated, wizard));
    wizard.CompletedGoals.insert("OpenGate");
    EXPECT_FALSE(QuestOffers::Qualifies(*gated, wizard));
    wizard.CompletedGoals.clear();
    EXPECT_FALSE(QuestOffers::Qualifies(*unreadable, wizard)) << "bytes that do not decode keep the quest closed";
}

TEST_F(RequirementObjectsTest, SomethingThatIsNotARequirementListGivesNoTree)
{
    EXPECT_FALSE(RequirementObjects::TreeOf(Create("class ReqHasQuest").get()).has_value());
    EXPECT_FALSE(RequirementObjects::TreeOf(nullptr).has_value());
    EXPECT_EQ(RequirementObjects::RowOf(nullptr).Type, RequirementObjects::UnnamedRequirement);
}
