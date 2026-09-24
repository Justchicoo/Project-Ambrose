/*
 * Project Ambrose by Imjustchico
 * Checks what the terminal dashboard promises: it draws exactly the size it was given so a terminal that changed shape is laid out again rather than redrawn at the old shape, every panel an operator needs is in it, the newest log lines are the ones kept when the window is short because the last thing that happened is what they are watching for, a window too small to read is told so rather than filled with squeezed panels, the keys that leave are judged the same whether they came from a terminal or from here, the whole loop runs against a terminal a test supplies so that a size which changes between key presses proves the panels are laid out again and the key that leaves proves what the loop returns, and asking for the dashboard where there is no terminal is declined with a warning that says what runs instead.
 */

#include "TerminalDashboard.h"

#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <vector>

namespace
{
    std::vector<std::string> Rows(std::string const& drawn)
    {
        std::vector<std::string> rows;
        std::size_t begin = 0;
        while (begin <= drawn.size())
        {
            std::size_t const end = drawn.find('\n', begin);
            if (end == std::string::npos)
            {
                if (begin < drawn.size())
                    rows.push_back(drawn.substr(begin));
                break;
            }
            std::string row = drawn.substr(begin, end - begin);
            if (!row.empty() && row.back() == '\r')
                row.pop_back();
            rows.push_back(std::move(row));
            begin = end + 1;
        }
        return rows;
    }

    std::string Plain(std::string const& row)
    {
        std::string plain;
        for (std::size_t at = 0; at < row.size();)
        {
            if (row[at] == '\x1b' && at + 1 < row.size() && row[at + 1] == '[')
            {
                at += 2;
                while (at < row.size() && row[at] != 'm')
                    ++at;
                if (at < row.size())
                    ++at;
                continue;
            }
            plain.push_back(row[at]);
            ++at;
        }
        return plain;
    }

    std::size_t Widest(std::vector<std::string> const& rows)
    {
        std::size_t widest = 0;
        for (std::string const& row : rows)
        {
            std::string const plain = Plain(row);
            std::size_t letters = 0;
            for (std::size_t at = 0; at < plain.size();)
            {
                unsigned char const lead = static_cast<unsigned char>(plain[at]);
                at += lead < 0x80 ? 1 : (lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4));
                ++letters;
            }
            widest = std::max(widest, letters);
        }
        return widest;
    }

    TerminalPanels Panels()
    {
        TerminalPanels panels;
        panels.App = "gameserver";
        panels.Role = "game";
        panels.Revision = "b04e0ee6";
        panels.State = "running";
        panels.Uptime = "3h 12m";
        panels.Figures = { { "sessions", "4" }, { "players", "3" } };
        panels.Problems = {};
        panels.Logs = { "first line", "second line", "third line" };
        panels.Typed = "server info";
        return panels;
    }
}

TEST(TerminalDashboardTest, ItDrawsTheSizeItIsGivenSoAResizeLaysItOutAgain)
{
    TerminalPanels const panels = Panels();

    std::string const small = TerminalDashboard::Draw(panels, 80, 24);
    std::vector<std::string> const smallRows = Rows(small);
    EXPECT_EQ(smallRows.size(), 24u) << "the drawing fills the height it was told, no more and no less";
    EXPECT_EQ(Widest(smallRows), 80u) << "and the width";

    std::string const wide = TerminalDashboard::Draw(panels, 140, 40);
    std::vector<std::string> const wideRows = Rows(wide);
    EXPECT_EQ(wideRows.size(), 40u) << "a terminal that grew is laid out again at its new size";
    EXPECT_EQ(Widest(wideRows), 140u);

    EXPECT_NE(small, wide) << "the same panels at two sizes are two drawings, which is what laying out again means";
}

TEST(TerminalDashboardTest, EveryPanelAnOperatorNeedsIsDrawn)
{
    std::string const drawn = TerminalDashboard::Draw(Panels(), 100, 30);
    EXPECT_NE(drawn.find("Status"), std::string::npos);
    EXPECT_NE(drawn.find("Sessions"), std::string::npos);
    EXPECT_NE(drawn.find("Log"), std::string::npos);
    EXPECT_NE(drawn.find("Command"), std::string::npos);

    EXPECT_NE(drawn.find("gameserver"), std::string::npos);
    EXPECT_NE(drawn.find("running"), std::string::npos);
    EXPECT_NE(drawn.find("3h 12m"), std::string::npos);
    EXPECT_NE(drawn.find("sessions"), std::string::npos);
    EXPECT_NE(drawn.find("Ambrose>"), std::string::npos) << "the command line is the point of being here rather than reading a log file";
    EXPECT_NE(drawn.find("server info"), std::string::npos) << "and what has been typed so far is shown";
}

TEST(TerminalDashboardTest, AShortWindowKeepsTheNewestLinesRatherThanTheOldest)
{
    TerminalPanels panels = Panels();
    panels.Logs.clear();
    for (int at = 1; at <= 60; ++at)
        panels.Logs.push_back("line " + std::to_string(at));

    std::string const drawn = TerminalDashboard::Draw(panels, 100, 20);
    EXPECT_NE(drawn.find("line 60"), std::string::npos) << "the last thing that happened is what an operator is watching for";
    EXPECT_EQ(drawn.find("line 1 "), std::string::npos) << "and the oldest lines are the ones a short window drops";
}

TEST(TerminalDashboardTest, AWindowTooSmallToReadSaysSoRatherThanSqueezingThePanels)
{
    std::string const drawn = TerminalDashboard::Draw(Panels(), 20, 5);
    EXPECT_NE(drawn.find("too small"), std::string::npos);
    EXPECT_EQ(drawn.find("Ambrose>"), std::string::npos) << "a command line nobody can read is worse than a sentence saying why";
    EXPECT_EQ(Rows(drawn).size(), 5u) << "and it still draws the size it was given";
}

TEST(TerminalDashboardTest, TheKeysThatLeaveAreJudgedTheSameWhereverTheyCameFrom)
{
    EXPECT_EQ(TerminalDashboard::Judge("q"), TerminalVerdict::Quit);
    EXPECT_EQ(TerminalDashboard::Judge("Q"), TerminalVerdict::Quit);
    EXPECT_EQ(TerminalDashboard::Judge("\x03"), TerminalVerdict::Quit) << "Ctrl+C leaves, and leaves the same way q does";

    EXPECT_EQ(TerminalDashboard::Judge("\r"), TerminalVerdict::Send);
    EXPECT_EQ(TerminalDashboard::Judge("\n"), TerminalVerdict::Send);
    EXPECT_EQ(TerminalDashboard::Judge("\x7f"), TerminalVerdict::Rubbed);
    EXPECT_EQ(TerminalDashboard::Judge("a"), TerminalVerdict::Typed);
    EXPECT_EQ(TerminalDashboard::Judge(" "), TerminalVerdict::Typed);
    EXPECT_EQ(TerminalDashboard::Judge("\x1b[A"), TerminalVerdict::Ignore) << "an arrow key is not a letter and must not be typed into the line";
}

TEST(TerminalDashboardTest, ATerminalThatChangedShapeIsLaidOutAgainAndQLeavesWithNothingWrong)
{
    std::vector<std::string> painted;
    std::vector<std::string> ran;
    std::vector<std::string> keys{ "s", "t", "a", "t", "u", "s", "\r", "q" };
    std::size_t next = 0;
    int width = 80;
    int height = 24;

    TerminalSources sources;
    sources.Panels = [] { return Panels(); };
    sources.Size = [&width, &height] { return std::pair<int, int>(width, height); };
    sources.Show = [&painted](std::string const& drawn) { painted.push_back(drawn); };
    sources.Send = [&ran](std::string const& command) { ran.push_back(command); };
    sources.NextKey = [&keys, &next, &width, &height]() -> std::optional<std::string> {
        if (next == 3)
        {
            width = 132;
            height = 43;
        }
        if (next >= keys.size())
            return std::nullopt;
        return keys[next++];
    };

    int const code = TerminalDashboard::Loop(sources);

    EXPECT_EQ(code, 0) << "q leaves the dashboard with nothing wrong, which is what an operator over SSH needs it to do";
    ASSERT_GE(painted.size(), keys.size());
    EXPECT_EQ(ran, std::vector<std::string>{ "status" }) << "what was typed before the return key is what was run";

    EXPECT_EQ(Rows(painted.front()).size(), 24u) << "the first painting is the size the terminal was";
    EXPECT_EQ(Widest(Rows(painted.front())), 80u);
    EXPECT_EQ(Rows(painted.back()).size(), 43u) << "and after the terminal changed shape the panels are laid out again at the new one";
    EXPECT_EQ(Widest(Rows(painted.back())), 132u);
}

TEST(TerminalDashboardTest, AKeyReaderThatEndsLeavesTheSameWayQDoes)
{
    TerminalSources sources;
    sources.Panels = [] { return Panels(); };
    sources.Size = [] { return std::pair<int, int>(80, 24); };
    sources.Show = [](std::string const&) {};
    sources.NextKey = []() -> std::optional<std::string> { return std::nullopt; };

    EXPECT_EQ(TerminalDashboard::Loop(sources), 0) << "a terminal that closed under the operator is not an error either";
}

TEST(TerminalDashboardTest, ALoopWithNowhereToDrawRefusesRatherThanRunningBlind)
{
    TerminalSources sources;
    sources.Panels = [] { return Panels(); };
    sources.Size = [] { return std::pair<int, int>(80, 24); };
    EXPECT_EQ(TerminalDashboard::Loop(sources), 1);
}

TEST(TerminalDashboardTest, WithoutATerminalTheDashboardIsDeclinedAndSaysWhy)
{
    std::string warning;

    EXPECT_TRUE(TerminalDashboard::Offer(true, true, warning)) << "asked for, and there is a terminal to draw in";
    EXPECT_TRUE(warning.empty()) << warning;

    EXPECT_FALSE(TerminalDashboard::Offer(true, false, warning))
        << "asked for, but the output is a file or a pipe, so the ordinary console is what runs";
    EXPECT_NE(warning.find("--tui"), std::string::npos) << warning;
    EXPECT_NE(warning.find("terminal"), std::string::npos) << warning;
    EXPECT_NE(warning.find("console"), std::string::npos)
        << "the warning says what happens instead, not only that something did not: " << warning;

    EXPECT_FALSE(TerminalDashboard::Offer(false, true, warning)) << "nobody asked for it";
    EXPECT_TRUE(warning.empty()) << "and saying nothing was wrong would be noise: " << warning;

    EXPECT_FALSE(TerminalDashboard::Offer(false, false, warning));
    EXPECT_TRUE(warning.empty());
}
