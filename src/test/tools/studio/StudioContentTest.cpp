/*
 * Project Ambrose by Imjustchico
 * Tests what the Studio shows from quest rows with no install behind them: quests come back sorted and paged and found by name or locale title, one quest carries its goals, dialog lines with their text and results with their timing, a missing quest or cinematic answers 404 through the channel, a message that is not an object answers nothing, and a cinematic's key ignores its folder, its case and its .xml.
 */

#include "StudioChannel.h"
#include "StudioContent.h"

#include <gtest/gtest.h>

namespace
{
    QuestRows SampleRows()
    {
        QuestRows rows;
        QuestTemplateRow unicorn;
        unicorn.Name = "WC-COMMONS-MAIN-001";
        unicorn.TitleKey = "QuestTitle_1";
        unicorn.Level = 1;
        unicorn.Mainline = true;
        QuestTemplateRow ravenwood;
        ravenwood.Name = "WC-COMMONS-MAIN-002";
        ravenwood.TitleKey = "QuestTitle_2";
        rows.Quests = { ravenwood, unicorn };
        QuestGoalRow talk;
        talk.Quest = unicorn.Name;
        talk.Goal = "TalkToLincoln";
        talk.Type = "persona";
        rows.Goals = { talk };
        QuestDialogEntryRow line;
        line.Quest = unicorn.Name;
        line.Tag = "Prep";
        line.DialogKey = "Dialog_1";
        rows.DialogEntries = { line };
        QuestResultRow teleport;
        teleport.Quest = unicorn.Name;
        teleport.Class = "class ResTeleport";
        teleport.Start = true;
        rows.Results = { teleport };
        return rows;
    }

    StudioContent SampleContent()
    {
        return StudioContent(SampleRows(), nullptr, nullptr, [](std::string_view key) -> std::optional<std::string>
        {
            if (key == "QuestTitle_1")
                return "Unicorn Way";
            if (key == "QuestTitle_2")
                return "To Ravenwood!";
            if (key == "Dialog_1")
                return "Welcome to Wizard City.";
            return std::nullopt;
        });
    }
}

TEST(StudioContentTest, QuestsAreSortedPagedAndFoundByNameOrTitle)
{
    StudioContent const content = SampleContent();
    nlohmann::ordered_json const all = content.Quests("", 0, 0);
    ASSERT_EQ(all["total"], 2);
    EXPECT_EQ(all["items"][0]["name"], "WC-COMMONS-MAIN-001");
    EXPECT_EQ(all["items"][0]["title"], "Unicorn Way");
    EXPECT_EQ(all["items"][0]["goals"], 1);

    nlohmann::ordered_json const second = content.Quests("", 1, 1);
    EXPECT_EQ(second["total"], 2);
    ASSERT_EQ(second["items"].size(), 1u);
    EXPECT_EQ(second["items"][0]["name"], "WC-COMMONS-MAIN-002");

    EXPECT_EQ(content.Quests("ravenwood", 0, 0)["total"], 1);
    EXPECT_EQ(content.Quests("main-001", 0, 0)["total"], 1);
    EXPECT_EQ(content.Quests("nothing like it", 0, 0)["total"], 0);
}

TEST(StudioContentTest, AQuestCarriesItsGoalsDialogAndResults)
{
    std::optional<nlohmann::ordered_json> const quest = SampleContent().Quest("WC-COMMONS-MAIN-001");
    ASSERT_TRUE(quest);
    EXPECT_EQ((*quest)["title"], "Unicorn Way");
    ASSERT_EQ((*quest)["goals"].size(), 1u);
    EXPECT_EQ((*quest)["goals"][0]["name"], "TalkToLincoln");
    ASSERT_EQ((*quest)["dialogs"].size(), 1u);
    EXPECT_EQ((*quest)["dialogs"][0]["text"], "Welcome to Wizard City.");
    ASSERT_EQ((*quest)["results"].size(), 1u);
    EXPECT_EQ((*quest)["results"][0]["class"], "class ResTeleport");
    EXPECT_TRUE((*quest)["results"][0]["on_start"]);
    EXPECT_FALSE(SampleContent().Quest("missing"));
}

TEST(StudioContentTest, TheChannelAnswersEachPathAndRefusesTheRest)
{
    StudioContent const content = SampleContent();
    StudioChannel const channel(content);

    nlohmann::ordered_json const quest = nlohmann::ordered_json::parse(channel.Answer(R"({"id":7,"path":"/studio/quest","body":{"name":"WC-COMMONS-MAIN-002"}})"));
    EXPECT_EQ(quest["id"], 7);
    EXPECT_EQ(quest["status"], 200);
    EXPECT_EQ(quest["body"]["title"], "To Ravenwood!");

    nlohmann::ordered_json const missing = nlohmann::ordered_json::parse(channel.Answer(R"({"id":8,"path":"/studio/cinematic","body":{"name":"Cinematics/Nothing.xml"}})"));
    EXPECT_EQ(missing["status"], 404);
    EXPECT_FALSE(missing["ok"]);

    nlohmann::ordered_json const summary = nlohmann::ordered_json::parse(channel.Answer(R"({"id":9,"path":"/studio/summary"})"));
    EXPECT_EQ(summary["body"]["quests"], 2);
    EXPECT_FALSE(summary["body"]["install"]);

    EXPECT_EQ(nlohmann::ordered_json::parse(channel.Answer(R"({"path":"/studio/elsewhere"})"))["status"], 404);
    EXPECT_TRUE(channel.Answer("[1,2]").empty());
}

TEST(StudioContentTest, ACinematicsKeyIgnoresItsFolderCaseAndExtension)
{
    EXPECT_EQ(StudioContent::CinematicKey("Cinematics/Cinematic_UnicornActor.xml"), "cinematic_unicornactor");
    EXPECT_EQ(StudioContent::CinematicKey("Cinematic_UnicornActor"), "cinematic_unicornactor");
    EXPECT_EQ(StudioContent::CinematicKey("cinematics\\SATYRCINEMATIC.XML"), "satyrcinematic");
}
