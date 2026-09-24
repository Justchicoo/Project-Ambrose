/*
 * Project Ambrose by Imjustchico
 * Who is playing right now, for GET /api/players: every wizard a gameserver has let into a realm, with the account behind it, the realm and zone it is in, and how long it has been there, read from the row the handoff writes rather than from anything a client says about itself. A wizard whose row survives its character being deleted is still listed, named by its own id, because an operator needs to see a row that should not be there rather than have it quietly left out.
 */

#ifndef AMBROSE_ONLINEPLAYERSVIEW_H
#define AMBROSE_ONLINEPLAYERSVIEW_H

#include "Types.h"

#include <string>
#include <vector>

class AdminRouter;

struct OnlinePlayer
{
    uint64 CharacterGuid = 0;
    uint64 AccountId = 0;
    uint32 RealmId = 0;
    int64 SinceEpochSeconds = 0;
    std::string Account;
    std::string Realm;
    std::string Name;
    std::string Zone;
    uint32 Level = 0;
    uint32 SchoolId = 0;
    bool Found = false;
};

class OnlinePlayersView
{
public:
    static constexpr int SchemaVersion = 1;

    OnlinePlayersView() = delete;

    static std::vector<OnlinePlayer> Read();
    static std::string PlayersJson(std::vector<OnlinePlayer> const& players, int64 nowEpochSeconds);
    static void Register(AdminRouter& router);
};

#endif
