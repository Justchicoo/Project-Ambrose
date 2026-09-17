/*
 * Project Ambrose by Imjustchico
 * Tests setup prompts with scripted answers: without a terminal nothing is printed or read; Enter picks the first option, a number picks its option, other text is a path, s skips, and an out-of-range number asks again; yes-or-no accepts Enter, y, yes, n and no in any case and gives up after three unclear answers; closed input and a timeout stop every later question.
 */

#include "ScriptedPromptInput.h"
#include "SetupPrompt.h"

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    struct Harness
    {
        std::shared_ptr<ScriptedPromptInput::Counters> Counters = std::make_shared<ScriptedPromptInput::Counters>();
        std::ostringstream Out;
        std::unique_ptr<SetupPrompt> Prompt;

        Harness(std::vector<std::string> lines, bool interactive = true, bool closeAtEnd = true, std::chrono::seconds timeout = std::chrono::seconds(30))
        {
            Prompt = std::make_unique<SetupPrompt>(std::make_unique<ScriptedPromptInput>(std::move(lines), closeAtEnd, Counters), Out, interactive, timeout);
        }
    };

    std::vector<std::string> const Options{ "C:/Games/One (r806919)", "D:/Games/Two (r900000)" };
}

TEST(SetupPromptTest, WithoutATerminalNothingIsAskedOrRead)
{
    Harness harness({ "1", "y" }, false);
    EXPECT_FALSE(harness.Prompt->IsInteractive());
    EXPECT_EQ(harness.Prompt->Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
    EXPECT_FALSE(harness.Prompt->Confirm("Extract?"));
    harness.Prompt->Say("hello");
    EXPECT_EQ(harness.Out.str(), "");
    EXPECT_EQ(harness.Counters->Reads.load(), 0);
    SetupPrompt const noInput(nullptr, harness.Out, true, std::chrono::seconds(1));
    EXPECT_FALSE(noInput.IsInteractive());
}

TEST(SetupPromptTest, ChoicesTakeEnterNumbersPathsAndSkips)
{
    Harness harness({ "", " 2 ", "9", "1", "E:/Mine", "S", "", "C:/Typed" });
    SetupPrompt::Choice first = harness.Prompt->Choose("Which install?", Options);
    EXPECT_EQ(first.Kind, SetupPrompt::Answer::Picked);
    EXPECT_EQ(first.Index, 0u);
    SetupPrompt::Choice second = harness.Prompt->Choose("Which install?", Options);
    EXPECT_EQ(second.Kind, SetupPrompt::Answer::Picked);
    EXPECT_EQ(second.Index, 1u);
    SetupPrompt::Choice retried = harness.Prompt->Choose("Which install?", Options);
    EXPECT_EQ(retried.Kind, SetupPrompt::Answer::Picked);
    EXPECT_EQ(retried.Index, 0u);
    SetupPrompt::Choice typed = harness.Prompt->Choose("Which install?", Options);
    EXPECT_EQ(typed.Kind, SetupPrompt::Answer::Path);
    EXPECT_EQ(typed.Path, "E:/Mine");
    EXPECT_EQ(harness.Prompt->Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
    EXPECT_EQ(harness.Prompt->Choose("Which dump?", {}).Kind, SetupPrompt::Answer::Skipped);
    SetupPrompt::Choice path = harness.Prompt->Choose("Which dump?", {});
    EXPECT_EQ(path.Kind, SetupPrompt::Answer::Path);
    EXPECT_EQ(path.Path, "C:/Typed");

    std::string const out = harness.Out.str();
    EXPECT_NE(out.find("Which install?\n  1) C:/Games/One (r806919)\n  2) D:/Games/Two (r900000)\n"), std::string::npos);
    EXPECT_NE(out.find("Choose a number from 1 to 2."), std::string::npos);
    EXPECT_NE(out.find("Type a path, or press Enter to skip: "), std::string::npos);
    EXPECT_TRUE(harness.Prompt->IsInteractive());
}

TEST(SetupPromptTest, ConfirmationsAcceptYesAndNoAndGiveUpWhenUnclear)
{
    Harness harness({ "", "YES", "maybe", "y", "No", "what", "huh", "eh" });
    EXPECT_TRUE(harness.Prompt->Confirm("Extract?"));
    EXPECT_TRUE(harness.Prompt->Confirm("Extract?"));
    EXPECT_TRUE(harness.Prompt->Confirm("Extract?"));
    EXPECT_FALSE(harness.Prompt->Confirm("Extract?"));
    EXPECT_FALSE(harness.Prompt->Confirm("Extract?"));
    EXPECT_EQ(harness.Counters->Reads.load(), 8);
    EXPECT_NE(harness.Out.str().find("Extract? [Y/n]: "), std::string::npos);
    EXPECT_NE(harness.Out.str().find("Answer y or n."), std::string::npos);
}

TEST(SetupPromptTest, ClosedInputAndTimeoutsStopEveryLaterQuestion)
{
    Harness closed({});
    EXPECT_EQ(closed.Prompt->Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
    EXPECT_FALSE(closed.Prompt->IsInteractive());
    EXPECT_FALSE(closed.Prompt->Confirm("Extract?"));
    EXPECT_NE(closed.Out.str().find("Input closed; skipping setup questions."), std::string::npos);

    Harness waiting({}, true, false, std::chrono::seconds(1));
    EXPECT_FALSE(waiting.Prompt->Confirm("Extract?"));
    EXPECT_EQ(waiting.Counters->Interrupts.load(), 1);
    EXPECT_FALSE(waiting.Prompt->IsInteractive());
    EXPECT_EQ(waiting.Prompt->Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
    EXPECT_NE(waiting.Out.str().find("No answer in time; skipping setup questions."), std::string::npos);
}
