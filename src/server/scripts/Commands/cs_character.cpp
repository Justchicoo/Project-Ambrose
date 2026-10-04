/*
 * Project Ambrose by Imjustchico
 * The character group: game-master commands for changing the wizard in the caller's live game session, including HUD updates for gold, mana, health and potion capacity.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>

namespace
{
    GameSession* InWorldSession(CommandCaller& caller)
    {
        GameSession* const session = caller.GetGameSession();
        if (!session || !session->GetStats())
        {
            caller.Reply("This command needs a wizard in the world.");
            return nullptr;
        }
        return session;
    }

    std::optional<int64> ReadValue(CommandCaller& caller, std::string_view what, std::vector<std::string> const& arguments)
    {
        if (arguments.size() != 1)
        {
            caller.Reply(fmt::format("Usage: .character {} <value>", what));
            return std::nullopt;
        }
        std::optional<int64> const value = Ambrose::StringTo<int64>(arguments.front());
        if (!value)
        {
            caller.Reply(fmt::format("{} is not a number", arguments.front()));
            return std::nullopt;
        }
        return value;
    }

    bool SetGold(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, "gold", arguments);
        if (!value)
            return false;
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        session->SetGold(*value);
        PlayerStats const* const stats = session->GetStats();
        caller.Reply(fmt::format("Gold is now {} of {}.", stats->GetGold(), stats->GetBase().Gold));
        return true;
    }

    bool SetMana(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, "mana", arguments);
        if (!value)
            return false;
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        session->SetMana(static_cast<int32>(std::clamp<int64>(*value, std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max())));
        PlayerStats const* const stats = session->GetStats();
        caller.Reply(fmt::format("Mana is now {} of {}.", stats->GetMana(), stats->GetMaxMana()));
        return true;
    }

    bool Heal(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        if (!arguments.empty())
        {
            caller.Reply("Usage: .character heal");
            return false;
        }
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        PlayerStats const* const stats = session->GetStats();
        session->SetHealth(stats->GetMaxHitpoints());
        caller.Reply(fmt::format("Health is now {} of {}.", stats->GetHitpoints(), stats->GetMaxHitpoints()));
        return true;
    }

    bool SetPotion(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, "potion", arguments);
        if (!value)
            return false;
        if (*value < 0 || *value > 1000)
        {
            caller.Reply("Potion capacity must be between 0 and 1000.");
            return false;
        }
        GameSession* const session = InWorldSession(caller);
        if (!session)
            return false;
        session->SetPotionCapacity(static_cast<uint32>(*value));
        PlayerStats const* const stats = session->GetStats();
        caller.Reply(fmt::format("Potion charges are now {} of {}.", stats->GetPotionCharge(), stats->GetPotionMax()));
        return true;
    }

    bool NotInWorldYet(CommandCaller& caller, std::string what, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = ReadValue(caller, what, arguments);
        if (!value)
            return false;
        if (!InWorldSession(caller))
            return false;
        caller.Reply(fmt::format("Setting {} to {} is not implemented yet", what, *value));
        return true;
    }

    class CharacterCommands : public CommandScript
    {
    public:
        CharacterCommands() : CommandScript("cs_character") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            auto const sets = [](char const* what)
            {
                return [what](CommandCaller& caller, std::vector<std::string> const& arguments) { return NotInWorldYet(caller, what, arguments); };
            };
            return {
                { .Name = "character", .SecurityLevel = SEC_GAMEMASTER, .Help = "change a wizard", .Children = {
                    { .Name = "level", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's level", .Run = sets("level") },
                    { .Name = "gold", .SecurityLevel = SEC_GAMEMASTER, .Help = "set the current wizard's gold", .Run = SetGold },
                    { .Name = "mana", .SecurityLevel = SEC_GAMEMASTER, .Help = "set the current wizard's mana", .Run = SetMana },
                    { .Name = "heal", .SecurityLevel = SEC_GAMEMASTER, .Help = "restore the current wizard to full health", .Run = Heal },
                    { .Name = "potion", .SecurityLevel = SEC_GAMEMASTER, .Help = "set the current wizard's potion capacity and fill it", .Run = SetPotion },
                    { .Name = "xp", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's experience", .Run = sets("experience") },
                } },
            };
        }
    };
}

void AddSC_cs_character()
{
    new CharacterCommands();
}
