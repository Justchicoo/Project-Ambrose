/*
 * Project Ambrose by Imjustchico
 * The character group: what a game master changes about a wizard. Each command takes the name of a character and a value, and for now says what it would set and refuses politely, because the character it would reach lives in a world no session has entered yet; milestone 4.09 gives them the player to act on, and the table, the levels and the parsing they are reached through do not change when it does.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <optional>

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
        caller.Reply(fmt::format("Setting {} to {} needs a character in a world, which no session has entered yet", what, *value));
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
                return [what](CommandCaller& caller, std::vector<std::string> const& arguments) { return NotInAWorldYet(caller, what, arguments); };
            };
            return {
                { .Name = "character", .SecurityLevel = SEC_GAMEMASTER, .Help = "change a wizard", .Children = {
                    { .Name = "level", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's level", .Run = sets("level") },
                    { .Name = "gold", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a wizard's gold", .Run = sets("gold") },
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
