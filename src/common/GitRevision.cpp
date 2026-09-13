/*
 * Project Ambrose by Imjustchico
 * Returns the generated git revision values and formats the full version string.
 */

#include "GitRevision.h"
#include "RevisionData.h"

#include <fmt/format.h>

char const* GitRevision::GetHash()
{
    return AMBROSE_REVISION_HASH;
}

char const* GitRevision::GetBranch()
{
    return AMBROSE_REVISION_BRANCH;
}

char const* GitRevision::GetDate()
{
    return AMBROSE_REVISION_DATE;
}

std::string GitRevision::GetFullVersion()
{
    return fmt::format("Project Ambrose rev {} ({}) {}", GetHash(), GetBranch(), GetDate());
}
