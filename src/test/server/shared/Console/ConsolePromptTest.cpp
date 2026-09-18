/*
 * Project Ambrose by Imjustchico
 * Tests the drawn prompt on a fake console: what it writes as keys arrive, how a log line erases and redraws the typed text, the line left behind when one is entered, the sideways scroll that keeps a long line on one row, the prompt going away for good when input closes, the colors it paints, and that a stream which is not a terminal gets no prompt at all.
 */

#include "ConsolePrompt.h"
#include "FakeConsoleDevice.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    ConsoleKey Press(ConsoleKeyKind kind)
    {
        ConsoleKey key;
        key.Kind = kind;
        return key;
    }

    ConsoleKey Letter(char c)
    {
        ConsoleKey key;
        key.Kind = ConsoleKeyKind::Character;
        key.Text.assign(1, c);
        return key;
    }

    class PromptFixture
    {
    public:
        PromptFixture(bool terminal, bool virtualTerminal, ConsoleColorMode colors)
        {
            auto device = std::make_unique<FakeConsoleDevice>(terminal, virtualTerminal);
            _device = device.get();
            _writer = std::make_unique<ConsoleWriter>(std::move(device));
            _writer->SetColorMode(colors);
            _prompt = std::make_unique<ConsolePrompt>(*_writer, "Ambrose> ");
        }

        ~PromptFixture()
        {
            _prompt.reset();
        }

        FakeConsoleDevice& Device() { return *_device; }
        ConsoleWriter& Writer() { return *_writer; }
        ConsolePrompt& Prompt() { return *_prompt; }

        std::string Taken()
        {
            std::string const output = _device->Output();
            std::string const fresh = output.substr(_read);
            _read = output.size();
            return fresh;
        }

        ConsolePrompt::Result Type(char c)
        {
            std::string line;
            return _prompt->Apply(Letter(c), line);
        }

        void TypeAll(std::string_view text)
        {
            for (char const c : text)
                Type(c);
        }

    private:
        FakeConsoleDevice* _device = nullptr;
        std::unique_ptr<ConsoleWriter> _writer;
        std::unique_ptr<ConsolePrompt> _prompt;
        std::size_t _read = 0;
    };
}

TEST(ConsolePromptTest, AttachDrawsThePromptAndDetachTakesItBack)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Prompt().Attach();
    EXPECT_EQ(fixture.Taken(), "Ambrose> ");
    fixture.Prompt().Detach();
    EXPECT_EQ(fixture.Taken(), "\r\x1b[K");
}

TEST(ConsolePromptTest, EachKeyRedrawsTheLineAndPlacesTheCursor)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Prompt().Attach();
    fixture.Taken();
    EXPECT_EQ(fixture.Type('a'), ConsolePrompt::Result::Pending);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> a");
    fixture.Type('b');
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> ab");
    std::string line;
    fixture.Prompt().Apply(Press(ConsoleKeyKind::Left), line);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> ab\b");
}

TEST(ConsolePromptTest, ALogLineErasesAndRedrawsTheHalfTypedCommand)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Prompt().Attach();
    fixture.Type('s');
    fixture.Type('t');
    fixture.Taken();
    fixture.Writer().WriteLines("INFO  a client connected\n", ConsoleColor::Default);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KINFO  a client connected\nAmbrose> st");
}

TEST(ConsolePromptTest, EnterLeavesTheLineBehindAndStartsAFreshPrompt)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Prompt().Attach();
    fixture.Type('h');
    fixture.Type('i');
    fixture.Taken();
    std::string line;
    EXPECT_EQ(fixture.Prompt().Apply(Press(ConsoleKeyKind::Enter), line), ConsolePrompt::Result::Line);
    EXPECT_EQ(line, "hi");
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> hi\nAmbrose> ");
    EXPECT_EQ(fixture.Prompt().Apply(Press(ConsoleKeyKind::EndOfFile), line), ConsolePrompt::Result::Closed);
}

TEST(ConsolePromptTest, InterruptAbandonsTheLine)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Prompt().Attach();
    fixture.Type('o');
    fixture.Taken();
    std::string line;
    EXPECT_EQ(fixture.Prompt().Apply(Press(ConsoleKeyKind::Interrupt), line), ConsolePrompt::Result::Pending);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> o^C\nAmbrose> ");
}

TEST(ConsolePromptTest, CompletionsPrintAboveThePrompt)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Prompt().Editor().SetCompleter([](std::string_view) { return std::vector<std::string>{ "shutdown", "status" }; });
    fixture.Prompt().Attach();
    fixture.Type('s');
    fixture.Taken();
    std::string line;
    fixture.Prompt().Apply(Press(ConsoleKeyKind::Tab), line);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[Kshutdown\nstatus\nAmbrose> s");
}

TEST(ConsolePromptTest, ColorsPaintThePromptAndLegacyConsolesUseAttributes)
{
    PromptFixture ansi(true, true, ConsoleColorMode::Always);
    ansi.Prompt().Attach();
    EXPECT_EQ(ansi.Taken(), "\x1b[33mAmbrose> \x1b[0m");

    PromptFixture legacy(true, false, ConsoleColorMode::Always);
    legacy.Prompt().Attach();
    EXPECT_EQ(legacy.Taken(), "Ambrose> ");
    EXPECT_EQ(legacy.Device().LegacyColors(), (std::vector<ConsoleColor>{ ConsoleColor::Brown }));
    legacy.Type('a');
    EXPECT_EQ(legacy.Taken(), "\r         \rAmbrose> a");
}

TEST(ConsolePromptTest, AStreamThatIsNotATerminalGetsNoPrompt)
{
    PromptFixture fixture(false, false, ConsoleColorMode::Never);
    fixture.Prompt().Attach();
    EXPECT_EQ(fixture.Taken(), "");
    fixture.Type('a');
    fixture.Writer().WriteLines("INFO  plain\n", ConsoleColor::Default);
    EXPECT_EQ(fixture.Taken(), "INFO  plain\n");
    fixture.Prompt().Detach();
    EXPECT_EQ(fixture.Taken(), "");
}

TEST(ConsolePromptTest, ALineWiderThanTheWindowScrollsSidewaysInsteadOfWrapping)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Device().SetColumns(20);
    fixture.Prompt().Attach();
    fixture.TypeAll("012345678");
    fixture.Taken();
    fixture.Type('9');
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> 0123456789");
    fixture.Type('a');
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> 123456789a");
    fixture.Writer().WriteLines("INFO  a client connected\n", ConsoleColor::Default);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KINFO  a client connected\nAmbrose> 123456789a");
    std::string line;
    fixture.Prompt().Apply(Press(ConsoleKeyKind::Left), line);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> 0123456789");
}

TEST(ConsolePromptTest, ASubmittedLineKeepsEveryCharacterInTheScrollback)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Device().SetColumns(20);
    fixture.Prompt().Attach();
    fixture.TypeAll("shutdown 3600 now");
    fixture.Taken();
    std::string line;
    EXPECT_EQ(fixture.Prompt().Apply(Press(ConsoleKeyKind::Enter), line), ConsolePrompt::Result::Line);
    EXPECT_EQ(line, "shutdown 3600 now");
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> shutdown 3600 now\nAmbrose> ");
}

TEST(ConsolePromptTest, ClosingTheInputTakesThePromptBackForGood)
{
    PromptFixture fixture(true, true, ConsoleColorMode::Never);
    fixture.Prompt().Attach();
    fixture.Taken();
    std::string line;
    EXPECT_EQ(fixture.Prompt().Apply(Press(ConsoleKeyKind::EndOfFile), line), ConsolePrompt::Result::Closed);
    EXPECT_EQ(fixture.Taken(), "\r\x1b[KAmbrose> \n");
    fixture.Writer().WriteLines("INFO  the server keeps running\n", ConsoleColor::Default);
    EXPECT_EQ(fixture.Taken(), "INFO  the server keeps running\n");
}
