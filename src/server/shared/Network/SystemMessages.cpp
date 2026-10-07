/*
 * Project Ambrose by Imjustchico
 * Says which forced disconnects carry a ban's end and writes that end as the client's ban parser reads it: decimal Unix seconds no later than the client's 32-bit reading of them holds, and the latest of them for a ban with no end, since the ban dialog calls a ban permanent only when it ends at least five years out and reads the parser's forever, 630720000, as a date in 1989.
 */

#include "SystemMessages.h"
#include "DisconnectReason.h"

#include <fmt/format.h>

#include <algorithm>

bool SystemMessages::CarriesBanEnd(uint32 disconnectType) noexcept
{
    return disconnectType == DisconnectReason::Banned || disconnectType == DisconnectReason::AccountBanned || disconnectType == DisconnectReason::MachineBanned;
}

std::string SystemMessages::FormatBanEnd(uint64 unbanDate)
{
    return fmt::format("{}", unbanDate == 0 ? LatestBanEnd : std::min<uint64>(unbanDate, LatestBanEnd));
}
