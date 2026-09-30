/*
 * Project Ambrose by Imjustchico
 * The character group: what a game master changes about a wizard in the world. 'character gold <amount>' sets its gold, which the pouch its level gives caps, 'character heal' fills its health, 'character mana <amount>' sets its mana and 'character potion <charges>' its potion's charge, each followed by the wizard, by character id or name, or without one the caller's own wizard. The change runs on the world thread, where the wizard's stats are kept, its client is told at once and the stats are saved; a name two wizards share names neither, and the ids to choose between are listed instead. Level and experience still say what they would set and refuse, until the milestones that level a wizard give them a change to make.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "World.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    bool NotInAWorldYet(CommandCaller& caller, std::string what, std::vector<std::string> const& arguments)
    {
        if (arguments.empty())
        {
            caller.Reply(fmt::format("Give the {} to set, and the character to set it on", what));
            return false;
        }
        std::optional<int64> const value = Ambrose::StringTo<int64>(arguments.front());
        if (!value)
        {
            caller.Reply(fmt::format("{} is not a number", arguments.front()));
            return false;
        }
        caller.Reply(fmt::format("Setting {} to {} waits for the milestone that levels a wizard", what, *value));
        return true;
    }

    struct Target
    {
        std::shared_ptr<GameSession> Session;
        std::string Name;
    };

    std::optional<Target> FindWizard(CommandCaller& caller, std::vector<std::string> const& words)
    {
        std::string const wizard = words.empty() ? (caller.GetCharacterId() != 0 ? std::to_string(caller.GetCharacterId()) : std::string()) : fmt::format("{}", fmt::join(words, " "));
        if (wizard.empty())
        {
            caller.Reply("Name the wizard, by character id or name");
            return std::nullopt;
        }
        std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(wizard);
        if (found.empty())
        {
            caller.Reply(fmt::format("No wizard in the world has the character id or name {}", wizard));
            return std::nullopt;
        }
        if (found.size() > 1)
        {
            caller.Reply(fmt::format("{} wizards in the world are named {}; name one by its character id:", found.size(), wizard));
            for (std::shared_ptr<GameSession> const& session : found)
                caller.Reply(fmt::format("  {} on session {}", session->GetCharacterId(), session->GetSessionId()));
            return std::nullopt;
        }
        std::string name = found.front()->GetCharacterName();
        if (name.empty())
            name = fmt::format("wizard {}", found.front()->GetCharacterId());
        return Target{ found.front(), std::move(name) };
    }

    template<typename Result>
    std::optional<Result> RunOn(CommandCaller& caller, Target const& target, std::function<Result(GameSession&)> change)
    {
        auto const result = std::make_shared<std::optional<Result>>();
        if (!sWorld.RunFor(target.Session, [result, change = std::move(change)](GameSession& wizard) { *result = change(wizard); }, World::CommandTimeout))
        {
            caller.Reply(fmt::format("The world did not answer within {} s, so {} may yet change", World::CommandTimeout.count(), target.Name));
            return std::nullopt;
        }
        return *result;
    }

    std::optional<int64> Amount(CommandCaller& caller, std::string_view command, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const value = arguments.empty() ? std::nullopt : Ambrose::StringTo<int64>(arguments.front());
        if (!value || *value < 0)
        {
            caller.Reply(fmt::format("character {} takes an amount of zero or more, then the wizard by character id or name, or none for your own", command));
            return std::nullopt;
        }
        return value;
    }

    std::vector<std::string> Rest(std::vector<std::string> const& arguments)
    {
        return arguments.empty() ? std::vector<std::string>{} : std::vector<std::string>(arguments.begin() + 1, arguments.end());
    }

    bool Gold(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const amount = Amount(caller, "gold", arguments);
        std::optional<Target> const target = amount ? FindWizard(caller, Rest(arguments)) : std::nullopt;
        if (!target)
            return false;
        int64 const wanted = *amount;
        std::optional<std::optional<GoldChange>> const change = RunOn<std::optional<GoldChange>>(caller, *target, [wanted](GameSession& wizard) -> std::optional<GoldChange>
        {
            PlayerStats const* const stats = wizard.GetStats();
            return stats ? wizard.ModifyGold(wanted - stats->GetGold()) : std::nullopt;
        });
        if (!change)
            return false;
        if (!*change)
        {
            caller.Reply(fmt::format("{} has no stats to change yet", target->Name));
            return false;
        }
        if ((*change)->Overflow > 0)
            caller.Reply(fmt::format("{} now holds {} gold, a full pouch; {} did not fit", target->Name, (*change)->Gold, (*change)->Overflow));
        else
            caller.Reply(fmt::format("{} now holds {} gold", target->Name, (*change)->Gold));
        return true;
    }

    bool Heal(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<Target> const target = FindWizard(caller, arguments);
        if (!target)
            return false;
        std::optional<bool> const healed = RunOn<bool>(caller, *target, [](GameSession& wizard)
        {
            PlayerStats const* const stats = wizard.GetStats();
            return stats && wizard.SetHealth(stats->GetMaxHitpoints());
        });
        if (!healed)
            return false;
        caller.Reply(*healed ? fmt::format("{} is at full health", target->Name) : fmt::format("{} has no stats to change yet", target->Name));
        return *healed;
    }

    bool Mana(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const amount = Amount(caller, "mana", arguments);
        std::optional<Target> const target = amount ? FindWizard(caller, Rest(arguments)) : std::nullopt;
        if (!target)
            return false;
        int32 const wanted = static_cast<int32>(std::min<int64>(*amount, std::numeric_limits<int32>::max()));
        std::optional<std::optional<int32>> const mana = RunOn<std::optional<int32>>(caller, *target, [wanted](GameSession& wizard) -> std::optional<int32>
        {
            return wizard.SetMana(wanted) ? std::optional<int32>(wizard.GetStats()->GetMana()) : std::nullopt;
        });
        if (!mana)
            return false;
        caller.Reply(*mana ? fmt::format("{} now has {} mana", target->Name, **mana) : fmt::format("{} has no stats to change yet", target->Name));
        return mana->has_value();
    }

    bool Potion(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        std::optional<int64> const amount = Amount(caller, "potion", arguments);
        std::optional<Target> const target = amount ? FindWizard(caller, Rest(arguments)) : std::nullopt;
        if (!target)
            return false;
        float const wanted = static_cast<float>(std::min<int64>(*amount, 1000));
        std::optional<std::optional<float>> const charge = RunOn<std::optional<float>>(caller, *target, [wanted](GameSession& wizard) -> std::optional<float>
        {
            return wizard.SetPotionCharge(wanted) ? std::optional<float>(wizard.GetStats()->GetPotionCharge()) : std::nullopt;
        });
        if (!charge)
            return false;
        caller.Reply(*charge ? fmt::format("{}'s potion now holds {} charge(s)", target->Name, **charge) : fmt::format("{} has no stats to change yet", target->Name));
        return charge->has_value();
    }

    class CharacterCommands : public CommandScript
    {
    public:
        CharacterCommands() : CommandScript("cs_character") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            auto const sets = [](char const* what)
            {
                return [what](CommandCaller& caller, std::vector<std::string> const& arguments) { return NotInAWorldYet(caller, what, arguments); };
            };
            return {
                { .Name = "character", .SecurityLevel = SEC_GAMEMASTER, .Help = "change a wizard", .Children = {
                    { .Name = "level", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's level", .Run = sets("level") },
                    { .Name = "gold", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's gold, up to its pouch", .Run = Gold },
                    { .Name = "heal", .SecurityLevel = SEC_GAMEMASTER, .Help = "fill a wizard's health", .Run = Heal },
                    { .Name = "mana", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's mana", .Run = Mana },
                    { .Name = "potion", .SecurityLevel = SEC_GAMEMASTER, .Help = "set the charges a wizard's potion holds", .Run = Potion },
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
