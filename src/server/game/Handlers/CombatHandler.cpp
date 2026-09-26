/*
 * Project Ambrose by Imjustchico
 * Receives the first combat messages in the world as typed stubs, logging the raw move fields until combat behavior is built.
 */

#include "GameSession.h"
#include "Log.h"

void GameSession::HandleCombatMove(GameMessages::CombatMove& message)
{
    LOG_DEBUG("server.combat", "Session {} decoded MSG_COMBATMOVE (MoveType {}, SpellSelection {}, SpellTarget {}, TimeLeft {}, ShadowPactTarget {}, SelectedTieredSpellID {})",
        GetSessionId(), message.MoveType, message.SpellSelection, message.SpellTarget, message.TimeLeft, message.ShadowPactTarget, message.SelectedTieredSpellId);
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
