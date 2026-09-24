/*
 * Project Ambrose by Imjustchico
 * Tests what an app remembers about the errors it raised: one line raised many times is one group with a count rather than many records, two lines that read alike from different places stay apart, the same line under a different template is a different group, a group keeps when it was first and last seen and the last message it rendered, the store is bounded with the group seen longest ago dropped first and the drop counted, and nothing below error level is remembered at all.
 */

#include "Log.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    LogMessage Raised(LogLevel level, std::string category, std::string text, std::string_view file, uint32 line, std::string tpl)
    {
        LogMessage message;
        message.Level = level;
        message.Category = std::move(category);
        message.Text = std::move(text);
        message.Time = std::chrono::system_clock::now();
        message.Template = std::move(tpl);
        message.Source = LogSource{ file, "Raise", line };
        return message;
    }

    LogErrorGroup const* Find(std::vector<LogErrorGroup> const& groups, uint32 line)
    {
        for (LogErrorGroup const& group : groups)
            if (group.Line == line)
                return &group;
        return nullptr;
    }
}

TEST(LogErrorStoreTest, OneLineRaisedManyTimesIsOneGroupWithACount)
{
    LogErrorStore store;
    for (int repeat = 0; repeat < 5; ++repeat)
        store.Note(Raised(LogLevel::Error, "server.a", "it failed for the " + std::to_string(repeat) + "th time", "src/A.cpp", 10, "it failed for the {}th time"));

    std::vector<LogErrorGroup> const groups = store.Groups();
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].Count, 5u);
    EXPECT_EQ(groups[0].Line, 10u);
    EXPECT_EQ(groups[0].File, "src/A.cpp");
    EXPECT_EQ(groups[0].Function, "Raise");
    EXPECT_EQ(groups[0].LastMessage, "it failed for the 4th time");
    EXPECT_LE(groups[0].FirstSeen, groups[0].LastSeen);
    EXPECT_FALSE(groups[0].Revision.empty());
}

TEST(LogErrorStoreTest, TheSameWordsFromDifferentPlacesAreDifferentGroups)
{
    LogErrorStore store;
    store.Note(Raised(LogLevel::Error, "server.a", "could not open it", "src/A.cpp", 10, "could not open it"));
    store.Note(Raised(LogLevel::Error, "server.a", "could not open it", "src/B.cpp", 10, "could not open it"));
    store.Note(Raised(LogLevel::Error, "server.a", "could not open it", "src/A.cpp", 11, "could not open it"));
    store.Note(Raised(LogLevel::Error, "server.b", "could not open it", "src/A.cpp", 10, "could not open it"));
    store.Note(Raised(LogLevel::Error, "server.a", "could not open it", "src/A.cpp", 10, "could not open {}"));

    EXPECT_EQ(store.Size(), 5u);
}

TEST(LogErrorStoreTest, TheGroupSeenLongestAgoIsDroppedFirstAndTheDropIsCounted)
{
    LogErrorStore store;
    store.SetCapacity(3);
    for (uint32 line = 1; line <= 3; ++line)
        store.Note(Raised(LogLevel::Error, "server.a", "failed", "src/A.cpp", line, "failed"));
    store.Note(Raised(LogLevel::Error, "server.a", "failed again", "src/A.cpp", 1, "failed"));
    store.Note(Raised(LogLevel::Error, "server.a", "failed", "src/A.cpp", 4, "failed"));

    std::vector<LogErrorGroup> const groups = store.Groups();
    EXPECT_EQ(groups.size(), 3u);
    EXPECT_EQ(store.GetDropped(), 1u);
    EXPECT_NE(Find(groups, 1), nullptr) << "line 1 was seen again, so it is not the oldest";
    EXPECT_EQ(Find(groups, 2), nullptr) << "line 2 was seen longest ago";
    EXPECT_NE(Find(groups, 4), nullptr);
}

TEST(LogErrorStoreTest, NothingBelowErrorIsRememberedAndACapacityOfNoneRemembersNothing)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Warn, "server.test", "a warning is not an error");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server.test", "nor is a note");
    EXPECT_EQ(harness.GetLog().GetErrors().Size(), 0u);

    AMBROSE_LOG(harness.GetLog(), LogLevel::Error, "server.test", "this one is");
    ASSERT_EQ(harness.GetLog().GetErrors().Size(), 1u);
    std::vector<LogErrorGroup> const groups = harness.GetLog().GetErrors().Groups();
    EXPECT_EQ(groups[0].Category, "server.test");
    EXPECT_EQ(groups[0].Template, "this one is");
    EXPECT_NE(groups[0].File.find("LogErrorsTest.cpp"), std::string::npos);

    LogErrorStore none;
    none.SetCapacity(0);
    none.Note(Raised(LogLevel::Fatal, "server.a", "failed", "src/A.cpp", 1, "failed"));
    EXPECT_EQ(none.Size(), 0u);
}
