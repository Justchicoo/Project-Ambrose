/*
 * Project Ambrose by Imjustchico
 * The realms a player may be sent to, for GET /api/realms: each realmlist row with where it is reached, how full it is against the limit its operator set, when it last said it was alive and whether that beat is recent enough to count, so the panel shows a realm that stopped beating as gone rather than waiting for somebody to mark it down. Editing a realm's limits and flags belongs to 17.31 and waits on the settings path it names.
 */

#ifndef AMBROSE_ADMINREALMSVIEW_H
#define AMBROSE_ADMINREALMSVIEW_H

#include "Types.h"

#include <string>

class AdminRouter;

class AdminRealmsView
{
public:
    static constexpr int SchemaVersion = 1;

    AdminRealmsView() = delete;

    static std::string RealmsJson(int64 nowEpochSeconds);
    static void Register(AdminRouter& router);
};

#endif
