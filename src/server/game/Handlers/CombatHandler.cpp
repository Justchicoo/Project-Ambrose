/*
 * Project Ambrose by Imjustchico
 * Receives the combat messages in the world: a wizard's move is put to the duel it is in, a pass or a change of mind taken and any other MoveType refused and logged once per value, and the rest are typed stubs until the milestones that answer them.
 */

#include "GameSession.h"
#include "DuelMgr.h"
#include "Log.h"

#include <mutex>
#include <set>

void GameSession::HandleCombatMove(GameMessages::CombatMove& message)
{
    LOG_DEBUG("server.combat", "Session {} decoded MSG_COMBATMOVE (MoveType {}, SpellSelection {}, SpellTarget {}, TimeLeft {}, ShadowPactTarget {}, SelectedTieredSpellID {})",
        GetSessionId(), message.MoveType, message.SpellSelection, message.SpellTarget, message.TimeLeft, message.ShadowPactTarget, message.SelectedTieredSpellId);
    std::optional<std::pair<uint64, DuelMoveResult>> const chosen = sDuelMgr.Choose(_worldGuid, message.MoveType);
    if (!chosen)
    {
        LOG_DEBUG("server.combat", "Session {}'s wizard {} sent MSG_COMBATMOVE outside a duel", GetSessionId(), _worldGuid);
        return;
    }
    auto const [duelId, result] = *chosen;
    if (result == DuelMoveResult::Unsupported)
    {
        static std::mutex refusedMutex;
        static std::set<uint8> refused;
        std::lock_guard const lock(refusedMutex);
        if (refused.insert(message.MoveType).second)
            LOG_WARN("server.combat", "Duel {} refused MoveType {} from wizard {}: this server takes only a pass (3) and a change of mind (4) so far", duelId, message.MoveType, _worldGuid);
        return;
    }
    if (result == DuelMoveResult::NotPlanning)
        LOG_DEBUG("server.combat", "Duel {} ignored wizard {}'s MoveType {} outside its planning phase", duelId, _worldGuid, message.MoveType);
}

void GameSession::HandleCombatDraw(GameMessages::CombatDraw&)
{
}

void GameSession::HandleCombatAFK(GameMessages::CombatAFK&)
{
}

void GameSession::HandleCombatVictory(GameMessages::CombatVictory&)
{
}

void GameSession::HandlePetWillCast(GameMessages::PetWillCast&)
{
}

void GameSession::HandleDismissSummon(GameMessages::DismissSummon&)
{
}

void GameSession::HandleCombatCheat(GameMessages::CombatCheat&)
{
}
