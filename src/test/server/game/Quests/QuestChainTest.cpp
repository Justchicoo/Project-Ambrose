/*
 * Project Ambrose by Imjustchico
 * Tests the quest chaining without client data: NPC templates placed within the radius of each other in one zone form one stack named by its lowest id, the same spot in another zone or a step too far does not, and a chain of near placements joins them all; a quest's turn-in NPC comes from the persona its completing goal names, with or without the _Persona suffix, and none when a completing goal is not a persona goal; a quest the package does not start is started by its prior quest's turn-in NPC, a starter that is a copy of that NPC counts as agreeing, and priors turned in to two different NPCs start nothing; where the wiki giver names several templates, the turn-in NPC is kept only when it or a copy of it is among them, else the one of them placed in its zone, else none, and two templates share a zone only when both are placed in it; a quest every school takes is neither gated on nor started from a quest only one school takes, while another quest of that school is.
 */

#include "QuestChain.h"
#include "QuestNpcStacks.h"

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    QuestPackageQuest Quest(std::string const& name, std::string const& persona, uint32 starter = 0)
    {
        QuestPackageQuest quest;
        quest.Name = name;
        QuestTemplateRow row;
        row.Name = name;
        row.StarterTemplateId = starter;
        quest.Rows.Quests.push_back(row);
        QuestGoalRow goal;
        goal.Quest = name;
        goal.Goal = "TurnIn";
        goal.Type = "persona";
        goal.PersonaName = persona;
        quest.Rows.Goals.push_back(goal);
        QuestGoalLogicRow logic;
        logic.Quest = name;
        logic.Goal = "TurnIn";
        logic.CompleteQuest = true;
        quest.Rows.Logic.push_back(logic);
        return quest;
    }

    QuestChain::ObjectLookup Names(std::map<std::string, uint32, std::less<>> const& names)
    {
        return [names](std::string_view name) -> std::optional<uint32>
        {
            auto const found = names.find(name);
            return found != names.end() ? std::optional<uint32>(found->second) : std::nullopt;
        };
    }
}

TEST(QuestNpcStacksTest, JoinsTemplatesOnOneSpotInOneZone)
{
    QuestNpcStacks const stacks = QuestNpcStacks::Build({
        { "WC_Hub", 300, 10.0f, 20.0f, 0.0f },
        { "WC_Hub", 200, 11.0f, 20.0f, 0.0f },
        { "WC_Hub", 400, 30.0f, 20.0f, 0.0f },
        { "WC_Ravenwood", 500, 10.0f, 20.0f, 0.0f },
    }, 5.0f);
    EXPECT_EQ(stacks.StackOf(300), 200u);
    EXPECT_EQ(stacks.StackOf(200), 200u);
    EXPECT_TRUE(stacks.Same(200, 300));
    EXPECT_FALSE(stacks.Same(200, 400));
    EXPECT_FALSE(stacks.Same(200, 500));
    EXPECT_EQ(stacks.GetStackCount(), 1u);
    EXPECT_EQ(stacks.GetStackedTemplates(), 2u);
    EXPECT_EQ(stacks.OneOf({ 300, 200 }), std::optional<uint32>(200));
    EXPECT_EQ(stacks.OneOf({ 300, 400 }), std::nullopt);
    EXPECT_EQ(stacks.OneOf({}), std::nullopt);
}

TEST(QuestNpcStacksTest, JoinsAChainOfNearPlacements)
{
    QuestNpcStacks const stacks = QuestNpcStacks::Build({
        { "Zone", 3, 0.0f, 0.0f, 0.0f },
        { "Zone", 2, 4.0f, 0.0f, 0.0f },
        { "Zone", 1, 8.0f, 0.0f, 0.0f },
    }, 5.0f);
    EXPECT_EQ(stacks.StackOf(3), 1u);
    EXPECT_EQ(stacks.GetStackCount(), 1u);
    EXPECT_EQ(stacks.GetStackedTemplates(), 3u);
}

TEST(QuestChainTest, TurnInActorReadsTheCompletingPersona)
{
    QuestChain::ObjectLookup const objects = Names({ { "Headmaster", 38232 } });
    EXPECT_EQ(QuestChain::TurnInActor(Quest("A", "Headmaster"), objects), std::optional<uint32>(38232));
    EXPECT_EQ(QuestChain::TurnInActor(Quest("B", "Headmaster_Persona"), objects), std::optional<uint32>(38232));
    QuestPackageQuest notPersona = Quest("C", "Headmaster");
    notPersona.Rows.Goals.front().Type = "waypoint";
    EXPECT_EQ(QuestChain::TurnInActor(notPersona, objects), std::nullopt);
    EXPECT_EQ(QuestChain::TurnInActor(Quest("D", "Nobody"), objects), std::nullopt);
}

TEST(QuestChainTest, StartsTheNextQuestFromThePriorTurnIn)
{
    QuestNpcStacks const stacks = QuestNpcStacks::Build({
        { "WC_Hub", 100, 0.0f, 0.0f, 0.0f },
        { "WC_Hub", 101, 0.5f, 0.0f, 0.0f },
    });
    QuestPackageRead read;
    read.Quests.push_back(Quest("First", "Ambrose"));
    read.Quests.push_back(Quest("Second", "Ambrose"));
    read.Quests.push_back(Quest("Third", "Gamma", 101));
    read.Quests.push_back(Quest("Other", "Delta"));
    read.Quests.push_back(Quest("Fourth", "Ambrose"));
    QuestChain::ObjectLookup const objects = Names({ { "Ambrose", 100 }, { "Gamma", 200 }, { "Delta", 300 } });
    std::vector<QuestLink> const links = { { "First", "Second" }, { "Second", "Third" }, { "Third", "Fourth" }, { "Other", "Fourth" } };

    QuestChainMatch const match = QuestChain::Apply(nullptr, {}, links, {}, read, objects, &stacks);
    EXPECT_EQ(read.Quests[1].Rows.Quests.front().StarterTemplateId, 100u);
    EXPECT_EQ(read.Quests[4].Rows.Quests.front().StarterTemplateId, 0u);
    EXPECT_EQ(match.StartersFilled, 1u);
    EXPECT_EQ(match.StartersChecked, 1u);
    EXPECT_EQ(match.StartersAgreeing, 1u);
    EXPECT_EQ(match.StartersConflicting, 1u);
    EXPECT_EQ(match.WithStarter, 2u);
}

TEST(QuestChainTest, AWikiGiverOfSeveralTemplatesKeepsOnlyAStarterItNames)
{
    QuestNpcStacks const stacks = QuestNpcStacks::Build({
        { "WC_Hub", 100, 0.0f, 0.0f, 0.0f },
        { "WC_Hub", 101, 0.5f, 0.0f, 0.0f },
        { "WC_Hub", 400, 90.0f, 0.0f, 0.0f },
        { "WC_Ravenwood", 500, 0.0f, 0.0f, 0.0f },
        { "WC_Ravenwood", 600, 90.0f, 0.0f, 0.0f },
    });
    QuestPackageRead read;
    read.Quests.push_back(Quest("First", "Ambrose"));
    read.Quests.push_back(Quest("Named", "Ambrose"));
    read.Quests.push_back(Quest("Nearby", "Ambrose"));
    read.Quests.push_back(Quest("Elsewhere", "Ambrose"));
    QuestChain::ObjectLookup const objects = Names({ { "Ambrose", 100 } });
    std::vector<QuestLink> const links = { { "First", "Named" }, { "First", "Nearby" }, { "First", "Elsewhere" } };
    QuestGiverChoices const choices = { { "Named", { 101, 500 } }, { "Nearby", { 400, 500 } }, { "Elsewhere", { 500, 600 } } };

    QuestChainMatch const match = QuestChain::Apply(nullptr, {}, links, choices, read, objects, &stacks);
    EXPECT_EQ(read.Quests[1].Rows.Quests.front().StarterTemplateId, 100u);
    EXPECT_EQ(read.Quests[2].Rows.Quests.front().StarterTemplateId, 400u);
    EXPECT_EQ(read.Quests[3].Rows.Quests.front().StarterTemplateId, 0u);
    EXPECT_EQ(match.StartersFromGiverChoice, 1u);
    EXPECT_EQ(match.StartersFromGiverZone, 1u);
    EXPECT_EQ(match.StartersLeftToGiver, 1u);
    EXPECT_EQ(match.StartersFilled, 2u);
}

TEST(QuestNpcStacksTest, SaysWhetherTwoTemplatesShareAZone)
{
    QuestNpcStacks const stacks = QuestNpcStacks::Build({ { "A", 1, 0.0f, 0.0f, 0.0f }, { "A", 2, 90.0f, 0.0f, 0.0f }, { "B", 3, 0.0f, 0.0f, 0.0f } });
    EXPECT_TRUE(stacks.ShareZone(1, 2));
    EXPECT_FALSE(stacks.ShareZone(1, 3));
    EXPECT_FALSE(stacks.ShareZone(1, 9));
}

TEST(QuestChainTest, AQuestEverySchoolTakesIsNotChainedToOneOnlyOneSchoolTakes)
{
    QuestPackageRead read;
    read.Quests.push_back(Quest("FireOnly", "Falmea"));
    read.Quests.back().SchoolGated = true;
    read.Quests.push_back(Quest("Everyone", "Ambrose"));
    read.Quests.push_back(Quest("FireNext", "Ambrose"));
    read.Quests.back().SchoolGated = true;
    QuestChain::ObjectLookup const objects = Names({ { "Falmea", 700 } });
    std::vector<QuestArc> const arcs = { { "Story", { "FireOnly", "Everyone" } } };
    std::vector<QuestLink> const links = { { "FireOnly", "FireNext" } };

    QuestChainMatch const match = QuestChain::Apply(nullptr, arcs, links, {}, read, objects);
    EXPECT_EQ(match.ArcLinksImported, 1u);
    EXPECT_EQ(match.ArcLinksFromOneSchool, 1u);
    EXPECT_EQ(match.GatesAdded + match.GatesNotEncoded, 0u);
    EXPECT_EQ(read.Quests[1].Rows.Quests.front().StarterTemplateId, 0u);
    EXPECT_EQ(read.Quests[2].Rows.Quests.front().StarterTemplateId, 700u);
}
