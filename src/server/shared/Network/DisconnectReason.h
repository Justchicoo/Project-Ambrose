/*
 * Project Ambrose by Imjustchico
 * The reasons MSG_FORCE_DISCONNECT's Type gives, each the string hash of the name the client compares Type against to choose the dialog it shows: CSR for a game master's kick, Maintenance for a closing server, the one whose Message the client shows, Banned, AccountBanned and MachineBanned with the TimeStamp a ban lasts until, AISDisconnect, and User, which the client hashes but never tests. Any other Type gets the client's plain disconnect dialog.
 */

#ifndef AMBROSE_DISCONNECTREASON_H
#define AMBROSE_DISCONNECTREASON_H

#include "StringHash.h"
#include "Types.h"

namespace DisconnectReason
{
    inline constexpr uint32 Csr = StringHash::KiStringHash("CSR");
    inline constexpr uint32 Maintenance = StringHash::KiStringHash("Maintenance");
    inline constexpr uint32 Banned = StringHash::KiStringHash("Banned");
    inline constexpr uint32 AccountBanned = StringHash::KiStringHash("AccountBanned");
    inline constexpr uint32 MachineBanned = StringHash::KiStringHash("MachineBanned");
    inline constexpr uint32 AisDisconnect = StringHash::KiStringHash("AISDisconnect");
    inline constexpr uint32 User = StringHash::KiStringHash("User");
}

#endif
