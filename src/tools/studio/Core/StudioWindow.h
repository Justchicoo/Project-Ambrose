/*
 * Project Ambrose by Imjustchico
 * The window the Studio opens: the desktop shell's window showing the Studio's own page, compiled into the program and served from memory on its own origin, with every message the page sends answered by the Studio's channel. Its web view data and remembered place live in the Studio's folder under the Ambrose data folder, and a first window gives the page 1440 by 900, room for a list beside an editor.
 */

#ifndef AMBROSE_STUDIOWINDOW_H
#define AMBROSE_STUDIOWINDOW_H

#include "ShellWindow.h"

#include <filesystem>
#include <functional>
#include <string>

class StudioWindow
{
public:
    static constexpr char const* Program = "studio";
    static constexpr char const* DataFolderName = "Studio";
    static constexpr int DefaultWidth = 1440;
    static constexpr int DefaultHeight = 900;

    using Answering = std::function<std::string(std::string const& message)>;

    StudioWindow() = delete;

    static bool Available();
    static bool Show(std::filesystem::path const& dataFolder, Answering answer, std::string& error);
};

#endif
