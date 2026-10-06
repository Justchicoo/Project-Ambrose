/*
 * Project Ambrose by Imjustchico
 * Says which forced disconnects carry a ban's end and writes that end as the client's ban parser reads it: decimal Unix seconds no later than the client's 32-bit reading of them holds, or forever for a ban with no end.
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
    return unbanDate == 0 ? std::string(PermanentBanTimeStamp) : fmt::format("{}", std::min<uint64>(unbanDate, LatestBanEnd));
}
