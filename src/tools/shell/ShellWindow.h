/*
 * Project Ambrose by Imjustchico
 * One window holding the operating system's own web view, which the launcher and the panel program both open. It shows either the program's own page, compiled into it and served from memory on the program's own secure origin with the host channel answering that origin alone, or a remote panel bound to one origin, with web messages off, every other origin and every new window sent to the system browser, downloads saved only through a dialog, and a certificate the web view cannot verify accepted only by its pin. Web view data lives in a profile of its own under the program's data folder, and where the window was left is remembered in a file there. A window whose page has not finished loading within its start timeout is closed and reported as not having come up, so a program falls back rather than waiting on a web view that never draws. A probe runs scripts in the page and closes the window, which is how the smoke test drives it off screen.
 */

#ifndef AMBROSE_SHELLWINDOW_H
#define AMBROSE_SHELLWINDOW_H

#include "ShellRules.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class EmbeddedPage;

struct ShellProbe
{
    std::vector<std::string> Scripts;
    std::chrono::milliseconds Gap{ 400 };
    std::chrono::milliseconds Timeout{ 60000 };
    std::function<void(std::vector<std::string> const& results)> Done;
};

struct ShellWindowOptions
{
    using Answering = std::function<std::string(std::string const& message)>;

    std::string Program;
    std::string Title = "Ambrose";
    EmbeddedPage const* Page = nullptr;
    std::string StartPath = "/index.html";
    std::string Remote;
    std::filesystem::path DataFolder;
    std::filesystem::path PlaceFile;
    int Width = 980;
    int Height = 720;
    Answering Answer;
    std::function<std::string(ShellOrigin const& origin)> PinFor;
    std::function<void(std::string const& url)> OpenExternal;
    std::function<std::filesystem::path(std::filesystem::path const& suggested)> SaveAs;
    std::function<void(std::string const& line)> Log;
    bool OffScreen = false;
    std::optional<ShellProbe> Probe;
    std::chrono::milliseconds StartTimeout{ 30000 };
};

class ShellWindow
{
public:
    ShellWindow() = delete;

    static bool Available();
    static bool Show(ShellWindowOptions const& options, std::string& error);
    static void OpenInSystemBrowser(std::string const& url);

    static std::optional<ShellOrigin> BoundOrigin(ShellWindowOptions const& options, std::string& error);
    static std::string StartUrl(ShellWindowOptions const& options);
    static std::filesystem::path ProfileFolder(ShellWindowOptions const& options);
};

#endif
