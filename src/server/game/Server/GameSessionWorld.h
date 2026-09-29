/*
 * Project Ambrose by Imjustchico
 * The world services a game session needs without coupling the session library to the world implementation.
 */

#ifndef AMBROSE_GAMESESSIONWORLD_H
#define AMBROSE_GAMESESSIONWORLD_H

#include "Types.h"

#include <memory>

class GameSession;

class GameSessionWorld
{
public:
    virtual ~GameSessionWorld() = default;

    virtual void RemoveSession(GameSession const* session) = 0;
    virtual std::shared_ptr<GameSession> FindSessionByCharacterId(uint64 characterId, GameSession const* except) const = 0;
};

#endif
