/*
 * Project Ambrose by Imjustchico
 * The one call that brings every script into the manager: CMake finds each AddSC function in the script folders and each module's own loader, writes the file that calls them all, and the app calls this once at startup and hands the manager what it loaded, so adding a script or a module is a new file and a rebuild rather than an edit to anything in the core.
 */

#ifndef AMBROSE_SCRIPTLOADER_H
#define AMBROSE_SCRIPTLOADER_H

void AddScripts();

#endif
