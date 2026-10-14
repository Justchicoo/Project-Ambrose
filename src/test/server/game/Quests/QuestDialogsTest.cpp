/*
 * Project Ambrose by Imjustchico
 * Tests the quest dialogs a wizard's client is playing: an entry event MSG_COMPLETEDIALOG reports fires QuestScript::OnDialogEvent exactly once, a repeat and an event no played line carries fire nothing, and a dialog closes when its last event fires or the client reports it closed.
 */

#include "QuestDialogs.h"
#include "ScriptMgr.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    constexpr uint64 Wizard = 7;
    constexpr uint64 Npc = 4503599627370521;

    std::vector<std::string> Heard;

    class HeardQuestScript : public QuestScript
    {
    public:
        HeardQuestScript() : QuestScript("heard_dialog_events") {}

        void OnDialogEvent(uint64 wizard, std::string_view quest, std::string_view event) override
        {
            Heard.push_back(std::to_string(wizard) + ":" + std::string(quest) + ":" + std::string(event));
        }
    };

    class QuestDialogsTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            sScriptMgr.Unload();
            Heard.clear();
            new HeardQuestScript();
        }

        void TearDown() override
        {
            sScriptMgr.Unload();
        }
    };

    Dialogs::ActorDialog Lines(std::vector<std::string> const& events)
    {
        Dialogs::ActorDialog dialog;
        dialog.Tag = "Start";
        for (std::string const& event : events)
        {
            Dialogs::ActorDialogEntry entry;
            entry.DialogKey = "Dialog_" + event;
            entry.DialogEvent = event;
            dialog.Entries.push_back(entry);
        }
        return dialog;
    }
}

TEST_F(QuestDialogsTest, AnEntryEventInCompleteDialogFiresOnDialogEventExactlyOnce)
{
    QuestDialogs dialogs;
    dialogs.Play(Npc, QuestDialogs::QuestStart, "Errand", Lines({ "", "PlayGateCinematic", "OpenGate" }));

    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::QuestStart, "PlayGateCinematic"), std::optional<std::string>("Errand"));
    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::QuestStart, "PlayGateCinematic"), std::nullopt) << "a repeated event fires nothing";
    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::QuestStart, "NotInTheDialog"), std::nullopt);
    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::QuestComplete, "OpenGate"), std::nullopt) << "an event is matched to the dialog of its completion type";
    EXPECT_EQ(Heard, (std::vector<std::string>{ "7:Errand:PlayGateCinematic" }));

    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::QuestStart, "OpenGate"), std::optional<std::string>("Errand"));
    EXPECT_EQ(Heard.size(), 2u);
    dialogs.Play(Npc, QuestDialogs::QuestComplete, "Errand", Lines({ "OpenGate" }));
    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::QuestComplete, "OpenGate"), std::optional<std::string>("Errand")) << "another dialog of the quest fires its own events";
}

TEST_F(QuestDialogsTest, ADialogTheClientReportsClosedFiresNothingMore)
{
    QuestDialogs dialogs;
    dialogs.Play(Npc, QuestDialogs::Underway, "Errand", Lines({ "Remind" }));
    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::Underway, ""), std::nullopt);
    EXPECT_EQ(dialogs.Complete(Wizard, Npc, QuestDialogs::Underway, "Remind"), std::nullopt);
    EXPECT_TRUE(Heard.empty());
}
