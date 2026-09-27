/*
 * Project Ambrose by Imjustchico
 * Matches a live log record's category against a subscription's category prefixes at dots, and holds the one compiled copy of the log's subscriber ring.
 */

#include "LogSubscription.h"

template class StreamSubscription<LogMessage, LogStreamFilter>;

bool LogStreamFilter::MatchesCategory(std::string_view category) const noexcept
{
    for (std::string const& prefix : Categories)
        if (Ambrose::Logging::IsCategoryWithin(category, prefix))
            return true;
    return false;
}
