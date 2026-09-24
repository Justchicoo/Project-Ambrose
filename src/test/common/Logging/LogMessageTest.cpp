/*
 * Project Ambrose by Imjustchico
 * Tests prefix layout, the short console timestamp, multi-line splitting, escaping and UTF-8 repair.
 */

#include "LogMessage.h"

#include <gtest/gtest.h>

namespace
{
    LogMessage MakeMessage(std::string text)
    {
        LogMessage message;
        message.Level = LogLevel::Info;
        message.Category = "server.test";
        message.Text = std::move(text);
        message.Time = std::chrono::system_clock::time_point(std::chrono::milliseconds(1700000000123));
        message.ThreadId = 42;
        return message;
    }

    std::string Render(LogMessage const& message, uint8 flags)
    {
        std::string out;
        message.AppendLines(out, static_cast<AppenderFlags>(flags), true);
        return out;
    }
}

TEST(LogMessageTest, PrefixSegmentsFollowFlagsInFixedOrder)
{
    LogMessage const message = MakeMessage("hello");
    EXPECT_EQ(Render(message, 0x00), "hello\n");
    EXPECT_EQ(Render(message, 0x01), "2023-11-14_22:13:20.123 hello\n");
    EXPECT_EQ(Render(message, 0x02), "INFO  hello\n");
    EXPECT_EQ(Render(message, 0x03), "2023-11-14_22:13:20.123 INFO  hello\n");
    EXPECT_EQ(Render(message, 0x04), "[server.test] hello\n");
    EXPECT_EQ(Render(message, 0x05), "2023-11-14_22:13:20.123 [server.test] hello\n");
    EXPECT_EQ(Render(message, 0x06), "INFO  [server.test] hello\n");
    EXPECT_EQ(Render(message, 0x07), "2023-11-14_22:13:20.123 INFO  [server.test] hello\n");
    EXPECT_EQ(Render(message, 0x27), "2023-11-14_22:13:20.123 INFO  T42 [server.test] hello\n");
    EXPECT_EQ(Render(message, 0x18), "hello\n");
}

TEST(LogMessageTest, TheShortTimestampKeepsTheTimeAndDropsTheDate)
{
    LogMessage const message = MakeMessage("hello");
    LogLayout layout;
    layout.Timestamp = LogTimestampStyle::Short;
    std::string out;
    message.AppendLines(out, AppenderFlags::PrefixTimestamp, true, nullptr, layout);
    EXPECT_EQ(out, "22:13:20.123 hello\n");

    layout.Timestamp = LogTimestampStyle::Off;
    out.clear();
    message.AppendLines(out, AppenderFlags::PrefixTimestamp, true, nullptr, layout);
    EXPECT_EQ(out, "hello\n");
}

TEST(LogMessageTest, EachLineOfMultiLineMessageGetsPrefix)
{
    EXPECT_EQ(Render(MakeMessage("first\nsecond\nthird"), 0x06), "INFO  [server.test] first\nINFO  [server.test] second\nINFO  [server.test] third\n");
    EXPECT_EQ(Render(MakeMessage("fake\n2023-11-14_22:13:20.123 FATAL [server.x] forged"), 0x02), "INFO  fake\nINFO  2023-11-14_22:13:20.123 FATAL [server.x] forged\n");
}

TEST(LogMessageTest, TrailingNewlineTrimmedCrLfNormalized)
{
    EXPECT_EQ(Render(MakeMessage("one\r\ntwo\r\n"), 0x02), "INFO  one\nINFO  two\n");
    EXPECT_EQ(Render(MakeMessage("line\n"), 0x00), "line\n");
    EXPECT_EQ(Render(MakeMessage("a\n\nb"), 0x02), "INFO  a\nINFO\nINFO  b\n");
}

TEST(LogMessageTest, ControlCharactersAreEscaped)
{
    EXPECT_EQ(Render(MakeMessage("a\x1b[31mred\tok\x7f"), 0x00), "a\\x1B[31mred\tok\\x7F\n");
    EXPECT_EQ(Render(MakeMessage(std::string("nul\0byte", 8)), 0x00), "nul\\x00byte\n");
    LogMessage message = MakeMessage("text");
    message.Category = "evil\x1b";
    EXPECT_EQ(Render(message, 0x04), "[evil\\x1B] text\n");
    EXPECT_EQ(Render(MakeMessage("csi\xC2\x9B" "2J done"), 0x00), "csi\\u009B2J done\n");
    EXPECT_EQ(Render(MakeMessage("nel\xC2\x85 and \xC2\xA0nbsp"), 0x00), "nel\\u0085 and \xC2\xA0nbsp\n");
}

TEST(LogMessageTest, InvalidUtf8IsRepaired)
{
    EXPECT_EQ(Render(MakeMessage("a\xFF" "b"), 0x00), "a\xEF\xBF\xBD" "b\n");
    EXPECT_EQ(Render(MakeMessage("caf\xC3\xA9"), 0x00), "caf\xC3\xA9\n");
    EXPECT_EQ(Render(MakeMessage("\xC3"), 0x00), "\xEF\xBF\xBD\n");
}

TEST(LogMessageTest, EmptyMessageWritesPrefixOnly)
{
    EXPECT_EQ(Render(MakeMessage(""), 0x06), "INFO  [server.test]\n");
    EXPECT_EQ(Render(MakeMessage(""), 0x00), "\n");
}

TEST(LogMessageTest, ThreadIdIsStablePerThread)
{
    uint64 const first = LogMessage::CurrentOsThreadId();
    EXPECT_EQ(first, LogMessage::CurrentOsThreadId());
    EXPECT_NE(first, 0u);
}
