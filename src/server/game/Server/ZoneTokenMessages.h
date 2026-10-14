/*
 * Project Ambrose by Imjustchico
 * The messages that show one wizard its zone's tokens: MSG_AddZoneToken handing its client a token in the state the server keeps, MSG_ModifyZoneToken moving one by a delta and MSG_RemoveZoneToken taking one away by name.
 */

#ifndef AMBROSE_ZONETOKENMESSAGES_H
#define AMBROSE_ZONETOKENMESSAGES_H

#include "GameMessages.h"
#include "ZoneTokens.h"

#include <optional>
#include <string>

namespace ZoneTokenMessages
{
    std::optional<GameMessages::AddZoneToken> Add(uint64 player, ZoneTokenDefinition const& token, ZoneTokenState const& state, std::string& problem);
    GameMessages::ModifyZoneToken Modify(uint64 player, ZoneTokenChange const& change);
    GameMessages::RemoveZoneToken Remove(uint64 player, ZoneTokenChange const& change);
}

#endif
