/*
 * Project Ambrose by Imjustchico
 * Tests the quest manager over hand-written rows it reads from an in-memory source: a Prep dialog whose lines are spoken by template 38232 and then template 0 makes the quest offered by 38232 alone, the goal lookups find each goal by persona, tag, adjective and zone, the validator refuses goalsToAdd naming a missing goal, a bounty goal without adjectives or a tally count, a persona no object is named, a logic entry that both completes the quest and adds goals, a starter without a Prep dialog and a key the locale text lacks, a start skips the bad quests and logs how many quests, goals and validation errors it found, a reload of quest_template swaps in an added quest with its starter while one that brings errors keeps the quests serving, and a quest registry requirement gates availability until its Q1 entry is present.
 */

#include "Log.h"
#include "LogTestConfig.h"
#include "QuestMgr.h"
#include "QuestValidator.h"
#include "ReloadMgr.h"
#include "RequirementMgr.h"
#include "TestAppender.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr uint32 HeadmasterId = 38232;
    constexpr uint32 GammaId = 40001;

    QuestTemplateRow Quest(std::string name)
    {
        QuestTemplateRow row;
        row.Name = std::move(name);
        row.TitleKey = "QuestTitle_" + row.Name;
        row.Level = 1;
        return row;
    }

    QuestGoalRow Goal(std::string quest, std::string goal, std::string type)
    {
        QuestGoalRow row;
        row.Quest = std::move(quest);
        row.Goal = std::move(goal);
        row.Type = std::move(type);
        return row;
    }

    QuestDialogEntryRow Line(std::string quest, std::string tag, uint32 position, uint32 actor)
    {
        QuestDialogEntryRow row;
        row.Quest = std::move(quest);
        row.Owner = "quest";
        row.Tag = std::move(tag);
        row.Position = position;
        row.ActorTemplateId = actor;
        row.DialogKey = "Dialog_" + row.Quest + "_" + row.Tag + "_" + std::to_string(position);
        return row;
    }

    void AddStarter(QuestRows& rows, std::string const& quest, uint32 actor)
    {
        rows.Dialogs.push_back(QuestDialogRow{ quest, "quest", "", "Start" });
        rows.Dialogs.push_back(QuestDialogRow{ quest, "quest", "", "Prep" });
        rows.DialogEntries.push_back(Line(quest, "Start", 0, actor));
        rows.DialogEntries.push_back(Line(quest, "Prep", 1, 0));
        rows.DialogEntries.push_back(Line(quest, "Prep", 0, actor));
    }

    QuestRows Fixture()
    {
        QuestRows rows;
        rows.Objects = { { 1, "Player" }, { HeadmasterId, "Headmaster" }, { GammaId, "Gamma" } };
        rows.Quests.push_back(Quest("PrepQuest"));

        QuestGoalRow persona = Goal("PrepQuest", "TalkToHeadmaster", "persona");
        persona.PersonaName = "Headmaster";
        rows.Goals.push_back(persona);
        QuestGoalRow bounty = Goal("PrepQuest", "DefeatUndead", "bounty");
        bounty.TallyCount = 3;
        rows.Goals.push_back(bounty);
        rows.Adjectives.push_back(QuestGoalAdjectiveRow{ "PrepQuest", "DefeatUndead", 0, "Undead" });
        QuestGoalRow waypoint = Goal("PrepQuest", "VisitCommons", "waypoint");
        waypoint.DestinationZone = "WizardCity/WC_Hub";
        rows.Goals.push_back(waypoint);
        QuestGoalRow usage = Goal("PrepQuest", "UseLamp", "usage");
        rows.Goals.push_back(usage);
        rows.ClientTags.push_back(QuestGoalClientTagRow{ "PrepQuest", "UseLamp", 0, "Lamp" });
        rows.Adjectives.push_back(QuestGoalAdjectiveRow{ "PrepQuest", "UseLamp", 0, "Lit" });

        rows.StartGoals.push_back(QuestStartGoalRow{ "PrepQuest", "TalkToHeadmaster" });
        rows.Logic.push_back(QuestGoalLogicRow{ "PrepQuest", "TalkToHeadmaster", 0, false });
        rows.LogicMembers.push_back(QuestGoalLogicMemberRow{ "PrepQuest", "TalkToHeadmaster", 0, "ADD", 1, "VisitCommons" });
        rows.LogicMembers.push_back(QuestGoalLogicMemberRow{ "PrepQuest", "TalkToHeadmaster", 0, "ADD", 0, "DefeatUndead" });
        rows.Logic.push_back(QuestGoalLogicRow{ "PrepQuest", "DefeatUndead", 0, true });
        rows.LogicMembers.push_back(QuestGoalLogicMemberRow{ "PrepQuest", "DefeatUndead", 0, "AND", 0, "VisitCommons" });

        AddStarter(rows, "PrepQuest", HeadmasterId);
        rows.Madlibs.push_back(QuestDialogMadlibRow{ "PrepQuest", "quest", "", "Prep", 0, "Name", "Headmaster" });
        rows.Dialogs.push_back(QuestDialogRow{ "PrepQuest", "goal", "TalkToHeadmaster", "Complete" });
        QuestDialogEntryRow goalLine = Line("PrepQuest", "Complete", 0, HeadmasterId);
        goalLine.Owner = "goal";
        goalLine.Goal = "TalkToHeadmaster";
        rows.DialogEntries.push_back(goalLine);
        return rows;
    }

    std::vector<std::string> Messages(std::vector<QuestValidationError> const& errors)
    {
        std::vector<std::string> messages;
        for (QuestValidationError const& error : errors)
            messages.push_back(error.Message);
        return messages;
    }

    bool Holds(std::vector<std::string> const& messages, std::string_view text)
    {
        return std::any_of(messages.begin(), messages.end(), [text](std::string const& message) { return message.find(text) != std::string::npos; });
    }

    class QuestRegistryContext : public RequirementContext
    {
    public:
        std::optional<bool> HasRegistryEntry(std::string_view questName, std::string_view entryName, bool isQuestRegistry) const override
        {
            LastQuest = questName;
            LastEntry = entryName;
            LastIsQuestRegistry = isQuestRegistry;
            return Complete;
        }

        bool Complete = false;
        mutable std::string LastQuest;
        mutable std::string LastEntry;
        mutable bool LastIsQuestRegistry = false;
    };

    std::vector<std::string> Check(QuestRows const& rows, QuestValidator::KeyLookup keys = {})
    {
        return Messages(QuestValidator::Validate(rows, QuestValidator::Context::From(rows.Objects, std::move(keys))));
    }

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

        bool Contains(std::string_view text) const
        {
            for (LogMessage const& message : Store->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

        std::shared_ptr<TestAppenderStore> Store;
    };

    class QuestMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            sRequirementMgr.Clear();
            sRequirementMgr.SetRowSource([] { return RequirementRows{}; });
            sRequirementMgr.RegisterReloadTargets();
            sQuestMgr.Clear();
            _rows = std::make_shared<QuestRows>(Fixture());
            std::shared_ptr<QuestRows> const rows = _rows;
            sQuestMgr.SetRowSource([rows](QuestRows& out, std::vector<std::string>&)
            {
                out = *rows;
                return true;
            });
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            sRequirementMgr.Clear();
            sQuestMgr.Clear();
        }

        std::shared_ptr<QuestRows> _rows;
    };
}

TEST_F(QuestMgrTest, PrepEntriesIndexAsOfferedByTheFirstSpeakerOnly)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(sQuestMgr.Load(errors)) << (errors.empty() ? "" : errors.front());
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    EXPECT_EQ(quests->GetQuestsOfferedBy(HeadmasterId), std::vector<std::string>{ "PrepQuest" });
    EXPECT_TRUE(quests->GetQuestsOfferedBy(0).empty());
    EXPECT_TRUE(quests->GetQuestsOfferedBy(GammaId).empty());

    QuestInfo const* const quest = quests->Find("PrepQuest");
    ASSERT_NE(quest, nullptr);
    EXPECT_EQ(quest->Starter, HeadmasterId);
    QuestDialog const& prep = quest->Dialogs.at("Prep");
    ASSERT_EQ(prep.Lines.size(), 2u);
    EXPECT_EQ(prep.Lines[0].Row.ActorTemplateId, HeadmasterId);
    EXPECT_EQ(prep.Lines[1].Row.ActorTemplateId, 0u);
    ASSERT_EQ(prep.Lines[0].Madlibs.size(), 1u);
    EXPECT_EQ(prep.Lines[0].Madlibs[0].Value, "Headmaster");
}

TEST_F(QuestMgrTest, GoalsAreFoundByPersonaTagAdjectiveAndZone)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(sQuestMgr.Load(errors));
    std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
    EXPECT_EQ(quests->GetQuestCount(), 1u);
    EXPECT_EQ(quests->GetGoalCount(), 4u);
    EXPECT_EQ(quests->GetPersonaGoals("Headmaster"), (std::vector<QuestGoalRef>{ { "PrepQuest", "TalkToHeadmaster" } }));
    EXPECT_EQ(quests->GetBountyGoals("Undead"), (std::vector<QuestGoalRef>{ { "PrepQuest", "DefeatUndead" } }));
    EXPECT_EQ(quests->GetWaypointGoals("WizardCity/WC_Hub"), (std::vector<QuestGoalRef>{ { "PrepQuest", "VisitCommons" } }));
    EXPECT_EQ(quests->GetUsageGoalsByTag("Lamp"), (std::vector<QuestGoalRef>{ { "PrepQuest", "UseLamp" } }));
    EXPECT_EQ(quests->GetUsageGoalsByAdjective("Lit"), (std::vector<QuestGoalRef>{ { "PrepQuest", "UseLamp" } }));
    EXPECT_TRUE(quests->GetBountyGoals("Lit").empty());

    QuestInfo const* const quest = quests->Find("PrepQuest");
    ASSERT_NE(quest, nullptr);
    EXPECT_EQ(quest->StartGoals, std::vector<std::string>{ "TalkToHeadmaster" });
    QuestGoal const& talk = quest->Goals.at("TalkToHeadmaster");
    ASSERT_EQ(talk.Logic.size(), 1u);
    EXPECT_EQ(talk.Logic[0].GoalsToAdd, (std::vector<std::string>{ "DefeatUndead", "VisitCommons" }));
    EXPECT_FALSE(talk.Logic[0].CompleteQuest);
    EXPECT_EQ(talk.Dialogs.at("Complete").Lines.size(), 1u);
    EXPECT_EQ(quest->Goals.at("DefeatUndead").Logic[0].RequiresAll, std::vector<std::string>{ "VisitCommons" });
}

TEST(QuestMgrValidatorTest, TheFixtureHasNoErrors)
{
    EXPECT_TRUE(Check(Fixture()).empty());
}

TEST(QuestMgrValidatorTest, RejectsGoalsToAddNamingAMissingGoal)
{
    QuestRows rows = Fixture();
    rows.LogicMembers.push_back(QuestGoalLogicMemberRow{ "PrepQuest", "TalkToHeadmaster", 0, "ADD", 2, "FindTheMissingGoal" });
    std::vector<std::string> const errors = Check(rows);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(Holds(errors, "goalsToAdd"));
    EXPECT_TRUE(Holds(errors, "FindTheMissingGoal"));
}

TEST(QuestMgrValidatorTest, RejectsABountyGoalWithNoAdjectives)
{
    QuestRows rows = Fixture();
    rows.Adjectives.erase(rows.Adjectives.begin());
    std::vector<std::string> const errors = Check(rows);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(Holds(errors, "bounty goal DefeatUndead has no adjective"));
}

TEST(QuestMgrValidatorTest, RejectsABountyGoalWithATallyCountOfZero)
{
    QuestRows rows = Fixture();
    rows.Goals[1].TallyCount = 0;
    std::vector<std::string> const errors = Check(rows);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(Holds(errors, "bounty goal DefeatUndead has a tally_count of 0"));
}

TEST(QuestMgrValidatorTest, RejectsAPersonaNoObjectIsNamed)
{
    QuestRows rows = Fixture();
    rows.Goals[0].PersonaName = "Nobody";
    std::vector<std::string> const errors = Check(rows);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(Holds(errors, "persona goal TalkToHeadmaster names Nobody, which is no object_template object_name"));
}

TEST(QuestMgrValidatorTest, RejectsALogicEntryThatBothCompletesAndAddsGoals)
{
    QuestRows rows = Fixture();
    rows.Logic[0].CompleteQuest = true;
    std::vector<std::string> const errors = Check(rows);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(Holds(errors, "both completes the quest (completeQuest) and adds goals (goalsToAdd)"));
}

TEST(QuestMgrValidatorTest, RejectsAStarterWithoutAPrepDialogAndAnUnknownActor)
{
    QuestRows rows = Fixture();
    std::erase_if(rows.DialogEntries, [](QuestDialogEntryRow const& row) { return row.Tag == "Prep"; });
    std::erase_if(rows.Madlibs, [](QuestDialogMadlibRow const& row) { return row.Tag == "Prep"; });
    rows.DialogEntries.push_back(Line("PrepQuest", "Underway", 0, 99999));
    rows.Dialogs.push_back(QuestDialogRow{ "PrepQuest", "quest", "", "Underway" });
    std::vector<std::string> const errors = Check(rows);
    EXPECT_EQ(errors.size(), 2u);
    EXPECT_TRUE(Holds(errors, "has a starter, but it has no Prep dialog line"));
    EXPECT_TRUE(Holds(errors, "names actor template 99999, which object_template does not hold"));
}

TEST(QuestMgrValidatorTest, ChecksKeysOnlyWhenLocaleTextIsAvailable)
{
    QuestRows const rows = Fixture();
    EXPECT_TRUE(Check(rows).empty());
    std::vector<std::string> const errors = Check(rows, [](std::string_view key) { return key != "QuestTitle_PrepQuest"; });
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(Holds(errors, "title_key QuestTitle_PrepQuest does not resolve"));
}

TEST_F(QuestMgrTest, StartSkipsInvalidQuestsAndLogsTheCounts)
{
    _rows->Quests.push_back(Quest("BrokenQuest"));
    QuestGoalRow broken = Goal("BrokenQuest", "Hunt", "bounty");
    _rows->Goals.push_back(broken);

    CapturedLog log;
    QuestLoadResult const result = sQuestMgr.LoadSkippingInvalid();
    EXPECT_TRUE(result.Loaded);
    EXPECT_EQ(result.Quests, 1u);
    EXPECT_EQ(result.Goals, 4u);
    EXPECT_EQ(result.Errors.size(), 2u);
    EXPECT_TRUE(log.Contains("Loaded 1 quests, 4 goals, 2 validation errors"));
    EXPECT_EQ(sQuestMgr.GetQuests()->Find("BrokenQuest"), nullptr);
    EXPECT_NE(sQuestMgr.GetQuests()->Find("PrepQuest"), nullptr);
}

TEST_F(QuestMgrTest, ReloadSwapsInAnAddedQuestAndIndexesItsStarter)
{
    sQuestMgr.RegisterReloadTargets();
    ASSERT_TRUE(sReloadMgr.Reload(QuestMgr::Target).Ok);
    uint64 const generation = sQuestMgr.GetGeneration();
    std::shared_ptr<QuestStore const> const before = sQuestMgr.GetQuests();
    EXPECT_TRUE(before->GetQuestsOfferedBy(GammaId).empty());

    _rows->Quests.push_back(Quest("GammaQuest"));
    QuestGoalRow talk = Goal("GammaQuest", "TalkToGamma", "persona");
    talk.PersonaName = "Gamma";
    _rows->Goals.push_back(talk);
    AddStarter(*_rows, "GammaQuest", GammaId);

    CapturedLog log;
    ReloadOutcome const outcome = sReloadMgr.Reload(QuestMgr::Target);
    ASSERT_TRUE(outcome.Ok) << (outcome.Errors.empty() ? "" : outcome.Errors.front());
    EXPECT_GT(sQuestMgr.GetGeneration(), generation);
    std::shared_ptr<QuestStore const> const after = sQuestMgr.GetQuests();
    EXPECT_EQ(after->GetQuestsOfferedBy(GammaId), std::vector<std::string>{ "GammaQuest" });
    EXPECT_EQ(after->GetQuestsOfferedBy(HeadmasterId), std::vector<std::string>{ "PrepQuest" });
    EXPECT_EQ(after->GetPersonaGoals("Gamma"), (std::vector<QuestGoalRef>{ { "GammaQuest", "TalkToGamma" } }));
    EXPECT_TRUE(before->GetQuestsOfferedBy(GammaId).empty());
    EXPECT_TRUE(log.Contains("Loaded 2 quests, 5 goals, 0 validation errors"));
}

TEST_F(QuestMgrTest, ReloadThatIntroducesErrorsKeepsTheOldSnapshotAndReportsEveryError)
{
    sQuestMgr.RegisterReloadTargets();
    ASSERT_TRUE(sReloadMgr.Reload(QuestMgr::Target).Ok);
    std::shared_ptr<QuestStore const> const serving = sQuestMgr.GetQuests();
    uint64 const generation = sQuestMgr.GetGeneration();

    _rows->Quests.push_back(Quest("GammaQuest"));
    AddStarter(*_rows, "GammaQuest", GammaId);
    _rows->Goals[0].PersonaName = "Nobody";
    _rows->Goals[1].TallyCount = 0;
    _rows->Logic[0].CompleteQuest = true;

    ReloadOutcome const outcome = sReloadMgr.Reload(QuestMgr::Target);
    EXPECT_FALSE(outcome.Ok);
    EXPECT_EQ(outcome.Errors.size(), 3u);
    EXPECT_TRUE(Holds(outcome.Errors, "names Nobody"));
    EXPECT_TRUE(Holds(outcome.Errors, "tally_count of 0"));
    EXPECT_TRUE(Holds(outcome.Errors, "both completes the quest"));
    EXPECT_EQ(sQuestMgr.GetQuests(), serving);
    EXPECT_EQ(sQuestMgr.GetGeneration(), generation);
    EXPECT_TRUE(sQuestMgr.GetQuests()->GetQuestsOfferedBy(GammaId).empty());
    EXPECT_EQ(sQuestMgr.GetQuests()->GetQuestsOfferedBy(HeadmasterId), std::vector<std::string>{ "PrepQuest" });
}

TEST_F(QuestMgrTest, QuestRegistryCompletionGatesQuestAvailability)
{
    _rows->Quests.front().RequirementListId = "q1-complete";
    auto requirements = std::make_shared<RequirementRows>();
    requirements->Lists.push_back(RequirementListRow{ "q1-complete" });
    RequirementRow completed;
    completed.ListId = "q1-complete";
    completed.Type = "ReqHasEntry";
    completed.QuestName = "Q1";
    completed.EntryName = "Complete";
    completed.IsQuestRegistry = true;
    requirements->Requirements.push_back(completed);
    sRequirementMgr.SetRowSource([requirements] { return *requirements; });

    ReloadOutcome const requirementReload = sReloadMgr.Reload(RequirementMgr::Target);
    ASSERT_TRUE(requirementReload.Ok) << (requirementReload.Errors.empty() ? "" : requirementReload.Errors.front());
    sQuestMgr.RegisterReloadTargets();
    ReloadOutcome const outcome = sReloadMgr.Reload(QuestMgr::Target);
    ASSERT_TRUE(outcome.Ok) << (outcome.Errors.empty() ? "" : outcome.Errors.front());

    QuestRegistryContext context;
    EXPECT_FALSE(sQuestMgr.CanOffer("PrepQuest", context));
    EXPECT_TRUE(sQuestMgr.GetQuestsOfferedBy(HeadmasterId, context).empty());
    EXPECT_EQ(context.LastQuest, "Q1");
    EXPECT_EQ(context.LastEntry, "Complete");
    EXPECT_TRUE(context.LastIsQuestRegistry);

    context.Complete = true;
    EXPECT_TRUE(sQuestMgr.CanOffer("PrepQuest", context));
    EXPECT_EQ(sQuestMgr.GetQuestsOfferedBy(HeadmasterId, context), std::vector<std::string>{ "PrepQuest" });
}
