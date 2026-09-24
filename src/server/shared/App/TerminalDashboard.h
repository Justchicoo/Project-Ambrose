/*
 * Project Ambrose by Imjustchico
 * The full-screen panels an operator over SSH watches instead of a scrolling console: what the server is, how long it has been up and what it is worried about, how many sessions it holds, the newest log lines, and the command line they type into. What to draw is a plain description filled by the app, and drawing it is a function of that description and the size of the terminal, so a layout can be rendered and read at any size without a terminal existing; only running the loop needs one. A key press is judged by the same rule whether it arrives from a real terminal or a test, so the way out of the dashboard is one decision rather than two. The loop itself takes where its panels, its size, its keys and its screen come from, so the whole of it runs against a test's own terminal: a size that changes between keys proves the panels are laid out again, and the key that leaves proves what the loop returns. The terminal loop hands its caller a way to end it, because a server told to shut down cannot wait for somebody to press a key, and a thread still running when the app is destroyed ends the process the hard way.
 */

#ifndef AMBROSE_TERMINALDASHBOARD_H
#define AMBROSE_TERMINALDASHBOARD_H

#include "Types.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct TerminalPanels
{
    std::string App;
    std::string Role;
    std::string Revision;
    std::string State;
    std::string Uptime;
    std::vector<std::pair<std::string, std::string>> Figures;
    std::vector<std::string> Problems;
    std::vector<std::string> Logs;
    std::string Typed;
    std::string Said;
};

enum class TerminalVerdict : uint8
{
    Ignore,
    Quit,
    Send,
    Typed,
    Rubbed
};

struct TerminalSources
{
    std::function<TerminalPanels()> Panels;
    std::function<std::pair<int, int>()> Size;
    std::function<std::optional<std::string>()> NextKey;
    std::function<void(std::string const&)> Show;
    std::function<void(std::string const&)> Send;
};

class TerminalDashboard
{
public:
    static constexpr int LeastWidth = 40;
    static constexpr int LeastHeight = 12;

    TerminalDashboard() = delete;

    static std::string Draw(TerminalPanels const& panels, int width, int height);
    static TerminalVerdict Judge(std::string const& key);
    static int Loop(TerminalSources const& sources);
    static bool Offer(bool asked, bool terminal, std::string& warning);
    static int RunInTerminal(std::function<TerminalPanels()> panels, std::function<void(std::string const&)> send,
        std::function<void(std::function<void()>)> giveExit = {});
};

#endif
