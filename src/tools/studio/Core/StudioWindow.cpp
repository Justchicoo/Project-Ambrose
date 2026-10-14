/*
 * Project Ambrose by Imjustchico
 * Opens the Studio's page through the desktop shell with its data and place in the Studio's folder under the Ambrose data folder.
 */

#include "StudioWindow.h"

#include "StudioPage.h"

bool StudioWindow::Available()
{
    return ShellWindow::Available();
}

bool StudioWindow::Show(std::filesystem::path const& dataFolder, Answering answer, std::string& error)
{
    if (dataFolder.empty())
    {
        error = "no Ambrose data folder could be found for the window's web view";
        return false;
    }
    ShellWindowOptions options;
    options.Program = Program;
    options.Title = "Ambrose Studio";
    options.Page = &StudioPage();
    options.DataFolder = dataFolder / DataFolderName;
    options.PlaceFile = dataFolder / DataFolderName / "studio-window.json";
    options.Width = DefaultWidth;
    options.Height = DefaultHeight;
    options.Answer = std::move(answer);
    return ShellWindow::Show(options, error);
}
