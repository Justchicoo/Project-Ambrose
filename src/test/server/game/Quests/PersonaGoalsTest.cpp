/*
 * Project Ambrose by Imjustchico
 * Tests talk-to goals: a persona goal naming WC-RAV-NPC06 binds to template 38232, whose object name that is, and not to a twin template showing the same display name under another object name; a talk-to goal with no image of its own shows the NPC's portrait icon while an authored image stays; the NPC offers it as a GoalEntryFull carrying the quest's and goal's GIDs while the goal is active, and picking it hands that goal on; and MSG_COMPLETEPERSONA's check refuses a goal the wizard does not hold, one already complete, one that is not a persona goal, a wrong NPC and a wizard out of range, changing nothing.
 */

#include "ObjectGuid.h"
#include "PersonaGoalProvider.h"
#include "PersonaGoals.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr uint32 StudentId = 38232;
    constexpr uint32 TwinId = 40002;
    constexpr uint64 Npc = 4503599627370521;

    std::shared_ptr<QuestStore const> Store()
    {
        QuestRows rows;
        rows.Objects = { { 1, "Player" }, { StudentId, "WC-RAV-NPC06" }, { TwinId, "WC-RAV-NPC06-Twin" } };
        QuestTemplateRow quest;
        quest.Name = "Errand";
        quest.TitleKey = "QuestTitle_Errand";
        quest.Level = 1;
        rows.Quests.push_back(quest);
        QuestGoalRow talk;
        talk.Quest = "Errand";
        talk.Goal = "TalkToStudent";
        talk.Type = "persona";
        talk.TitleKey = "WizardQuestGoals_TalkNPC";
        talk.PersonaName = "WC-RAV-NPC06";
        rows.Goals.push_back(talk);
        QuestGoalRow defeat;
        defeat.Quest = "Errand";
        defeat.Goal = "DefeatUndead";
        defeat.Type = "bounty";
        defeat.TallyCount = 5;
        rows.Goals.push_back(defeat);
        rows.StartGoals.push_back(QuestStartGoalRow{ "Errand", "TalkToStudent" });
        rows.StartGoals.push_back(QuestStartGoalRow{ "Errand", "DefeatUndead" });
        return QuestStore::Build(rows);
    }

    struct Wizard
    {
        GuidGenerator Gids{ ObjectGuid::QuestBase };
        QuestLog Log;
        uint64 Quest = 0;
        uint64 Talk = 0;
        uint64 Defeat = 0;

        Wizard()
        {
            Quest = Log.Add("Errand", Gids, 1791650000)->Gid;
            Talk = Log.StartGoal("Errand", "TalkToStudent", Gids)->Gid;
            Defeat = Log.StartGoal("Errand", "DefeatUndead", Gids)->Gid;
        }
    };
}

TEST(PersonaGoalsTest, AGoalNamingWcRavNpc06BindsToItsTemplateAndNotToADisplayNameTwin)
{
    std::shared_ptr<QuestStore const> const quests = Store();
    Wizard wizard;
    std::vector<PersonaGoals::Active> const student = PersonaGoals::For(*quests, wizard.Log, StudentId);
    ASSERT_EQ(student.size(), 1u);
    EXPECT_EQ(student[0].HeldGoal->Gid, wizard.Talk);
    EXPECT_TRUE(PersonaGoals::For(*quests, wizard.Log, TwinId).empty());
    EXPECT_TRUE(PersonaGoalProvider::Serves(*quests, StudentId));
    EXPECT_FALSE(PersonaGoalProvider::Serves(*quests, TwinId));

    std::vector<PersonaGoals::Active> picked;
    PersonaGoalProvider provider(quests, wizard.Log, [&picked](NpcServiceTarget const&, PersonaGoals::Active const& goal) { picked.push_back(goal); });
    NpcServiceTarget const target{ 1, Npc, StudentId };
    std::vector<NpcServiceOption> const options = provider.GetServiceOptions(target);
    ASSERT_EQ(options.size(), 1u);
    EXPECT_EQ(options[0].ServiceName, PersonaGoals::ServiceName);
    EXPECT_EQ(options[0].IconKey, PersonaGoals::CompleteIcon);
    EXPECT_EQ(options[0].DisplayKey, "QuestTitle_Errand");
    ASSERT_TRUE(options[0].Goal.has_value());
    EXPECT_EQ(options[0].Goal->QuestId, wizard.Quest);
    EXPECT_EQ(options[0].Goal->GoalId, wizard.Talk);
    EXPECT_EQ(options[0].Goal->GoalTitle, "WizardQuestGoals_TalkNPC");
    EXPECT_EQ(options[0].Goal->PersonaName, "WC-RAV-NPC06");
    provider.OnServiceInteraction(target, 0);
    ASSERT_EQ(picked.size(), 1u);
    EXPECT_EQ(picked[0].HeldGoal->Name, "TalkToStudent");

    ASSERT_TRUE(wizard.Log.CompleteGoal("Errand", "TalkToStudent"));
    EXPECT_TRUE(provider.GetServiceOptions(target).empty()) << "a completed goal is not offered again";
}

TEST(PersonaGoalsTest, CompletePersonaIsRefusedForAnyGoalNpcOrPlaceItDoesNotFit)
{
    std::shared_ptr<QuestStore const> const quests = Store();
    Wizard wizard;
    uint64 const before = wizard.Log.GetChangeCount();
    PersonaGoals::Active found;
    EXPECT_EQ(PersonaGoals::Check(*quests, wizard.Log, wizard.Quest + 999, wizard.Talk, StudentId, true, found), PersonaGoals::Verdict::NotHeld);
    EXPECT_EQ(PersonaGoals::Check(*quests, wizard.Log, wizard.Quest, wizard.Talk + 999, StudentId, true, found), PersonaGoals::Verdict::NotHeld);
    EXPECT_EQ(PersonaGoals::Check(*quests, wizard.Log, wizard.Quest, wizard.Defeat, StudentId, true, found), PersonaGoals::Verdict::NotPersona);
    EXPECT_EQ(PersonaGoals::Check(*quests, wizard.Log, wizard.Quest, wizard.Talk, TwinId, true, found), PersonaGoals::Verdict::WrongNpc);
    EXPECT_EQ(PersonaGoals::Check(*quests, wizard.Log, wizard.Quest, wizard.Talk, StudentId, false, found), PersonaGoals::Verdict::OutOfRange);
    EXPECT_EQ(found.Goal, nullptr) << "nothing is handed back for a refused claim";
    EXPECT_EQ(wizard.Log.GetChangeCount(), before);
    EXPECT_TRUE(wizard.Log.IsGoalActive("Errand", "TalkToStudent"));

    ASSERT_EQ(PersonaGoals::Check(*quests, wizard.Log, wizard.Quest, wizard.Talk, StudentId, true, found), PersonaGoals::Verdict::Accepted);
    EXPECT_EQ(found.HeldGoal->Gid, wizard.Talk);
    ASSERT_TRUE(wizard.Log.CompleteGoal("Errand", "TalkToStudent"));
    EXPECT_EQ(PersonaGoals::Check(*quests, wizard.Log, wizard.Quest, wizard.Talk, StudentId, true, found), PersonaGoals::Verdict::NotActive);
}

TEST(PersonaGoalsTest, ATalkToGoalWithNoImageShowsItsNpcsPortraitAndOneWithAnImageKeepsIt)
{
    QuestRows rows;
    rows.Objects = { { StudentId, "WC-RAV-NPC06", "GUI/NpcPortraits/Student.dds" }, { TwinId, "WC-RAV-NPC06-Twin", "" } };
    QuestTemplateRow quest;
    quest.Name = "Errand";
    rows.Quests.push_back(quest);
    for (std::string const name : { "Plain", "Drawn", "Twin" })
    {
        QuestGoalRow talk;
        talk.Quest = "Errand";
        talk.Goal = name;
        talk.Type = "persona";
        talk.PersonaName = name == "Twin" ? "WC-RAV-NPC06-Twin" : "WC-RAV-NPC06";
        talk.Image1 = name == "Drawn" ? "GUI/NpcPortraits/Authored.dds" : "";
        rows.Goals.push_back(talk);
    }
    std::shared_ptr<QuestStore const> const quests = QuestStore::Build(rows);
    QuestInfo const* const info = quests->Find("Errand");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->Goals.at("Plain").Row.Image1, "GUI/NpcPortraits/Student.dds");
    EXPECT_EQ(info->Goals.at("Drawn").Row.Image1, "GUI/NpcPortraits/Authored.dds");
    EXPECT_EQ(info->Goals.at("Twin").Row.Image1, "") << "an NPC with no icon leaves the goal as it was";
}
