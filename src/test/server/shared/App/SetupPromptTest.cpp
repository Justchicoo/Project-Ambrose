/*
 * Project Ambrose by Imjustchico
 * Tests setup prompts with scripted answers: without a terminal nothing is printed or read; Enter picks the first option, a number picks its option, other text is a path, s skips, and an out-of-range number asks again; yes-or-no accepts Enter, y, yes, n and no in any case and gives up after three unclear answers; closed input, a timeout and a stop request stop every later question and say why, a stop ends a waiting question with or without a timeout, and a line that arrives as the timeout fires still counts as a timeout.
 */

#include "ScriptedPromptInput.h"
#include "SetupPrompt.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
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

    class LateLineInput final : public ConsoleInput
    {
    public:
        ReadResult ReadLine(std::string& line, std::chrono::milliseconds timeout) override
        {
            std::unique_lock lock(_mutex);
            if (!_wake.wait_for(lock, timeout, [this] { return _interrupted; }))
                return ReadResult::Timeout;
            line = "1";
            return ReadResult::Line;
        }

        void Interrupt() override
        {
            std::lock_guard const lock(_mutex);
            _interrupted = true;
            _wake.notify_all();
        }

    private:
        std::mutex _mutex;
        std::condition_variable _wake;
        bool _interrupted = false;
    };
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
    EXPECT_EQ(harness.Prompt->GetStatus(), SetupPrompt::Status::NotATerminal);
    SetupPrompt const noInput(nullptr, harness.Out, true, std::chrono::seconds(1));
    EXPECT_FALSE(noInput.IsInteractive());
    EXPECT_EQ(noInput.GetStatus(), SetupPrompt::Status::NotATerminal);
    std::unique_ptr<SetupPrompt> const disabled = SetupPrompt::ForProcess(harness.Out, false, std::chrono::seconds(1));
    EXPECT_FALSE(disabled->IsInteractive());
    EXPECT_EQ(disabled->GetStatus(), SetupPrompt::Status::Disabled);
    EXPECT_EQ(harness.Out.str(), "");
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
    EXPECT_EQ(closed.Prompt->GetStatus(), SetupPrompt::Status::InputClosed);

    Harness waiting({}, true, false, std::chrono::seconds(1));
    EXPECT_FALSE(waiting.Prompt->Confirm("Extract?"));
    EXPECT_EQ(waiting.Counters->Interrupts.load(), 1);
    EXPECT_FALSE(waiting.Prompt->IsInteractive());
    EXPECT_EQ(waiting.Prompt->Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
    EXPECT_NE(waiting.Out.str().find("No answer in time; skipping setup questions."), std::string::npos);
    EXPECT_EQ(waiting.Prompt->GetStatus(), SetupPrompt::Status::TimedOut);
}

TEST(SetupPromptTest, AStopRequestEndsAWaitingQuestionWithAndWithoutATimeout)
{
    for (std::chrono::seconds const timeout : { std::chrono::seconds(0), std::chrono::seconds(60) })
    {
        Harness harness({}, true, false, timeout);
        std::atomic<bool> stop{ false };
        std::atomic<int> polls{ 0 };
        harness.Prompt->SetCancellation([&stop, &polls]
        {
            ++polls;
            return stop.load();
        });
        std::thread stopper([&stop]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            stop = true;
        });
        auto const askedAt = std::chrono::steady_clock::now();
        EXPECT_FALSE(harness.Prompt->Confirm("Extract?"));
        stopper.join();
        EXPECT_LT(std::chrono::steady_clock::now() - askedAt, std::chrono::seconds(10)) << timeout.count();
        EXPECT_GE(polls.load(), 2) << timeout.count();
        EXPECT_EQ(harness.Counters->Interrupts.load(), 1) << timeout.count();
        EXPECT_FALSE(harness.Prompt->IsInteractive());
        EXPECT_EQ(harness.Prompt->GetStatus(), SetupPrompt::Status::Stopped);
        EXPECT_NE(harness.Out.str().find("Extract? [Y/n]: \nStop requested; skipping setup questions.\n"), std::string::npos) << harness.Out.str();
        EXPECT_EQ(harness.Prompt->Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
        EXPECT_EQ(harness.Out.str().find("Which install?"), std::string::npos);
    }

    Harness early({ "1" });
    early.Prompt->SetCancellation([] { return true; });
    EXPECT_EQ(early.Prompt->Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
    EXPECT_EQ(early.Counters->Reads.load(), 0);
    EXPECT_EQ(early.Out.str(), "\nStop requested; skipping setup questions.\n");
    EXPECT_EQ(early.Prompt->GetStatus(), SetupPrompt::Status::Stopped);

    Harness answered({ "2" });
    answered.Prompt->SetCancellation([] { return false; });
    SetupPrompt::Choice const choice = answered.Prompt->Choose("Which install?", Options);
    EXPECT_EQ(choice.Kind, SetupPrompt::Answer::Picked);
    EXPECT_EQ(choice.Index, 1u);
    EXPECT_TRUE(answered.Prompt->IsInteractive());
}

TEST(SetupPromptTest, ALineArrivingAsTheTimeoutFiresStillCountsAsATimeout)
{
    std::ostringstream out;
    SetupPrompt prompt(std::make_unique<LateLineInput>(), out, true, std::chrono::seconds(1));
    EXPECT_EQ(prompt.Choose("Which install?", Options).Kind, SetupPrompt::Answer::Skipped);
    EXPECT_FALSE(prompt.IsInteractive());
    EXPECT_EQ(prompt.GetStatus(), SetupPrompt::Status::TimedOut);
    EXPECT_FALSE(prompt.Confirm("Extract?"));
    std::string const text = out.str();
    EXPECT_NE(text.find("No answer in time; skipping setup questions."), std::string::npos) << text;
    EXPECT_EQ(text.find("Input closed"), std::string::npos) << text;
    EXPECT_EQ(text.find("Extract?"), std::string::npos) << text;
}
