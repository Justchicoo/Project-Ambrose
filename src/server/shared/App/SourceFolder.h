/*
 * Project Ambrose by Imjustchico
 * Finds the folder that holds the project's shipped data: the folder this build was made from while it is there, otherwise share/ambrose beside the installed executables, so the database updater and the supervisor's own store look for their SQL in the same place.
 */

#ifndef AMBROSE_SOURCEFOLDER_H
#define AMBROSE_SOURCEFOLDER_H

#include <filesystem>

namespace Ambrose
{
    std::filesystem::path FindSourceFolder();
}

#endif
