/*
 * Project Ambrose by Imjustchico
 * Exercises every built-in requirement against a fake wizard context, passing on its own fact and failing the list when that fact does not hold, list operators and nesting, fail-closed unknown types, and both atomic success and failure paths of requirement reloads.
 */

#include "Log.h"
#include "LogTestConfig.h"
#include "RequirementMgr.h"
#include "ReloadMgr.h"
#include "TestAppender.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    RequirementRow Requirement(std::string type, std::uint32_t position)
    {
        RequirementRow row;
        row.ListId = "root";
        row.Position = position;
        row.Type = std::move(type);
        return row;
    }

    RequirementRows SingleRequirement(RequirementRow row)
    {
        RequirementRows rows;
        rows.Lists.push_back(RequirementListRow{ "root" });
        rows.Requirements.push_back(std::move(row));
        return rows;
    }

    RequirementRows BuiltInRows()
    {
        RequirementRows rows;
        rows.Lists.push_back(RequirementListRow{ "root" });

        RequirementRow hasQuest = Requirement("ReqHasQuest", 0);
        hasQuest.QuestName = "Q1";
        rows.Requirements.push_back(hasQuest);

        RequirementRow hasGoal = Requirement("ReqHasGoal", 1);
        hasGoal.QuestName = "Q1";
        hasGoal.GoalName = "Goal1";
        hasGoal.RequiredStatus = static_cast<std::uint8_t>(RequirementGoalStatus::Complete);
        rows.Requirements.push_back(hasGoal);

        RequirementRow hasEntry = Requirement("ReqHasEntry", 2);
        hasEntry.QuestName = "Q1";
        hasEntry.EntryName = "Complete";
        hasEntry.IsQuestRegistry = true;
        rows.Requirements.push_back(hasEntry);

        RequirementRow entryValue = Requirement("ReqEntryValue", 3);
        entryValue.EntryName = "Progress";
        entryValue.NumericValue = 5;
        entryValue.OperatorType = 3;
        rows.Requirements.push_back(entryValue);

        RequirementRow globalValue = Requirement("ReqGlobalRegistryValue", 4);
        globalValue.EntryName = "GlobalProgress";
        globalValue.NumericValue = 5;
        globalValue.OperatorType = 3;
        rows.Requirements.push_back(globalValue);

        RequirementRow magicLevel = Requirement("ReqMagicLevel", 5);
        magicLevel.MagicSchool = "Fire";
        magicLevel.NumericValue = 5;
        magicLevel.OperatorType = 3;
        rows.Requirements.push_back(magicLevel);

        RequirementRow schoolFocus = Requirement("ReqSchoolOfFocus", 6);
        schoolFocus.MagicSchool = "Fire";
        rows.Requirements.push_back(schoolFocus);

        RequirementRow isSchool = Requirement("ReqIsSchool", 7);
        isSchool.MagicSchool = "Fire";
        isSchool.TargetType = 0;
        rows.Requirements.push_back(isSchool);

        RequirementRow inZone = Requirement("ReqInZone", 8);
        inZone.ZoneName = "WizardCity/WC_Hub";
        rows.Requirements.push_back(inZone);

        RequirementRow isGender = Requirement("ReqIsGender", 9);
        isGender.Gender = "Female";
        rows.Requirements.push_back(isGender);

        RequirementRow hasBadge = Requirement("ReqHasBadge", 10);
        hasBadge.BadgeName = "Badge1";
        rows.Requirements.push_back(hasBadge);

        return rows;
    }

    class FakeRequirementContext : public RequirementContext
    {
    public:
        std::optional<bool> HasQuest(std::string_view questName) const override
        {
            auto found = Quests.find(std::string(questName));
            return found == Quests.end() ? std::nullopt : std::optional<bool>(found->second);
        }

        std::optional<RequirementGoalStatus> GetGoalStatus(std::string_view questName, std::string_view goalName) const override
        {
            LastQuest = questName;
            LastGoal = goalName;
            return GoalStatus;
        }

        std::optional<bool> HasRegistryEntry(std::string_view questName, std::string_view entryName, bool isQuestRegistry) const override
        {
            LastQuest = questName;
            LastEntry = entryName;
            LastIsQuestRegistry = isQuestRegistry;
            LastHasEntryQuest = questName;
            LastHasEntry = entryName;
            LastHasEntryIsQuestRegistry = isQuestRegistry;
            return RegistryEntry;
        }

        std::optional<double> GetRegistryValue(std::string_view questName, std::string_view entryName, bool isQuestRegistry) const override
        {
            LastQuest = questName;
            LastEntry = entryName;
            LastIsQuestRegistry = isQuestRegistry;
            LastRegistryValueEntry = entryName;
            LastRegistryValueIsQuestRegistry = isQuestRegistry;
            return Number;
        }

        std::optional<double> GetGlobalRegistryValue(std::string_view entryName) const override
        {
            LastEntry = entryName;
            LastGlobalRegistryEntry = entryName;
            return Number;
        }

        std::optional<double> GetMagicLevel(std::string_view school) const override
        {
            LastSchool = school;
            return Number;
        }

        std::optional<bool> HasSchoolOfFocus(std::string_view school) const override
        {
            LastSchool = school;
            return SchoolOfFocus;
        }

        std::optional<bool> IsSchool(std::string_view school, std::uint8_t targetType) const override
        {
            LastSchool = school;
            LastTargetType = targetType;
            return IsSchoolValue;
        }

        std::optional<bool> IsInZone(std::string_view zoneName) const override
        {
            LastZone = zoneName;
            return InZone;
        }

        std::optional<bool> IsGender(std::string_view gender) const override
        {
            LastGender = gender;
            return Gender;
        }

        std::optional<bool> HasBadge(std::string_view badgeName) const override
        {
            LastBadge = badgeName;
            return Badge;
        }

        std::map<std::string, bool, std::less<>> Quests;
        std::optional<RequirementGoalStatus> GoalStatus = RequirementGoalStatus::Complete;
        std::optional<bool> RegistryEntry = true;
        std::optional<double> Number = 10;
        std::optional<bool> SchoolOfFocus = true;
        std::optional<bool> IsSchoolValue = true;
        std::optional<bool> InZone = true;
        std::optional<bool> Gender = true;
        std::optional<bool> Badge = true;
        mutable std::string LastQuest;
        mutable std::string LastGoal;
        mutable std::string LastEntry;
        mutable std::string LastHasEntryQuest;
        mutable std::string LastHasEntry;
        mutable std::string LastRegistryValueEntry;
        mutable std::string LastGlobalRegistryEntry;
        mutable std::string LastSchool;
        mutable std::string LastZone;
        mutable std::string LastGender;
        mutable std::string LastBadge;
        mutable std::uint8_t LastTargetType = 255;
        mutable bool LastIsQuestRegistry = false;
        mutable bool LastHasEntryIsQuestRegistry = false;
        mutable bool LastRegistryValueIsQuestRegistry = false;
    };

    struct CapturedLog
    {
        CapturedLog() : Store(std::make_shared<TestAppenderStore>())
        {
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(Store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n"));
        }

        ~CapturedLog()
        {
            sLog.Reset();
        }

        std::size_t Count(std::string_view text) const
        {
            auto const messages = Store->Messages("Capture");
            return static_cast<std::size_t>(std::count_if(messages.begin(), messages.end(), [text](LogMessage const& message)
            {
                return message.Text.find(text) != std::string::npos;
            }));
        }

        std::shared_ptr<TestAppenderStore> Store;
    };

    class RequirementTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            sRequirementMgr.Clear();
            sRequirementMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            sRequirementMgr.Clear();
        }

        bool Load(RequirementRows rows, std::vector<std::string>& errors)
        {
            auto const source = std::make_shared<RequirementRows>(std::move(rows));
            sRequirementMgr.SetRowSource([source] { return *source; });
            return sRequirementMgr.Load(errors);
        }
    };
}

TEST_F(RequirementTest, EveryBuiltInTypeUsesItsMatchingWizardFact)
{
    RequirementRows rows = BuiltInRows();
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(std::move(rows), errors)) << (errors.empty() ? "" : errors.front());
    FakeRequirementContext context;
    context.Quests["Q1"] = true;

    EXPECT_TRUE(sRequirementMgr.Evaluate("root", context));
    EXPECT_EQ(context.LastGoal, "Goal1");
    EXPECT_EQ(context.LastHasEntryQuest, "Q1");
    EXPECT_EQ(context.LastHasEntry, "Complete");
    EXPECT_TRUE(context.LastHasEntryIsQuestRegistry);
    EXPECT_EQ(context.LastRegistryValueEntry, "Progress");
    EXPECT_FALSE(context.LastRegistryValueIsQuestRegistry);
    EXPECT_EQ(context.LastGlobalRegistryEntry, "GlobalProgress");
    EXPECT_EQ(context.LastSchool, "Fire");
    EXPECT_EQ(context.LastTargetType, 0);
    EXPECT_EQ(context.LastZone, "WizardCity/WC_Hub");
    EXPECT_EQ(context.LastGender, "Female");
    EXPECT_EQ(context.LastBadge, "Badge1");
}

TEST_F(RequirementTest, EachBuiltInTypeFailsTheListWhenItsOwnFactDoesNotHold)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(BuiltInRows(), errors)) << (errors.empty() ? "" : errors.front());

    std::vector<std::pair<std::string_view, std::function<void(FakeRequirementContext&)>>> const breaks{
        { "ReqHasQuest", [](FakeRequirementContext& context) { context.Quests["Q1"] = false; } },
        { "ReqHasGoal", [](FakeRequirementContext& context) { context.GoalStatus = RequirementGoalStatus::Incomplete; } },
        { "ReqHasEntry", [](FakeRequirementContext& context) { context.RegistryEntry = false; } },
        { "ReqEntryValue, ReqGlobalRegistryValue and ReqMagicLevel", [](FakeRequirementContext& context) { context.Number = 4; } },
        { "ReqSchoolOfFocus", [](FakeRequirementContext& context) { context.SchoolOfFocus = false; } },
        { "ReqIsSchool", [](FakeRequirementContext& context) { context.IsSchoolValue = false; } },
        { "ReqInZone", [](FakeRequirementContext& context) { context.InZone = false; } },
        { "ReqIsGender", [](FakeRequirementContext& context) { context.Gender = false; } },
        { "ReqHasBadge", [](FakeRequirementContext& context) { context.Badge = false; } },
        { "an unanswered fact", [](FakeRequirementContext& context) { context.Badge = std::nullopt; } }
    };
    for (auto const& [name, breakFact] : breaks)
    {
        FakeRequirementContext context;
        context.Quests["Q1"] = true;
        ASSERT_TRUE(sRequirementMgr.Evaluate("root", context)) << name;
        breakFact(context);
        EXPECT_FALSE(sRequirementMgr.Evaluate("root", context)) << name;
    }
}

TEST_F(RequirementTest, NumericOperatorsMatchTheClientValues)
{
    FakeRequirementContext context;
    context.Number = 5;
    std::vector<std::string> errors;

    for (auto const& [operatorType, expected, result] : std::vector<std::tuple<std::uint32_t, double, bool>>{
        { 0, 5, true },
        { 1, 4, true },
        { 2, 6, true },
        { 3, 5, true },
        { 4, 5, true }
    })
    {
        RequirementRow row = Requirement("ReqEntryValue", 0);
        row.EntryName = "Value";
        row.NumericValue = expected;
        row.OperatorType = operatorType;
        ASSERT_TRUE(Load(SingleRequirement(std::move(row)), errors)) << (errors.empty() ? "" : errors.front());
        EXPECT_EQ(sRequirementMgr.Evaluate("root", context), result) << "operator type " << operatorType;
    }
}

TEST_F(RequirementTest, NestedOrListsAndApplyNotCompose)
{
    RequirementRows rows;
    rows.Lists = {
        RequirementListRow{ "root", "AND", false, std::nullopt },
        RequirementListRow{ "nested-or", "OR", false, "root" },
        RequirementListRow{ "negated", "AND", true, "root" }
    };
    RequirementRow rootFact = Requirement("ReqHasQuest", 0);
    rootFact.QuestName = "Q-root";
    rows.Requirements.push_back(rootFact);
    RequirementRow orFalse = Requirement("ReqHasQuest", 0);
    orFalse.ListId = "nested-or";
    orFalse.QuestName = "Q-false";
    rows.Requirements.push_back(orFalse);
    RequirementRow orTrue = Requirement("ReqHasQuest", 1);
    orTrue.ListId = "nested-or";
    orTrue.QuestName = "Q-true";
    rows.Requirements.push_back(orTrue);
    RequirementRow toNegate = Requirement("ReqHasQuest", 0);
    toNegate.ListId = "negated";
    toNegate.QuestName = "Q-false";
    rows.Requirements.push_back(toNegate);

    std::vector<std::string> errors;
    ASSERT_TRUE(Load(std::move(rows), errors)) << (errors.empty() ? "" : errors.front());
    FakeRequirementContext context;
    context.Quests = { { "Q-root", true }, { "Q-false", false }, { "Q-true", true } };

    EXPECT_TRUE(sRequirementMgr.Evaluate("root", context));
    context.Quests["Q-true"] = false;
    EXPECT_FALSE(sRequirementMgr.Evaluate("root", context));
}

TEST_F(RequirementTest, RequirementApplyNotInvertsTheLeafResult)
{
    RequirementRow requirement = Requirement("ReqHasQuest", 0);
    requirement.QuestName = "Q1";
    requirement.ApplyNot = true;
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(SingleRequirement(std::move(requirement)), errors)) << (errors.empty() ? "" : errors.front());
    FakeRequirementContext context;
    context.Quests["Q1"] = false;

    EXPECT_TRUE(sRequirementMgr.Evaluate("root", context));
}

TEST_F(RequirementTest, UnknownTypeDoesNotPassWhenNegated)
{
    RequirementRow unknown = Requirement("ReqFromFutureClient", 0);
    unknown.ApplyNot = true;
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(SingleRequirement(std::move(unknown)), errors)) << (errors.empty() ? "" : errors.front());
    FakeRequirementContext context;

    EXPECT_FALSE(sRequirementMgr.Evaluate("root", context));
}

TEST_F(RequirementTest, UnknownTypeFailsClosedAndLogsOnlyOnce)
{
    RequirementRow unknown = Requirement("ReqFromFutureClient", 0);
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(SingleRequirement(std::move(unknown)), errors)) << (errors.empty() ? "" : errors.front());
    FakeRequirementContext context;
    CapturedLog log;

    EXPECT_FALSE(sRequirementMgr.Evaluate("root", context));
    EXPECT_FALSE(sRequirementMgr.Evaluate("root", context));
    EXPECT_EQ(log.Count("Unknown requirement type ReqFromFutureClient"), 1u);
}

TEST_F(RequirementTest, ReloadAppliesEditsAndMalformedReloadKeepsServingLists)
{
    auto rows = std::make_shared<RequirementRows>(SingleRequirement(Requirement("ReqHasQuest", 0)));
    rows->Requirements[0].QuestName = "Q1";
    sRequirementMgr.SetRowSource([rows] { return *rows; });
    std::vector<std::string> errors;
    ASSERT_TRUE(sRequirementMgr.Load(errors)) << (errors.empty() ? "" : errors.front());
    FakeRequirementContext context;
    context.Quests["Q1"] = true;
    context.Quests["Q2"] = false;
    ASSERT_TRUE(sRequirementMgr.Evaluate("root", context));
    std::uint64_t const firstGeneration = sRequirementMgr.GetGeneration();

    rows->Requirements[0].QuestName = "Q2";
    ReloadOutcome const edited = sReloadMgr.Reload(RequirementMgr::Target);
    ASSERT_TRUE(edited.Ok) << (edited.Errors.empty() ? "" : edited.Errors.front());
    EXPECT_GT(sRequirementMgr.GetGeneration(), firstGeneration);
    EXPECT_FALSE(sRequirementMgr.Evaluate("root", context));

    context.Quests["Q2"] = true;
    RequirementListRow& malformedList = rows->Lists[0];
    malformedList.Operator = "XOR";
    std::uint64_t const servingGeneration = sRequirementMgr.GetGeneration();
    ReloadOutcome const invalid = sReloadMgr.Reload(RequirementMgr::Target);
    EXPECT_FALSE(invalid.Ok);
    EXPECT_FALSE(invalid.Errors.empty());
    EXPECT_EQ(sRequirementMgr.GetGeneration(), servingGeneration);
    EXPECT_TRUE(sRequirementMgr.Evaluate("root", context));
}
