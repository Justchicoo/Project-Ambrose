/*
 * Project Ambrose by Imjustchico
 * What a shell window is bound to, worked out the same way on every desktop: the program's own origin when it shows its own page, or the one remote origin it was given, which must be plain HTTP only on loopback and HTTPS anywhere else; the address it opens first; and the profile folder its web view keeps data in. A build that found no web view says so plainly rather than opening nothing.
 */

#include "ShellWindow.h"

#include "EmbeddedPage.h"
#include "ShellProfiles.h"

std::optional<ShellOrigin> ShellWindow::BoundOrigin(ShellWindowOptions const& options, std::string& error)
{
    if ((options.Page == nullptr) == options.Remote.empty())
    {
        error = "a window shows either the program's own page or one remote panel";
        return std::nullopt;
    }
    if (options.Page != nullptr)
        return ShellOrigin::Own(options.Program);
    std::optional<ShellOrigin> const remote = ShellOrigin::Of(options.Remote);
    if (!remote || (remote->Scheme != "http" && remote->Scheme != "https"))
    {
        error = "a remote panel is opened by an http or https address, and " + options.Remote + " is neither";
        return std::nullopt;
    }
    if (remote->Scheme == "http" && remote->Host != "127.0.0.1" && remote->Host != "[::1]")
    {
        error = "a remote panel beyond this computer is opened only over https, and " + options.Remote + " is plain http";
        return std::nullopt;
    }
    return remote;
}

std::string ShellWindow::StartUrl(ShellWindowOptions const& options)
{
    if (options.Page != nullptr)
        return ShellOrigin::Own(options.Program).Describe() + options.StartPath;
    return options.Remote;
}

std::filesystem::path ShellWindow::ProfileFolder(ShellWindowOptions const& options)
{
    std::string ignored;
    std::optional<ShellOrigin> const bound = BoundOrigin(options, ignored);
    if (options.Page != nullptr || !bound)
        return ShellProfiles::OwnFolder(options.DataFolder);
    return ShellProfiles::FolderFor(options.DataFolder, *bound);
}

#if !defined(_WIN32) && !defined(AMBROSE_HAS_WEBKITGTK)

bool ShellWindow::Available()
{
    return false;
}

std::string ShellWindow::RuntimeVersion()
{
    return std::string();
}

bool ShellWindow::Show(ShellWindowOptions const&, std::string& error)
{
    error = "this build has no window of its own, because no web view was found when it was built";
    return false;
}

void ShellWindow::OpenInSystemBrowser(std::string const&)
{
}

#endif
