/*
 * Project Ambrose by Imjustchico
 * The server group: what an operator asks a running server about itself. 'server info' is the one a console reaches for first, saying which build is running, how long it has been up, how many times the world has ticked and how many sessions it holds, and it is open to any level because none of it is a secret to somebody already signed in.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "Duration.h"
#include "GitRevision.h"
#include "ScriptMgr.h"
#include "World.h"

#include <fmt/format.h>

#include <chrono>

namespace
{
    class ServerCommands : public CommandScript
    {
    public:
        ServerCommands() : CommandScript("cs_server") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "server", .SecurityLevel = SEC_PLAYER, .Help = "what this server is and what it is doing", .Children = {
                    { .Name = "info", .SecurityLevel = SEC_PLAYER, .Help = "the build, the uptime, the ticks and the sessions", .Run = Info },
                } },
            };
        }

    private:
        static bool Info(CommandCaller& caller, std::vector<std::string> const&)
        {
            caller.Reply(fmt::format("Project Ambrose {} on {}", GitRevision::GetHash(), GitRevision::GetBranch()));
            caller.Reply(fmt::format("The world has ticked {} time(s) and holds {} session(s)", sWorld.GetTickCount(), sWorld.GetSessionCount()));
            return true;
        }
    };
}

void AddSC_cs_server()
{
    new ServerCommands();
}
