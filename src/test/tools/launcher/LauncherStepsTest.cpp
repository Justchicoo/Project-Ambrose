/*
 * Project Ambrose by Imjustchico
 * Checks the first run the launcher's window shows: each ask moves at most one step on, the install and the run folder report their own numbers, the server is asked no more than once a second and the steps end with it open, a step that fails names the cause and what to do and stops the steps after it, the server is given up on after thirty tries, asking again starts from the top, and what the window is sent carries each step's id, label, state and word.
 */

#include "ConfigMgr.h"
#include "LauncherHarness.h"
#include "LauncherSteps.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace
{
    struct Stepping
    {
        LauncherHarness Harness;
        std::chrono::steady_clock::time_point Now{ std::chrono::seconds(100) };
        bool Open = false;
        int Asked = 0;
        LauncherSteps Steps{ [this](std::string const& host, uint16 port, std::string& error)
            {
                ++Asked;
                EXPECT_EQ(host, "127.0.0.1");
                EXPECT_EQ(port, 12000);
                if (!Open)
                    error = "connection refused";
                return Open;
            },
            [this] { return Now; } };

        std::vector<LauncherStep> const& Advance()
        {
            SetupPrompt prompt(std::make_unique<ScriptedPromptInput>(std::vector<std::string>{}, true, Harness.Counters), Harness.Out, false, std::chrono::seconds(1));
            Launcher const launcher(Harness.System, Harness.Files, Harness.Err);
            return Steps.Advance(launcher, LauncherRequest{}, SetupMode::Auto, prompt);
        }

        std::string States()
        {
            std::string states;
            for (LauncherStep const& step : Steps.Steps())
                states += (states.empty() ? "" : ",") + step.State;
            return states;
        }
    };
}

TEST(LauncherStepsTest, EachAskMovesOneStepOnAndTheStepsEndWithTheServerOpen)
{
    Stepping stepping;
    stepping.Harness.AddInstall();
    EXPECT_EQ(stepping.States(), "waiting,waiting,waiting");

    stepping.Advance();
    EXPECT_EQ(stepping.States(), "done,doing,waiting");
    EXPECT_NE(stepping.Steps.Steps()[0].Word.find(LauncherTestData::Revision), std::string::npos) << stepping.Steps.Steps()[0].Word;
    EXPECT_NE(stepping.Steps.Steps()[0].Word.find(ConfigMgr::PathToUtf8(ConfigMgr::PathFromUtf8(LauncherTestData::Install).lexically_normal())), std::string::npos);

    stepping.Advance();
    EXPECT_EQ(stepping.States(), "done,done,doing");
    EXPECT_NE(stepping.Steps.Steps()[1].Word.find(ConfigMgr::PathToUtf8(ConfigMgr::PathFromUtf8(LauncherTestData::RunFolder).lexically_normal())), std::string::npos) << stepping.Steps.Steps()[1].Word;
    EXPECT_NE(stepping.Steps.Steps()[1].Word.find(" files"), std::string::npos);
    EXPECT_FALSE(stepping.Harness.Files.Written.empty()) << "the run folder is written as Play would write it";

    stepping.Advance();
    EXPECT_EQ(stepping.Asked, 1);
    EXPECT_EQ(stepping.States(), "done,done,doing");
    EXPECT_NE(stepping.Steps.Steps()[2].Word.find("try 1 of 30"), std::string::npos) << stepping.Steps.Steps()[2].Word;

    stepping.Advance();
    EXPECT_EQ(stepping.Asked, 1) << "the server is asked no more than once a second";
    stepping.Now += std::chrono::seconds(1);
    stepping.Open = true;
    stepping.Advance();
    EXPECT_EQ(stepping.Asked, 2);
    EXPECT_EQ(stepping.States(), "done,done,done");
    EXPECT_NE(stepping.Steps.Steps()[2].Word.find("127.0.0.1:12000 is open"), std::string::npos) << stepping.Steps.Steps()[2].Word;

    stepping.Advance();
    EXPECT_EQ(stepping.Asked, 2) << "a finished run asks nothing more";
}

TEST(LauncherStepsTest, AStepThatFailsNamesTheCauseAndWhatToDoAndStopsTheRest)
{
    Stepping stepping;
    stepping.Advance();
    EXPECT_EQ(stepping.States(), "wrong,waiting,waiting");
    std::string const& word = stepping.Steps.Steps()[0].Word;
    EXPECT_NE(word.find("ClientDir"), std::string::npos) << word;
    EXPECT_NE(word.find("--client"), std::string::npos) << word;
    stepping.Advance();
    EXPECT_EQ(stepping.States(), "wrong,waiting,waiting");
    EXPECT_EQ(stepping.Asked, 0);
}

TEST(LauncherStepsTest, TheServerIsGivenUpOnAfterThirtyTriesAndAskingAgainStartsOver)
{
    Stepping stepping;
    stepping.Harness.AddInstall();
    stepping.Advance();
    stepping.Advance();
    for (int attempt = 0; attempt < LauncherSteps::ServerTries; ++attempt)
    {
        stepping.Now += std::chrono::seconds(1);
        stepping.Advance();
    }
    EXPECT_EQ(stepping.Asked, LauncherSteps::ServerTries);
    EXPECT_EQ(stepping.States(), "done,done,wrong");
    std::string const& word = stepping.Steps.Steps()[2].Word;
    EXPECT_NE(word.find("after 30 tries"), std::string::npos) << word;
    EXPECT_NE(word.find("connection refused"), std::string::npos) << word;
    EXPECT_NE(word.find("Start the Ambrose servers"), std::string::npos) << word;

    stepping.Steps.Restart();
    EXPECT_EQ(stepping.States(), "waiting,waiting,waiting");
    stepping.Open = true;
    stepping.Advance();
    stepping.Advance();
    stepping.Advance();
    EXPECT_EQ(stepping.States(), "done,done,done");
}

TEST(LauncherStepsTest, TheWindowIsSentEachStepsIdLabelStateAndWord)
{
    Stepping stepping;
    stepping.Harness.AddInstall();
    stepping.Advance();
    nlohmann::json const sent = nlohmann::json::parse(LauncherSteps::Describe(stepping.Steps.Steps()));
    ASSERT_TRUE(sent["steps"].is_array());
    ASSERT_EQ(sent["steps"].size(), 3u);
    EXPECT_EQ(sent["steps"][0]["id"], "install");
    EXPECT_EQ(sent["steps"][0]["state"], "done");
    EXPECT_EQ(sent["steps"][1]["id"], "run-folder");
    EXPECT_EQ(sent["steps"][1]["state"], "doing");
    EXPECT_EQ(sent["steps"][2]["label"], "Reach the login server");
    EXPECT_TRUE(sent["steps"][2]["word"].is_string());
}
