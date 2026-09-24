/*
 * Project Ambrose by Imjustchico
 * One command a CommandScript offers: the word it answers to, the level an account needs before it is even told the command exists, whether it may be run from a console as well as in game, its one-line help, whether its arguments are secret enough to keep out of a log, what it does, and the commands nested under it, so '.character gold 500' is the gold command inside the character group and the table is a tree rather than a list of strings to pull apart. Every field carries a default, so a table names the parts it cares about and a field added later breaks no table that was written before it.
 */

#ifndef AMBROSE_CHATCOMMAND_H
#define AMBROSE_CHATCOMMAND_H

#include "Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class CommandCaller;

struct ChatCommand
{
    using Handler = std::function<bool(CommandCaller& caller, std::vector<std::string> const& arguments)>;

    std::string Name = {};
    uint8 SecurityLevel = 0;
    bool AvailableInGame = true;
    bool AvailableOnConsole = true;
    std::string Help = {};
    bool Sensitive = false;
    Handler Run = {};
    std::vector<ChatCommand> Children = {};
};

#endif
