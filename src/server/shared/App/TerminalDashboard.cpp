/*
 * Project Ambrose by Imjustchico
 * Lays the panels out with FTXUI and renders them to text. The size is taken as given rather than asked of the terminal, so the same call draws an 80 by 24 window and a 200 by 60 one and a test can read either; a window smaller than anything can be read in is answered with one line saying so rather than with panels squeezed past legibility. The log panel holds the recent lines and is anchored to the end of them, so whatever height the layout leaves it, what shows is the newest, which is where an operator's eye already is on a console. Anchoring is left to the layout rather than worked out by counting rows, because a count that is two out does not crowd the panel, it silently drops the newest lines off the bottom, which is the opposite of what was asked for. Nothing here holds state: what is drawn is decided entirely by what it was handed.
 */

#include "TerminalDashboard.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <algorithm>

namespace
{
    using namespace ftxui;

    Element Pair(std::string const& name, std::string const& value)
    {
        return hbox({ text(name) | dim, text(" "), text(value) | bold });
    }

    Element StatusPanel(TerminalPanels const& panels)
    {
        Elements rows;
        rows.push_back(Pair("app", panels.App.empty() ? "unnamed" : panels.App));
        if (!panels.Role.empty())
            rows.push_back(Pair("role", panels.Role));
        rows.push_back(Pair("state", panels.State.empty() ? "unknown" : panels.State));
        rows.push_back(Pair("up", panels.Uptime.empty() ? "not running" : panels.Uptime));
        if (!panels.Revision.empty())
            rows.push_back(Pair("build", panels.Revision));
        return window(text(" Status "), vbox(std::move(rows)));
    }

    Element FiguresPanel(TerminalPanels const& panels)
    {
        Elements rows;
        for (auto const& [name, value] : panels.Figures)
            rows.push_back(Pair(name, value));
        if (rows.empty())
            rows.push_back(text("nothing published yet") | dim);
        return window(text(" Sessions "), vbox(std::move(rows)));
    }

    Element ProblemsPanel(TerminalPanels const& panels)
    {
        Elements rows;
        for (std::string const& problem : panels.Problems)
            rows.push_back(text(problem));
        if (rows.empty())
            rows.push_back(text("nothing to report") | dim);
        return window(text(" Problems "), vbox(std::move(rows)));
    }

    Element LogPanel(TerminalPanels const& panels)
    {
        constexpr std::size_t Keep = 500;
        Elements rows;
        std::size_t const from = panels.Logs.size() > Keep ? panels.Logs.size() - Keep : 0;
        for (std::size_t at = from; at < panels.Logs.size(); ++at)
            rows.push_back(text(panels.Logs[at]));
        if (rows.empty())
            rows.push_back(text("waiting for the first line") | dim);
        return window(text(" Log "), vbox(std::move(rows)) | focusPositionRelative(0, 1) | yframe);
    }

    Element CommandPanel(TerminalPanels const& panels)
    {
        Elements rows;
        rows.push_back(hbox({ text("Ambrose> ") | bold, text(panels.Typed) }));
        if (!panels.Said.empty())
            rows.push_back(text(panels.Said) | dim);
        return window(text(" Command "), vbox(std::move(rows)));
    }
}

namespace
{
    ftxui::Element Compose(TerminalPanels const& panels)
    {
        using namespace ftxui;
        Element const top = hbox({ StatusPanel(panels) | flex, FiguresPanel(panels) | flex, ProblemsPanel(panels) | flex });
        return vbox({ top, LogPanel(panels) | flex, CommandPanel(panels) });
    }
}

std::string TerminalDashboard::Draw(TerminalPanels const& panels, int width, int height)
{
    using namespace ftxui;
    if (width < LeastWidth || height < LeastHeight)
    {
        Screen small = Screen::Create(Dimension::Fixed(std::max(1, width)), Dimension::Fixed(std::max(1, height)));
        Element const note = text("Terminal too small for the dashboard");
        Render(small, note);
        return small.ToString();
    }

    Element page = Compose(panels);
    Screen screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, page);
    return screen.ToString();
}

TerminalVerdict TerminalDashboard::Judge(std::string const& key)
{
    if (key == "q" || key == "Q")
        return TerminalVerdict::Quit;
    if (key == "\x03" || key == "\x04")
        return TerminalVerdict::Quit;
    if (key == "\n" || key == "\r")
        return TerminalVerdict::Send;
    if (key == "\x7f" || key == "\b")
        return TerminalVerdict::Rubbed;
    if (key.size() == 1 && key[0] >= 0x20 && key[0] < 0x7f)
        return TerminalVerdict::Typed;
    return TerminalVerdict::Ignore;
}

int TerminalDashboard::Loop(TerminalSources const& sources)
{
    if (!sources.Panels || !sources.Size || !sources.NextKey || !sources.Show)
        return 1;

    std::string typed;
    std::string said;
    auto const paint = [&]
    {
        TerminalPanels panels = sources.Panels();
        panels.Typed = typed;
        if (panels.Said.empty())
            panels.Said = said;
        auto const [width, height] = sources.Size();
        sources.Show(Draw(panels, width, height));
    };

    paint();
    while (true)
    {
        std::optional<std::string> const key = sources.NextKey();
        if (!key)
            return 0;
        switch (Judge(*key))
        {
            case TerminalVerdict::Quit:
                return 0;
            case TerminalVerdict::Send:
                if (!typed.empty() && sources.Send)
                {
                    sources.Send(typed);
                    said = "ran " + typed;
                }
                typed.clear();
                break;
            case TerminalVerdict::Rubbed:
                if (!typed.empty())
                    typed.pop_back();
                break;
            case TerminalVerdict::Typed:
                typed += *key;
                break;
            case TerminalVerdict::Ignore:
                break;
        }
        paint();
    }
}

bool TerminalDashboard::Offer(bool asked, bool terminal, std::string& warning)
{
    warning.clear();
    if (!asked)
        return false;
    if (terminal)
        return true;
    warning = "--tui draws panels that need a terminal, and this output is not one, so the ordinary console is used instead";
    return false;
}

int TerminalDashboard::RunInTerminal(std::function<TerminalPanels()> panels, std::function<void(std::string const&)> send,
    std::function<void(std::function<void()>)> giveExit)
{
    using namespace ftxui;
    if (!panels)
        return 1;

    std::string typed;
    std::string said;
    ScreenInteractive screen = ScreenInteractive::Fullscreen();

    Component const page = Renderer([&]
    {
        TerminalPanels drawn = panels();
        drawn.Typed = typed;
        if (drawn.Said.empty())
            drawn.Said = said;
        return Compose(drawn);
    });

    Component const keys = CatchEvent(page, [&](Event event)
    {
        std::string const key = event.is_character() ? event.character() : event.input();
        switch (Judge(key))
        {
            case TerminalVerdict::Quit:
                screen.Exit();
                return true;
            case TerminalVerdict::Send:
                if (!typed.empty() && send)
                {
                    send(typed);
                    said = "ran " + typed;
                }
                typed.clear();
                return true;
            case TerminalVerdict::Rubbed:
                if (!typed.empty())
                    typed.pop_back();
                return true;
            case TerminalVerdict::Typed:
                typed += key;
                return true;
            case TerminalVerdict::Ignore:
                return false;
        }
        return false;
    });

    if (giveExit)
        giveExit([&screen] { screen.Exit(); });
    screen.Loop(keys);
    return 0;
}
