/*
 * Project Ambrose by Imjustchico
 * Opens the launcher's page through the desktop shell: the page compiled into the launcher, its web view data in the launcher's folder under the Ambrose data folder, and its place in the file the launcher names.
 */

#include "LauncherWindow.h"

#include "LauncherPage.h"

bool LauncherWindow::Available()
{
    return ShellWindow::Available();
}

EmbeddedPage const& LauncherWindow::Page()
{
    return LauncherPage();
}

ShellWindowOptions LauncherWindow::Options(std::filesystem::path const& dataFolder, std::filesystem::path const& placeFile, Answering answer)
{
    ShellWindowOptions options;
    options.Program = Program;
    options.Title = "Ambrose";
    options.Page = &Page();
    options.DataFolder = dataFolder / DataFolderName;
    options.PlaceFile = placeFile;
    options.Width = DefaultWidth;
    options.Height = DefaultHeight;
    options.Answer = std::move(answer);
    return options;
}

bool LauncherWindow::Show(std::filesystem::path const& dataFolder, std::filesystem::path const& placeFile, Answering answer, std::string& error)
{
    if (dataFolder.empty())
    {
        error = "no Ambrose data folder could be found for the window's web view";
        return false;
    }
    return ShellWindow::Show(Options(dataFolder, placeFile, std::move(answer)), error);
}
