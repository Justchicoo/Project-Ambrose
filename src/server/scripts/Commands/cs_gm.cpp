/*
 * Project Ambrose by Imjustchico
 * The gm group: whether a game master is acting as one. 'gm on' and 'gm off' are what a tester reaches for first and what every later command reads to decide whether it is allowed, and both are refused to a console, because a console has no character to be visible or invisible as.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ScriptMgr.h"

namespace
{
    class GameMasterCommands : public CommandScript
    {
    public:
        GameMasterCommands() : CommandScript("cs_gm") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "gm", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "act as a game master, or stop", .Children = {
                    { .Name = "on", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "act as a game master", .Run = Unavailable },
                    { .Name = "off", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "stop acting as a game master", .Run = Unavailable },
                } },
            };
        }

    private:
        static bool Unavailable(CommandCaller& caller, std::vector<std::string> const&)
        {
            caller.Reply("This needs a character in a world, which no session has entered yet");
            return true;
        }
    };
}

void AddSC_cs_gm()
{
    new GameMasterCommands();
}
