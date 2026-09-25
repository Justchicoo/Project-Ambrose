/*
 * Project Ambrose by Imjustchico
 * The window the launcher opens when it is asked for one: the operating system's own web view showing the page that ships beside the program, with the launcher answering behind it. The page is served from a folder mapped into the view rather than from a listener, so the launcher opens no port and the window asks nothing of the network; the only traffic a run makes is the client's own to the login server. Every message the page sends is handed to the same channel the tests drive and the answer is handed back, so the window decides nothing the terminal would decide differently. A machine with no web view is told so and left with the console launcher, because a launcher that will not start is worse than one without a window. Where the window was left is remembered in a file of its own beside the launcher's other data and given back the next time, unless the place it names is one no screen holds any more, in which case the window opens where a window that had never run would.
 */

#ifndef AMBROSE_LAUNCHERWINDOW_H
#define AMBROSE_LAUNCHERWINDOW_H

#include <filesystem>
#include <functional>
#include <string>

class LauncherWindow
{
public:
    static constexpr char const* VirtualHost = "launcher.ambrose";
    static constexpr int DefaultWidth = 980;
    static constexpr int DefaultHeight = 720;

    using Answering = std::function<std::string(std::string const& message)>;

    LauncherWindow() = delete;

    static bool Available();
    static bool Show(std::filesystem::path const& page, std::filesystem::path const& placeFile, Answering answer, std::string& error);
};

#endif
