/*
 * Project Ambrose by Imjustchico
 * The window the launcher opens when it is asked for one: the desktop shell's window showing the launcher's own page, compiled into the program and served from memory on the launcher's own origin, with the launcher answering behind it. No listener is opened and the window asks nothing of the network; the only traffic a run makes is the client's own to the login server. Every message the page sends is handed to the same channel the tests drive and the answer is handed back, so the window decides nothing the terminal would decide differently. A machine with no web view is told so and left with the console launcher, because a launcher that will not start is worse than one without a window. The web view keeps its data in the launcher's own folder under the Ambrose data folder, and where the window was left is remembered in a file of its own beside the launcher's other data. A window opened for the first time gives its page 1024 by 640 inside the frame, the shape of a game launcher, which every screen is drawn to fit without scrolling; a size the user chose is kept over it. The one folder the window can ask to be opened is the one holding the client's log, handed to the system as a file URL the launcher wrote, never a path the page sent.
 */

#ifndef AMBROSE_LAUNCHERWINDOW_H
#define AMBROSE_LAUNCHERWINDOW_H

#include "ShellWindow.h"

#include <filesystem>
#include <functional>
#include <string>

class EmbeddedPage;

class LauncherWindow
{
public:
    static constexpr char const* Program = "launcher";
    static constexpr char const* DataFolderName = "Launcher";
    static constexpr int DefaultWidth = 1024;
    static constexpr int DefaultHeight = 640;

    using Answering = std::function<std::string(std::string const& message)>;

    LauncherWindow() = delete;

    static bool Available();
    static EmbeddedPage const& Page();
    static ShellWindowOptions Options(std::filesystem::path const& dataFolder, std::filesystem::path const& placeFile, Answering answer);
    static bool Show(std::filesystem::path const& dataFolder, std::filesystem::path const& placeFile, Answering answer, std::string& error);
    static void OpenFolder(std::filesystem::path const& folder);
};

#endif
