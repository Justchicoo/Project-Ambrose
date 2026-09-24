/*
 * Project Ambrose by Imjustchico
 * The game server's message table: which messages it handles, which it does not handle yet, and which only the server sends.
 */

#ifndef AMBROSE_GAMEMESSAGETABLE_H
#define AMBROSE_GAMEMESSAGETABLE_H

#include "GameSession.h"
#include "MessageHandlerTable.h"

namespace GameMessageTable
{
    MessageHandlerTable<GameSession> const& Get();
}

#endif
