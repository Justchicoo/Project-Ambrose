/*
 * Project Ambrose by Imjustchico
 * Accessors for the git revision this build was compiled from.
 */

#ifndef AMBROSE_GITREVISION_H
#define AMBROSE_GITREVISION_H

#include <string>

namespace GitRevision
{
    char const* GetHash();
    char const* GetBranch();
    char const* GetDate();
    std::string GetFullVersion();
}

#endif
