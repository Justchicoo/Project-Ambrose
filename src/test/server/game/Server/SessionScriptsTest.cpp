/*
 * Project Ambrose by Imjustchico
 * Tests what the install's Lua scripts reach a session through: a ResSyncScript result becomes a call of its function in its script for the wizard and zone it runs for, while another class or an empty script or function is refused with the reason; ScriptCompleteQuest finishes a quest the wizard holds or lacks and leaves one it has finished alone; and CompleteQuestChain finishes every quest whose name continues the chain's and none whose name only starts like it.
 */

#include "GuidGenerator.h"
#include "ObjectGuid.h"
#include "QuestLog.h"
#include "QuestStore.h"
#include "ResultFixtures.h"
#include "SessionScripts.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 Now = 1791650000;

    PropertyObjectPtr SyncScript(std::string const& script, std::string const& function)
    {
        PropertyObjectPtr object = PropertyObject::Create(ResultFixtures::Catalog(), SessionScripts::SyncScriptClass);
        EXPECT_TRUE(object);
        if (object)
        {
            object->Set("m_script", std::string(script));
            object->Set("m_function", std::string(function));
        }
        return object;
    }

    ResultContext Context()
    {
        ResultContext context;
        context.Wizard = 0x1000000000ull;
        context.MapId = 12;
        context.Source = "quest WC-GNT-C01-001";
        return context;
    }

    std::shared_ptr<QuestStore const> Quests(std::vector<std::string> const& names)
    {
        QuestRows rows;
        for (std::string const& name : names)
        {
            QuestTemplateRow quest;
            quest.Name = name;
            quest.Level = 1;
            rows.Quests.push_back(quest);
        }
        return QuestStore::Build(rows);
    }
}

TEST(SessionScriptsTest, ASyncScriptResultIsACallOfItsFunctionForTheWizardAndZoneItRunsFor)
{
    PropertyObjectPtr const result = SyncScript("Scripts/WorldBuilding/CompleteWC.lua", "OpenKrok");
    ASSERT_TRUE(result);
    std::string error;
    std::optional<LuaScriptCall> const call = SessionScripts::SyncCallOf(*result, Context(), error);
    ASSERT_TRUE(call) << error;
    EXPECT_EQ(call->Script, "Scripts/WorldBuilding/CompleteWC.lua");
    EXPECT_EQ(call->Function, "OpenKrok");
    EXPECT_EQ(call->GameObjectId, 0x1000000000ull);
    EXPECT_EQ(call->ZoneId, 12u);
    EXPECT_EQ(call->Session, nullptr);
    EXPECT_EQ(call->CharacterId, 0u);
}

TEST(SessionScriptsTest, AnotherClassOrAnEmptyScriptOrFunctionIsRefusedWithTheReason)
{
    std::string error;
    ResultEntry const notify = ResultFixtures::NotifyText("hello", 0);
    ASSERT_TRUE(notify.Object);
    EXPECT_FALSE(SessionScripts::SyncCallOf(*notify.Object, Context(), error));
    EXPECT_EQ(error, "it holds a class ResClientNotifyText");
    PropertyObjectPtr const noFunction = SyncScript("Scripts/WorldBuilding/CompleteWC.lua", "");
    ASSERT_TRUE(noFunction);
    EXPECT_FALSE(SessionScripts::SyncCallOf(*noFunction, Context(), error));
    EXPECT_EQ(error, "its m_script or m_function is empty");
}

TEST(SessionScriptsTest, CompleteQuestFinishesAQuestTheWizardHoldsOrLacksAndLeavesAFinishedOneAlone)
{
    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    ASSERT_NE(log.Add("Held", gids, Now), nullptr);
    EXPECT_TRUE(SessionScripts::CompleteQuest(log, "Held", Now));
    EXPECT_TRUE(log.HasCompletedQuest("Held"));
    EXPECT_FALSE(log.HasQuest("Held"));
    EXPECT_TRUE(SessionScripts::CompleteQuest(log, "Lacked", Now)) << "a quest the wizard never took is given first, then finished";
    EXPECT_TRUE(log.HasCompletedQuest("Lacked"));
    EXPECT_FALSE(SessionScripts::CompleteQuest(log, "Held", Now)) << "a finished quest is not finished twice";
}

TEST(SessionScriptsTest, CompleteQuestChainFinishesEveryQuestWhoseNameContinuesTheChains)
{
    std::shared_ptr<QuestStore const> const quests = Quests({ "WC-GNT-C01-001", "WC-GNT-C01-002", "WC-GNT-C02-001", "WC-GNT-C01X-001", "KT-GNT-C01-001" });
    ASSERT_TRUE(quests);
    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    ASSERT_TRUE(SessionScripts::CompleteQuest(log, "WC-GNT-C01-001", Now));
    EXPECT_EQ(SessionScripts::CompleteChain(log, *quests, "WC-GNT-C01", Now), 1u) << "001 was already finished, so only 002 is";
    EXPECT_TRUE(log.HasCompletedQuest("WC-GNT-C01-002"));
    EXPECT_FALSE(log.HasCompletedQuest("WC-GNT-C01X-001"));
    EXPECT_FALSE(log.HasCompletedQuest("WC-GNT-C02-001"));
    EXPECT_EQ(SessionScripts::CompleteChain(log, *quests, "WC", Now), 3u);
    EXPECT_FALSE(log.HasCompletedQuest("KT-GNT-C01-001"));
}
